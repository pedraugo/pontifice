#include "scenes/loading/loadingScene.h"

#include <chrono>
#include <cmath>
#include <string>
#include <utility>

#include "scenes/sceneManager.h"
#include "scripts/colors/colors.h"
#include "scripts/config/config.h"
#include "scripts/i18n/i18n.h"
#include "scripts/text/text.h"
#include "scripts/ui/ui.h"
#include "scripts/log/log.h"

namespace {

constexpr float kCameraZ = 11.4f;
constexpr float kFov = 35.0f;
constexpr float kTorchY = -2.8f;
constexpr float kTextMargin = 60.0f;
constexpr float kTextScale = 2.0f * ui::kScale;

float HalfWidthAtOrigin(int width, int height) {
  const float aspect = static_cast<float>(width) / height;
  return kCameraZ * std::tan(math::Radians(kFov) / 2.0f) * aspect;
}

float LoadingTextCenterX(int width) {
  const std::string loading = i18n::Get("loading.label") + "...";
  const float w = text::Width(loading.c_str(), kTextScale);
  return width - ui::Px(kTextMargin) - w / 2.0f;
}
constexpr float kMinSeconds = 2.0f;
constexpr float kFadeOutSeconds = 0.7f;
constexpr float kNextFadeIn = 1.0f;
constexpr float kMaxDt = 0.05f;

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

float Flicker(float t) {
  return 1.0f + 0.10f * std::sin(t * 11.0f) + 0.06f * std::sin(t * 5.3f + 2.0f) +
         0.04f * std::sin(t * 23.7f + 0.7f);
}

}

LoadingScene::~LoadingScene() {
  if (preload_.valid()) preload_.wait();  // Preload may still run on another thread
  next_.reset();
  torch_.reset();
}

void LoadingScene::OnEnter(SceneContext& ctx) {
  target_ = ctx.loading_target;
  const std::string base = BasePath();
  // Debug: starting the clock ahead shortens the minimum wait.
  if (const char* env = SDL_getenv("PONTIFICE_LOADING_TIME")) time_ = SDL_atof(env);

  gpu_ = std::make_unique<Gpu>();
  if (!gpu_->Init(ctx.renderer, ctx.logical_width, ctx.logical_height, base + "shaders")) {
    LOG("Loading: 3D rendering unavailable; continuing without the torch");
    gpu_.reset();
    return;
  }
  torch_ = std::make_unique<Torch>();
  if (!torch_->LoadGlb(*gpu_, base + "models/torch.glb")) {
    LOG("torch.glb not loaded; generate it with tools/makeTorch.py");
    torch_.reset();
    return;
  }
  // Torch placed in world space under the loading label center.
  const float ndc_x = (LoadingTextCenterX(ctx.logical_width) - ctx.logical_width / 2.0f) /
                      (ctx.logical_width / 2.0f);
  const float torch_x = ndc_x * HalfWidthAtOrigin(ctx.logical_width, ctx.logical_height);
  torch_world_ = math::Mat4::Translate({torch_x, kTorchY, 0.0f});

  atmosphere_.light_intensity = 0.0f;
  atmosphere_.light_color = Torch::LightColor();
  atmosphere_.ambient = config::Color("light", "loading.ambient", {0.025f, 0.028f, 0.04f, 1.0f});
  torch_light_ = config::Float("light", "torch.loading.intensity", 0.9f);
  atmosphere_.point_pos = Torch::FlamePosition(torch_world_);
  atmosphere_.point_intensity = torch_light_;
  atmosphere_.fog_density = 0.0f;
  atmosphere_.vignette = 0.0f;
  atmosphere_.shadow_strength = 0.0f;
  atmosphere_.camera_pos = {0.0f, 0.0f, kCameraZ};
  ready_ = true;
}

void LoadingScene::StartPreload(const SceneContext& ctx) {
  next_ = SceneManager::Create(target_);
  preload_ = std::async(std::launch::async, [scene = next_.get(), info = ctx.Preload()] {
    scene->Preload(info);
  });
}

void LoadingScene::FinishPreloadIfReady(SceneContext& ctx) {
  if (loaded_ || !preload_.valid()) return;
  if (preload_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
  preload_.get();
  next_->OnEnter(ctx);
  loaded_ = true;
}

math::Mat4 LoadingScene::ViewProj(int width, int height) const {
  const float aspect = static_cast<float>(width) / height;
  const math::Mat4 proj = math::Mat4::Perspective(math::Radians(kFov), aspect, 0.1f, 100.0f);
  return proj * math::Mat4::LookAt({0.0f, 0.0f, kCameraZ}, {0.0f, 0.0f, 0.0f}, {0, 1, 0});
}

void LoadingScene::Update(SceneContext& ctx, float dt_seconds) {
  time_ += SDL_min(dt_seconds, kMaxDt);  // a long frame does not skip the animation
  if (done_) return;
  if (next_ == nullptr) StartPreload(ctx);
  FinishPreloadIfReady(ctx);

  if (loaded_ && time_ >= kMinSeconds) {
    ctx.preloaded = std::move(next_);
    ctx.fade_out_seconds = kFadeOutSeconds;
    ctx.fade_in_seconds = kNextFadeIn;
    ctx.next_scene = target_;
    done_ = true;
  }
}

void LoadingScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color black = colors::Get("background.loading");
  SDL_SetRenderDrawColor(r, black.r, black.g, black.b, 255);
  SDL_RenderClear(r);

  if (ready_ && gpu_->BeginFrame(colors::GetF("background.loading"))) {
    const math::Mat4 view_proj = ViewProj(ctx.logical_width, ctx.logical_height);
    atmosphere_.point_intensity = torch_light_ * Flicker(time_);
    gpu_->SetAtmosphere(atmosphere_);
    torch_->DrawBody(*gpu_, torch_world_, view_proj);
    torch_->DrawFlame(*gpu_, torch_world_, view_proj, atmosphere_.camera_pos);
    gpu_->EndFrame();
    SDL_RenderTexture(r, gpu_->Target(), nullptr, nullptr);
  }

  const int dots = static_cast<int>(time_ * 2.0f) % 4;
  const std::string loading = i18n::Get("loading.label");
  const std::string label = loading + std::string(static_cast<size_t>(dots), '.');
  // Full label width, so the text does not shift as the dots change.
  const float x =
      ctx.logical_width - ui::Px(kTextMargin) - text::Width((loading + "...").c_str(), kTextScale);
  text::Draw(r, label.c_str(), x, ctx.logical_height - ui::Px(kTextMargin), kTextScale,
             colors::Get("text.muted"));
}
