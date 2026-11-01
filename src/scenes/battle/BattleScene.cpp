#include "scenes/battle/BattleScene.h"

#include <string>
#include <utility>

#include "scripts/colors/Colors.h"
#include "scripts/image/Image.h"
#include "scripts/text/Text.h"

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

}  // namespace

BattleScene::~BattleScene() {
  if (field_front_2d_ != nullptr) SDL_DestroyTexture(field_front_2d_);
  if (gpu_ == nullptr) return;
  hand_.Clear();
  card_model_.reset();
  arena_model_.reset();
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
  fronts_ = {UploadSprite(Image::LoadFile(base + "sprites/CardFrontGood.png")),
             UploadSprite(Image::LoadFile(base + "sprites/CardFrontEvil.png")),
             UploadSprite(Image::LoadFile(base + "sprites/CardFrontField.png"))};
  arena_model_ = std::make_unique<Model>();
  if (!arena_model_->Upload(*gpu_, Model::Parse(base + "models/Arena.glb"))) {
    SDL_Log("Arena.glb nao carregado; gere com tools/texture_arena.py");
    arena_model_.reset();
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

void BattleScene::DrawCard() {
  if (hud_.DeckRemaining() <= 0 || fronts_.empty()) return;
  hud_.SetDeck(hud_.DeckUsed() + 1, hud_.DeckRemaining() - 1);
  hand_.Add(fronts_[hud_.DeckUsed() % fronts_.size()]);
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
  // Mão inicial de 5 cartas (ver docs/batalha.md). Conteúdo real virá do monte.
  SDL_GPUTexture* good = fronts_[0];
  SDL_GPUTexture* evil = fronts_[1];
  SDL_GPUTexture* field = fronts_[2];
  hand_.Add(good);
  hand_.Add(evil);
  hand_.Add(good);
  hand_.Add(field);
  hand_.Add(evil);
  hud_.SetDeck(static_cast<int>(hand_.Size()), Hud::kDeckSize - static_cast<int>(hand_.Size()));
  arena_world_ = math::Mat4::Scale(kArenaScale);

  const float aspect = static_cast<float>(ctx.logical_width) / ctx.logical_height;
  const math::Mat4 proj = math::Mat4::Perspective(math::Radians(35.0f), aspect, 0.1f, 100.0f);
  hand_view_proj_ = proj * math::Mat4::LookAt({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
  // Vista do jogador: acima e à frente do seu lado da arena, olhando para o centro.
  math::Vec3 eye{0, 10.5f, 12.2f};
  if (const char* env = SDL_getenv("PONTIFICE_ARENA_EYE")) {  // depuração: "x,y,z"
    SDL_sscanf(env, "%f,%f,%f", &eye.x, &eye.y, &eye.z);
  }
  arena_view_proj_ = proj * math::Mat4::LookAt(eye, {0, -0.1f, -0.3f}, {0, 1, 0});

  // Clima medieval: ambiente frio e azulado, luz principal quente de tocha vinda de cima e
  // de lado, luz pontual quente sobre o centro da arena, névoa na base e vinheta.
  arena_atmosphere_.light_dir = {-0.35f, 1.0f, 0.45f};
  arena_atmosphere_.light_intensity = 0.5f;
  arena_atmosphere_.light_color = {1.0f, 0.74f, 0.46f, 1.0f};
  arena_atmosphere_.ambient = {0.15f, 0.17f, 0.27f, 1.0f};
  arena_atmosphere_.point_pos = {0.0f, kArenaTopY + 4.5f, 0.0f};
  arena_atmosphere_.point_intensity = 1.7f;
  arena_atmosphere_.fog_color = fog_color_;
  arena_atmosphere_.fog_density = 0.05f;
  arena_atmosphere_.fog_start = 15.0f;
  arena_atmosphere_.fog_full_y = kArenaBaseY;
  arena_atmosphere_.fog_clear_y = kArenaTopY - 0.2f;
  arena_atmosphere_.fog_height_strength = 0.9f;
  arena_atmosphere_.camera_pos = eye;
  arena_atmosphere_.vignette = 0.55f;

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
}

void BattleScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
        ctx.next_scene = SceneId::kMenu;
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
  if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
    mouse_ = {-1, -1};
  }
}

void BattleScene::Update(SceneContext& ctx, float dt_seconds) {
  if (!ready_) return;
  hand_.Update(dt_seconds, mouse_, hand_view_proj_, ctx.logical_width, ctx.logical_height);
}

void BattleScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color fg = colors::Get("text.primary");
  const SDL_Color background = colors::Get("background.fog");
  const float cx = ctx.logical_width / 2.0f;

  if (ready_ && gpu_->BeginFrame(fog_color_)) {
    gpu_->SetAtmosphere(arena_atmosphere_);
    if (arena_model_ != nullptr) {
      arena_model_->Draw(*gpu_, arena_world_, arena_view_proj_, SDL_FColor{1, 1, 1, 1});
    }
    // Névoa depois da arena (transparente): de baixo para cima.
    for (const MistLayer& layer : kMistLayers) {
      gpu_->Draw(mist_quad_, nullptr, math::Mat4::Translate({0, layer.y, 0}), arena_view_proj_,
                 SDL_FColor{mist_color_.r, mist_color_.g, mist_color_.b, layer.alpha},
                 Gpu::DrawMode::kMist);
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
                       2.0f, fg);
  }

  text::DrawCentered(r, "BATALHA", cx, 40.0f, 4.0f, colors::Get("text.title"));
  text::DrawCentered(r, "Esc volta ao menu", cx, 80.0f, 2.0f, fg);
  hud_.Render(r, ctx.logical_width, ctx.logical_height);
}
