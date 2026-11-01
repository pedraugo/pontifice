#include "scenes/battle/BattleScene.h"

#include <cmath>
#include <string>
#include <utility>

#include <random>

#include "scripts/cardface/CardFace.h"
#include "scripts/colors/Colors.h"
#include "scripts/image/Image.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

}  // namespace

namespace {

// A arena é um prisma trapezoidal: topo 1,9 x 1,9 (tabuleiro), base 2,5 x 2,5, altura 0,36 unidades no
// modelo (um ladrilho); escalada para caber na câmera.
constexpr float kArenaScale = 4.2f;
// Alturas da arena no mundo (bounds do glb y[-0.061, 0.3] vezes a escala).
constexpr float kArenaBaseY = -0.061f * kArenaScale;
constexpr float kArenaTopY = 0.3f * kArenaScale;

// Camadas de névoa na base da arena: altura e opacidade de cada uma.
struct MistLayer {
  float y;
  float alpha;
};
constexpr MistLayer kMistLayers[] = {{kArenaBaseY + 0.05f, 0.8f},
                                     {kArenaBaseY + 0.45f, 0.5f}};
constexpr float kMistHalfSize = 40.0f;

// Tochas fincadas no chão de cada lado da arena; são a única luz da arena.
constexpr float kTorchX = 6.4f;
constexpr float kTorchZ = -1.0f;
constexpr float kTorchScale = 2.6f;
constexpr float kTorchTilt = 0.10f;   // rad, levemente inclinadas para fora
constexpr float kTorchLight = 2.6f;   // intensidade de cada chama
constexpr float kShadowFov = 130.0f;  // graus: da chama, cobre a arena inteira

// Abertura da batalha: a arena sobe de dentro de uma névoa densa enquanto a câmera se
// aproxima. A névoa extra fica em alturas fixas do mundo, por isso a arena "fura" camada por
// camada ao subir, e vai rareando até restarem só as camadas da base.
constexpr float kIntroSeconds = 1.6f;
constexpr float kIntroDrop = 3.2f;                     // quanto a arena começa abaixo do lugar
// Tochas emergem depois da arena: começam a subir quando ela já está quase no lugar.
constexpr float kTorchIntroStart = 1.1f;   // s depois da entrada
constexpr float kTorchIntroSeconds = 1.2f;
constexpr float kTorchIntroDrop = 4.5f;    // começam bem abaixo do chão
constexpr float kIgniteSeconds = 0.6f;     // o fogo acende depois de a tocha ter surgido
constexpr math::Vec3 kIntroEyeStart{0.0f, 16.0f, 22.0f};  // câmera longe e alta
constexpr math::Vec3 kArenaTarget{0.0f, -0.1f, -0.3f};
struct IntroMistLayer {
  float y;
  float alpha;
};
constexpr IntroMistLayer kIntroMistLayers[] = {
    {kArenaBaseY + 0.9f, 0.9f}, {kArenaBaseY + 1.3f, 0.8f}, {kArenaTopY - 0.3f, 0.7f},
    {kArenaTopY + 0.2f, 0.6f},  {kArenaTopY + 0.8f, 0.45f},
};

float SmoothStep(float t) {
  t = math::Clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

BattleScene::~BattleScene() {
  if (field_front_2d_ != nullptr) SDL_DestroyTexture(field_front_2d_);
  if (gpu_ == nullptr) return;
  hand_.Clear();
  card_model_.reset();
  arena_model_.reset();
  torch_.reset();
  gpu_->DestroyMesh(mist_quad_);
  for (SDL_GPUTexture* t : textures_) gpu_->DestroyTexture(t);
  textures_.clear();
}

void BattleScene::Preload(const PreloadInfo& info) {
  preloaded_ = true;
  const std::string base = BasePath();
  // Frente da carta de campo para o HUD: aparece com ~120 px de altura, então vai reduzida 4x
  // (400x557); vira SDL_Texture em OnEnter, na thread principal.
  hud_field_ = Image::LoadFile(base + "sprites/CardFrontField.png").Downscaled(4);
  if (info.gpu_device == nullptr) return;

  gpu_ = std::make_unique<Gpu>();
  if (!gpu_->InitDevice(info.gpu_device, info.logical_width, info.logical_height,
                        base + "shaders")) {
    gpu_.reset();
    return;
  }
  card_model_ = std::make_unique<Model>();
  if (!card_model_->Upload(*gpu_, Model::Parse(base + "models/Card.glb"))) {
    SDL_Log("Falha ao carregar models/Card.glb");
    card_model_.reset();
    return;
  }
  back_ = UploadSprite(Image::LoadFile(base + "sprites/CardBack.png"));
  front_normal_ = UploadSprite(Image::LoadFile(base + "sprites/CardFrontNormalMap.png"));
  back_normal_ = UploadSprite(Image::LoadFile(base + "sprites/CardBackNormalMap.png"));
  // Molduras reduzidas 2x (800x1115): a carta na mão é pequena e a frente de cada carta é
  // composta a partir delas com os textos (cardface).
  frames_[0] = Image::LoadFile(base + "sprites/CardFrontGood.png").Downscaled(2);
  frames_[1] = Image::LoadFile(base + "sprites/CardFrontEvil.png").Downscaled(2);
  frames_[2] = Image::LoadFile(base + "sprites/CardFrontField.png").Downscaled(2);

  // Monte: 40 cartas sorteadas por peso e embaralhadas (deck::Deck) ANTES de qualquer carta
  // aparecer; as 5 da mão inicial são compradas e compostas já aqui.
  cards_ = cards::LoadAll(base);
  std::mt19937 rng{std::random_device{}()};
  deck_ = deck::Deck(cards::BuildDeck(cards_, Hud::kDeckSize, rng));
  deck_.Shuffle();
  // Depuração: PONTIFICE_HAND=G101,E101,... força os códigos da mão inicial (o monte continua
  // embaralhado; essas cartas não saem dele).
  std::vector<std::string> forced;
  if (const char* env = SDL_getenv("PONTIFICE_HAND")) {
    std::string spec = env;
    size_t pos = 0;
    while (pos <= spec.size()) {
      size_t end = spec.find(',', pos);
      if (end == std::string::npos) end = spec.size();
      if (end > pos) forced.push_back(spec.substr(pos, end - pos));
      pos = end + 1;
    }
  }
  for (int i = 0; i < 5; ++i) {
    std::optional<std::string> code;
    if (!forced.empty()) {
      if (i < static_cast<int>(forced.size())) code = forced[i];
    } else {
      code = deck_.Draw();
    }
    if (!code) break;
    const Face face = MakeCardFace(*code);
    if (face.texture != nullptr) initial_hand_.push_back(face);
  }
  arena_model_ = std::make_unique<Model>();
  if (!arena_model_->Upload(*gpu_, Model::Parse(base + "models/Arena.glb"))) {
    SDL_Log("Arena.glb nao carregado; gere com tools/texture_arena.py");
    arena_model_.reset();
  }
  torch_ = std::make_unique<Torch>();
  if (!torch_->Upload(*gpu_, Model::Parse(base + "models/Torch.glb"))) {
    SDL_Log("Torch.glb nao carregado; gere com tools/make_torch.py");
    torch_.reset();
  }
  // Quad horizontal em y = 0; cada camada de névoa o desenha transladado para sua altura.
  const float h = kMistHalfSize;
  mist_quad_ = gpu_->CreateMesh(
      {{-h, 0, -h, 0, 0}, {h, 0, -h, 1, 0}, {h, 0, h, 1, 1}, {-h, 0, h, 0, 1}},
      {0, 1, 2, 0, 2, 3});
}

SDL_GPUTexture* BattleScene::UploadSprite(const Image& image) {
  SDL_GPUTexture* texture = gpu_->CreateTexture(image);
  if (texture != nullptr) textures_.push_back(texture);
  return texture;
}

SDL_Texture* BattleScene::UploadHudSprite(SDL_Renderer* renderer, const Image& image) {
  if (!image.Valid()) return nullptr;
  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STATIC, image.width, image.height);
  if (texture == nullptr) return nullptr;
  SDL_UpdateTexture(texture, nullptr, image.rgba.data(), image.width * 4);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
  return texture;
}

void BattleScene::ToggleSampleFieldCard() {
  if (hud_.HasFieldCard()) {
    hud_.ClearFieldCard();
    return;
  }
  // Primeira carta de assets/CardsField.json; o conteúdo real virá do slot de campo.
  FieldCardInfo card;
  card.name = "Dilúvio";
  card.favored = Category::kEvil;
  card.buff = 300;
  card.nerf = 300;
  card.image = field_front_2d_;
  hud_.SetFieldCard(card);
}

BattleScene::Face BattleScene::MakeCardFace(const std::string& code) {
  const CardInfo* card = cards::Find(cards_, code);
  if (card == nullptr || gpu_ == nullptr) return {};
  const Image& frame = card->category == 'G' ? frames_[0]
                       : card->category == 'E' ? frames_[1]
                                               : frames_[2];
  // Hierarquia 1 (Divindades, Soberanos): carta foil.
  return {UploadSprite(cardface::Compose(frame, *card)), card->hierarchy == 1};
}

void BattleScene::DrawCard() {
  const std::optional<std::string> code = deck_.Draw();
  if (!code) return;
  hud_.SetDeck(Hud::kDeckSize - static_cast<int>(deck_.Size()), static_cast<int>(deck_.Size()));
  const Face face = MakeCardFace(*code);
  if (face.texture != nullptr) hand_.Add(face.texture, face.foil);
}

void BattleScene::OnEnter(SceneContext& ctx) {
  // Sem Preload antes (entrada direta na batalha), carrega tudo aqui mesmo.
  if (!preloaded_) Preload(ctx.Preload());

  // HUD (docs/hud.md): vida cheia, sem carta de campo, cemitério vazio e monte com a mão
  // inicial já comprada. Não depende do render 3D.
  hud_.SetLife(Hud::kMaxLife, Hud::kMaxLife);
  hud_.ClearFieldCard();
  hud_.SetGraveyard(0);
  // Fundo = cor da névoa: o que some na distância se funde com ele (assets/colors.json).
  fog_color_ = colors::GetF("background.fog");
  mist_color_ = colors::GetF("background.mist");
  if (field_front_2d_ == nullptr) {
    field_front_2d_ = UploadHudSprite(ctx.renderer, hud_field_);
    hud_field_ = Image{};
  }

  if (gpu_ == nullptr || card_model_ == nullptr) {
    SDL_Log("Falha ao iniciar o render 3D");
    return;
  }
  gpu_->SetRenderer(ctx.renderer);

  hand_.SetModel(card_model_.get());
  hand_.SetBack(back_);
  hand_.SetNormalMaps(front_normal_, back_normal_);
  // Mão inicial: as 5 cartas compradas do monte embaralhado no Preload (ver docs/batalha.md).
  // Entram pela esquerda, uma a uma, depois de o fade in da entrada terminar.
  hand_.DelayDeal(0.8f);
  for (const Face& face : initial_hand_) hand_.Add(face.texture, face.foil);
  initial_hand_.clear();
  hud_.SetDeck(Hud::kDeckSize - static_cast<int>(deck_.Size()), static_cast<int>(deck_.Size()));
  arena_world_ = math::Mat4::Scale(kArenaScale);

  const float aspect = static_cast<float>(ctx.logical_width) / ctx.logical_height;
  proj_ = math::Mat4::Perspective(math::Radians(35.0f), aspect, 0.1f, 100.0f);
  hand_view_proj_ = proj_ * math::Mat4::LookAt({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
  // Vista do jogador: acima e à frente do seu lado da arena, olhando para o centro.
  math::Vec3 eye{0, 10.5f, 12.2f};
  if (const char* env = SDL_getenv("PONTIFICE_ARENA_EYE")) {  // depuração: "x,y,z"
    SDL_sscanf(env, "%f,%f,%f", &eye.x, &eye.y, &eye.z);
  }
  eye_final_ = eye;
  arena_view_proj_ = proj_ * math::Mat4::LookAt(eye, kArenaTarget, {0, 1, 0});

  // Clima medieval: a luz da arena vem só das duas tochas (luzes pontuais nas chamas, cor
  // quente, com tremular); sem luz direcional, ambiente frio e fraco, névoa na base e vinheta.
  arena_atmosphere_.light_dir = {-0.35f, 1.0f, 0.45f};
  arena_atmosphere_.light_intensity = 0.0f;
  arena_atmosphere_.light_color = {1.0f, 0.74f, 0.46f, 1.0f};
  arena_atmosphere_.ambient = {0.07f, 0.08f, 0.13f, 1.0f};
  arena_atmosphere_.point_intensity = 0.0f;  // acendem quando o fogo surge nas tochas
  arena_atmosphere_.point2_intensity = 0.0f;
  arena_atmosphere_.fog_color = fog_color_;
  arena_atmosphere_.fog_density = 0.05f;
  arena_atmosphere_.fog_start = 15.0f;
  arena_atmosphere_.fog_full_y = kArenaBaseY;
  arena_atmosphere_.fog_clear_y = kArenaTopY - 0.2f;
  arena_atmosphere_.fog_height_strength = 0.9f;
  arena_atmosphere_.camera_pos = eye;
  arena_atmosphere_.vignette = 0.55f;
  arena_atmosphere_.shadow_strength = 0.85f;

  // Mão: mesma tocha amarela (luz principal e pontual, com o mesmo tremular), ambiente um
  // pouco mais claro para as cartas ficarem legíveis. Sem névoa nem vinheta.
  hand_atmosphere_.light_dir = {0.3f, 0.6f, 1.0f};
  hand_atmosphere_.light_intensity = 0.5f;
  hand_atmosphere_.light_color = arena_atmosphere_.light_color;
  hand_atmosphere_.ambient = {0.34f, 0.36f, 0.46f, 1.0f};
  hand_atmosphere_.point_pos = {0.0f, 2.5f, 4.0f};  // acima e à frente da mão
  hand_atmosphere_.point_intensity = 1.0f;
  hand_atmosphere_.camera_pos = {0, 0, 10};
  ready_ = true;
  UpdateIntro(0.0f);  // primeiro quadro já com a arena afundada e a câmera longe
}

void BattleScene::UpdateIntro(float dt_seconds) {
  if (intro_ >= 1.0f && torch_intro_ >= 1.0f && ignite_ >= 1.0f) return;
  intro_time_ += dt_seconds;
  intro_ = SmoothStep(intro_time_ / kIntroSeconds);
  torch_intro_ = SmoothStep((intro_time_ - kTorchIntroStart) / kTorchIntroSeconds);
  ignite_ = SmoothStep((intro_time_ - kTorchIntroStart - kTorchIntroSeconds) / kIgniteSeconds);
  // Depuração: PONTIFICE_ARENA_EYE fixa a câmera, então pula a abertura.
  if (SDL_getenv("PONTIFICE_ARENA_EYE") != nullptr) intro_ = torch_intro_ = ignite_ = 1.0f;
  const float drop = kIntroDrop * (1.0f - intro_);
  arena_world_ = math::Mat4::Translate({0.0f, -drop, 0.0f}) * math::Mat4::Scale(kArenaScale);
  // Tochas emergem do chão depois da arena; o fogo acende depois, e a luz vem com ele.
  const float torch_drop = kTorchIntroDrop * (1.0f - torch_intro_);
  torch_world_[0] = math::Mat4::Translate({-kTorchX, kArenaBaseY - torch_drop, kTorchZ}) *
                    math::Mat4::RotateZ(kTorchTilt) * math::Mat4::Scale(kTorchScale);
  torch_world_[1] = math::Mat4::Translate({kTorchX, kArenaBaseY - torch_drop, kTorchZ}) *
                    math::Mat4::RotateZ(-kTorchTilt) * math::Mat4::Scale(kTorchScale);
  arena_atmosphere_.point_pos = Torch::FlamePosition(torch_world_[0]);
  arena_atmosphere_.point2_pos = Torch::FlamePosition(torch_world_[1]);
  // Sombras: cada chama olha para o centro da arena com campo bem aberto.
  const math::Mat4 shadow_proj =
      math::Mat4::Perspective(math::Radians(kShadowFov), 1.0f, 0.3f, 60.0f);
  for (int k = 0; k < 2; ++k) {
    const math::Vec3 flame = k == 0 ? arena_atmosphere_.point_pos : arena_atmosphere_.point2_pos;
    torch_light_vp_[k] = shadow_proj * math::Mat4::LookAt(flame, {0.0f, 0.3f, 0.0f}, {0, 1, 0});
  }
  arena_atmosphere_.point_intensity = kTorchLight * ignite_;
  arena_atmosphere_.point2_intensity = kTorchLight * ignite_;
  const math::Vec3 eye = kIntroEyeStart + (eye_final_ - kIntroEyeStart) * intro_;
  arena_view_proj_ = proj_ * math::Mat4::LookAt(eye, kArenaTarget, {0, 1, 0});
  arena_atmosphere_.camera_pos = eye;
}

void BattleScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
        if (hand_.Inspecting()) {  // primeiro sai da inspeção; só depois volta ao menu
          hand_.StopInspect();
          return;
        }
        ctx.next_scene = SceneId::kMenu;
        return;
      case SDLK_SPACE:  // vira a carta inspecionada
        hand_.Flip();
        return;
      case SDLK_F:  // depuração: carta de campo de exemplo
        ToggleSampleFieldCard();
        return;
      case SDLK_1:  // depuração: dano no oponente
        if (!hud_.Over()) hud_.SetLife(hud_.OpponentLife() - 700, hud_.PlayerLife());
        return;
      case SDLK_2:  // depuração: dano em você
        if (!hud_.Over()) hud_.SetLife(hud_.OpponentLife(), hud_.PlayerLife() - 700);
        return;
      case SDLK_D:  // depuração: compra do monte
        DrawCard();
        return;
      case SDLK_G:  // depuração: carta para o cemitério
        hud_.SetGraveyard(hud_.Graveyard() + 1);
        return;
      default:
        break;
    }
  }
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    mouse_ = {event.motion.x, event.motion.y};
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    hand_.MouseDown({event.button.x, event.button.y}, event.button.button == SDL_BUTTON_LEFT);
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
    hand_.MouseUp({event.button.x, event.button.y});
  }
  if (event.type == SDL_EVENT_MOUSE_WHEEL) {
    hand_.Wheel(event.wheel.y);
  }
  if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
    mouse_ = {-1, -1};
  }
}

void BattleScene::Update(SceneContext& ctx, float dt_seconds) {
  if (!ready_) return;
  UpdateIntro(dt_seconds);
  hand_.Update(dt_seconds, mouse_, hand_view_proj_, ctx.logical_width, ctx.logical_height);
}

void BattleScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color fg = colors::Get("text.primary");
  const SDL_Color background = colors::Get("background.fog");
  const float cx = ctx.logical_width / 2.0f;

  if (ready_) {
    // Passes de sombra (um por tocha): arena e corpos das tochas são os ocultadores.
    for (int k = 0; k < 2; ++k) {
      if (!gpu_->BeginShadowPass(k, torch_light_vp_[k])) break;
      if (arena_model_ != nullptr) arena_model_->DrawDepth(*gpu_, arena_world_);
      if (torch_ != nullptr) {
        for (const math::Mat4& world : torch_world_) torch_->DrawBodyDepth(*gpu_, world);
      }
      gpu_->EndShadowPass();
    }
  }
  if (ready_ && gpu_->BeginFrame(fog_color_)) {
    gpu_->SetAtmosphere(arena_atmosphere_);
    if (arena_model_ != nullptr) {
      arena_model_->Draw(*gpu_, arena_world_, arena_view_proj_, SDL_FColor{1, 1, 1, 1});
    }
    if (torch_ != nullptr) {
      for (const math::Mat4& world : torch_world_) {
        torch_->DrawBody(*gpu_, world, arena_view_proj_);
      }
    }
    // Névoa depois da arena (transparente): de baixo para cima.
    for (const MistLayer& layer : kMistLayers) {
      gpu_->Draw(mist_quad_, nullptr, math::Mat4::Translate({0, layer.y, 0}), arena_view_proj_,
                 SDL_FColor{mist_color_.r, mist_color_.g, mist_color_.b, layer.alpha},
                 Gpu::DrawMode::kMist);
    }
    // Abertura: névoa densa acima da base, que se dissipa conforme a arena emerge.
    if (intro_ < 1.0f) {
      const float remain = 1.0f - intro_;
      Atmosphere dense = arena_atmosphere_;
      dense.mist_density = 0.3f * remain;      // manto fechado no início, fiapos no fim
      dense.mist_fade_start = 10.0f;
      dense.mist_fade_end = 30.0f;
      gpu_->SetAtmosphere(dense);
      for (const IntroMistLayer& layer : kIntroMistLayers) {
        gpu_->Draw(mist_quad_, nullptr, math::Mat4::Translate({0, layer.y, 0}),
                   arena_view_proj_,
                   SDL_FColor{mist_color_.r, mist_color_.g, mist_color_.b,
                              layer.alpha * std::sqrt(remain)},
                   Gpu::DrawMode::kMist);
      }
    }
    // Chamas por cima da névoa (transparentes, escrevem profundidade).
    if (torch_ != nullptr) {
      gpu_->SetAtmosphere(arena_atmosphere_);
      for (const math::Mat4& world : torch_world_) {
        torch_->DrawFlame(*gpu_, world, arena_view_proj_, arena_atmosphere_.camera_pos,
                          ignite_);
      }
    }
    gpu_->NewLayer();  // a mão fica sempre por cima da arena
    gpu_->SetAtmosphere(hand_atmosphere_);
    hand_.Render(*gpu_, hand_view_proj_);
    gpu_->EndFrame();

    SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
    SDL_RenderClear(r);
    SDL_RenderTexture(r, gpu_->Target(), nullptr, nullptr);
  } else {
    SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
    SDL_RenderClear(r);
    text::DrawCentered(r, "Render 3D indisponivel. Veja o log.", cx, ctx.logical_height * 0.5f,
                       2.0f * ui::kScale, fg);
  }

  text::DrawCentered(r, "BATALHA", cx, ui::Px(40.0f), 4.0f * ui::kScale, colors::Get("text.title"));
  hud_.Render(r, ctx.logical_width, ctx.logical_height);
}
