#include "scenes/battle/BattleScene.h"

#include <string>

#include "scripts/image/Image.h"
#include "scripts/text/Text.h"

namespace {

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

}  // namespace

namespace {

// Fundo = cor da névoa: o que some na distância se funde com ele.
constexpr SDL_FColor kFogColor{0.15f, 0.14f, 0.18f, 1.0f};
constexpr SDL_Color kBackground{static_cast<Uint8>(kFogColor.r * 255),
                                static_cast<Uint8>(kFogColor.g * 255),
                                static_cast<Uint8>(kFogColor.b * 255), 255};
constexpr SDL_FColor kBackgroundF = kFogColor;

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
  if (gpu_ == nullptr) return;
  hand_.Clear();
  card_model_.reset();
  arena_model_.reset();
  gpu_->DestroyMesh(mist_quad_);
  for (SDL_GPUTexture* t : textures_) gpu_->DestroyTexture(t);
  textures_.clear();
}

SDL_GPUTexture* BattleScene::LoadSprite(const char* file) {
  SDL_GPUTexture* texture = gpu_->CreateTexture(Image::LoadFile(BasePath() + "sprites/" + file));
  if (texture != nullptr) textures_.push_back(texture);
  return texture;
}

void BattleScene::OnEnter(SceneContext& ctx) {
  const std::string base = BasePath();

  gpu_ = std::make_unique<Gpu>();
  if (!gpu_->Init(ctx.renderer, ctx.logical_width, ctx.logical_height, base + "shaders")) {
    SDL_Log("Falha ao iniciar o render 3D");
    gpu_.reset();
    return;
  }

  card_model_ = std::make_unique<Model>();
  if (!card_model_->LoadGlb(*gpu_, base + "models/Card.glb")) {
    SDL_Log("Falha ao carregar models/Card.glb");
    return;
  }

  SDL_GPUTexture* back = LoadSprite("CardBack.png");
  SDL_GPUTexture* good = LoadSprite("CardFrontGood.png");
  SDL_GPUTexture* evil = LoadSprite("CardFrontEvil.png");
  SDL_GPUTexture* field = LoadSprite("CardFrontField.png");

  hand_.SetModel(card_model_.get());
  hand_.SetBack(back);
  // Mão inicial de 5 cartas (ver docs/batalha.md). Conteúdo real virá do monte.
  hand_.Add(good);
  hand_.Add(evil);
  hand_.Add(good);
  hand_.Add(field);
  hand_.Add(evil);

  arena_model_ = std::make_unique<Model>();
  if (!arena_model_->LoadGlb(*gpu_, base + "models/Arena.glb")) {
    SDL_Log("Arena.glb nao carregado; gere com tools/texture_arena.py");
    arena_model_.reset();
  }
  arena_world_ = math::Mat4::Scale(kArenaScale);

  // Quad horizontal em y = 0; cada camada de névoa o desenha transladado para sua altura.
  const float h = kMistHalfSize;
  mist_quad_ = gpu_->CreateMesh(
      {{-h, 0, -h, 0, 0}, {h, 0, -h, 1, 0}, {h, 0, h, 1, 1}, {-h, 0, h, 0, 1}},
      {0, 1, 2, 0, 2, 3});

  const float aspect = static_cast<float>(ctx.logical_width) / ctx.logical_height;
  const math::Mat4 proj = math::Mat4::Perspective(math::Radians(35.0f), aspect, 0.1f, 100.0f);
  hand_view_proj_ = proj * math::Mat4::LookAt({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
  // Vista do jogador: acima e à frente do seu lado da arena, olhando para o centro.
  math::Vec3 eye{0, 11.0f, 13.0f};
  if (const char* env = SDL_getenv("PONTIFICE_ARENA_EYE")) {  // depuração: "x,y,z"
    SDL_sscanf(env, "%f,%f,%f", &eye.x, &eye.y, &eye.z);
  }
  arena_view_proj_ = proj * math::Mat4::LookAt(eye, {0, -1.0f, -0.3f}, {0, 1, 0});

  // Clima medieval: ambiente frio e azulado, luz principal quente de tocha vinda de cima e
  // de lado, luz pontual quente sobre o centro da arena, névoa na base e vinheta.
  arena_atmosphere_.light_dir = {-0.35f, 1.0f, 0.45f};
  arena_atmosphere_.light_intensity = 0.5f;
  arena_atmosphere_.light_color = {1.0f, 0.74f, 0.46f, 1.0f};
  arena_atmosphere_.ambient = {0.15f, 0.17f, 0.27f, 1.0f};
  arena_atmosphere_.point_pos = {0.0f, kArenaTopY + 4.5f, 0.0f};
  arena_atmosphere_.point_intensity = 1.7f;
  arena_atmosphere_.fog_color = kFogColor;
  arena_atmosphere_.fog_density = 0.05f;
  arena_atmosphere_.fog_start = 15.0f;
  arena_atmosphere_.fog_full_y = kArenaBaseY;
  arena_atmosphere_.fog_clear_y = kArenaTopY - 0.2f;
  arena_atmosphere_.fog_height_strength = 0.9f;
  arena_atmosphere_.camera_pos = eye;
  arena_atmosphere_.vignette = 0.55f;

  // Mão: mesma luz quente, mas sem névoa nem vinheta para as cartas ficarem legíveis.
  hand_atmosphere_.light_dir = {0.25f, 0.5f, 1.0f};
  hand_atmosphere_.light_intensity = 0.42f;
  hand_atmosphere_.light_color = {1.0f, 0.92f, 0.8f, 1.0f};
  hand_atmosphere_.ambient = {0.6f, 0.58f, 0.58f, 1.0f};
  hand_atmosphere_.camera_pos = {0, 0, 10};
  ready_ = true;
}

void BattleScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
    ctx.next_scene = SceneId::kMenu;
    return;
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
  const SDL_Color fg{230, 220, 200, 255};
  const float cx = ctx.logical_width / 2.0f;

  if (ready_ && gpu_->BeginFrame(kBackgroundF)) {
    gpu_->SetAtmosphere(arena_atmosphere_);
    if (arena_model_ != nullptr) {
      arena_model_->Draw(*gpu_, arena_world_, arena_view_proj_, SDL_FColor{1, 1, 1, 1});
    }
    // Névoa depois da arena (transparente): de baixo para cima.
    for (const MistLayer& layer : kMistLayers) {
      gpu_->Draw(mist_quad_, nullptr, math::Mat4::Translate({0, layer.y, 0}), arena_view_proj_,
                 SDL_FColor{0.78f, 0.74f, 0.8f, layer.alpha}, Gpu::DrawMode::kMist);
    }
    gpu_->NewLayer();  // a mão fica sempre por cima da arena
    gpu_->SetAtmosphere(hand_atmosphere_);
    hand_.Render(*gpu_, hand_view_proj_);
    gpu_->EndFrame();

    SDL_SetRenderDrawColor(r, kBackground.r, kBackground.g, kBackground.b, kBackground.a);
    SDL_RenderClear(r);
    SDL_RenderTexture(r, gpu_->Target(), nullptr, nullptr);
  } else {
    SDL_SetRenderDrawColor(r, kBackground.r, kBackground.g, kBackground.b, kBackground.a);
    SDL_RenderClear(r);
    text::DrawCentered(r, "Render 3D indisponivel. Veja o log.", cx, ctx.logical_height * 0.5f,
                       2.0f, fg);
  }

  text::DrawCentered(r, "BATALHA", cx, 40.0f, 4.0f, fg);
  text::DrawCentered(r, "Esc volta ao menu", cx, 80.0f, 2.0f, fg);
}
