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

BattleScene::~BattleScene() {
  if (gpu_ == nullptr) return;
  hand_.Clear();
  card_model_.reset();
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

  const float aspect = static_cast<float>(ctx.logical_width) / ctx.logical_height;
  const math::Mat4 proj = math::Mat4::Perspective(math::Radians(35.0f), aspect, 0.1f, 100.0f);
  const math::Mat4 view = math::Mat4::LookAt({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
  view_proj_ = proj * view;
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
  hand_.Update(dt_seconds, mouse_, view_proj_, ctx.logical_width, ctx.logical_height);
}

void BattleScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color fg{230, 220, 200, 255};
  const float cx = ctx.logical_width / 2.0f;

  if (ready_ && gpu_->BeginFrame(SDL_FColor{0.04f, 0.08f, 0.055f, 1.0f})) {
    hand_.Render(*gpu_, view_proj_);
    gpu_->EndFrame();

    SDL_SetRenderDrawColor(r, 10, 20, 14, 255);
    SDL_RenderClear(r);
    SDL_RenderTexture(r, gpu_->Target(), nullptr, nullptr);
  } else {
    SDL_SetRenderDrawColor(r, 10, 20, 14, 255);
    SDL_RenderClear(r);
    text::DrawCentered(r, "Render 3D indisponivel. Veja o log.", cx, ctx.logical_height * 0.5f,
                       2.0f, fg);
  }

  text::DrawCentered(r, "BATALHA", cx, 40.0f, 4.0f, fg);
  text::DrawCentered(r, "Esc volta ao menu", cx, 80.0f, 2.0f, fg);
}
