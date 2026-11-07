#include "scenes/loading/loadingScene.h"

#include <chrono>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "scenes/sceneManager.h"
#include "scripts/colors/colors.h"
#include "scripts/config/config.h"
#include "scripts/i18n/i18n.h"
#include "scripts/text/text.h"
#include "scripts/layout/layout.h"
#include "scripts/ui/ui.h"
#include "scripts/log/log.h"

namespace {

struct FlickerWave {
  float amplitude, frequency, phase;
};

// Label margin from ui.json; camera, torch and timings from layout.json ("loading"); read once.
struct Settings {
  float camera_z = layout::Number("loading.camera.z", 11.4f);
  float fov = layout::Number("loading.camera.fov", 35.0f);
  float torch_y = layout::Number("loading.torch.y", -2.8f);
  float text_margin = ui::Number("loading.textMargin", 90.0f);
  float dots_per_second = layout::Number("loading.dotsPerSecond", 2.0f);
  float min_seconds = layout::Number("loading.minSeconds", 2.0f);
  float fade_out = layout::Number("loading.fadeOut", 0.7f);
  float next_fade_in = layout::Number("loading.nextFadeIn", 1.0f);
  std::vector<FlickerWave> flicker = LoadFlicker();

  static std::vector<FlickerWave> LoadFlicker() {
    std::vector<FlickerWave> waves;
    const json::Value* list = layout::Node("loading.torch.flicker");
    if (list != nullptr && list->IsArray()) {
      for (const json::Value& item : list->array) {
        if (!item.IsObject()) continue;
        waves.push_back({config::Float(item, "amplitude", 0.0f),
                         config::Float(item, "frequency", 0.0f),
                         config::Float(item, "phase", 0.0f)});
      }
    }
    if (waves.empty()) waves = {{0.10f, 11.0f, 0.0f}, {0.06f, 5.3f, 2.0f}, {0.04f, 23.7f, 0.7f}};
    return waves;
  }
};

const Settings& S() {
  static const Settings settings;
  return settings;
}

float HalfWidthAtOrigin(int width, int height) {
  const float aspect = static_cast<float>(width) / height;
  return S().camera_z * std::tan(math::Radians(S().fov) / 2.0f) * aspect;
}

const text::Style& Style() {
  static const text::Style style = text::Styles({"regular"});
  return style;
}

float LoadingTextCenterX(int width) {
  const std::string loading = i18n::Get("loading.label") + "...";
  const float w = text::Width(loading.c_str(), Style());
  return width - S().text_margin - w / 2.0f;
}

constexpr float kMaxDt = 0.05f;

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

float Flicker(float t) {
  float value = 1.0f;
  for (const FlickerWave& wave : S().flicker) {
    value += wave.amplitude * std::sin(t * wave.frequency + wave.phase);
  }
  return value;
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
  torch_world_ = math::Mat4::Translate({torch_x, S().torch_y, 0.0f});

  atmosphere_.light_intensity = 0.0f;
  atmosphere_.light_color = Torch::LightColor();
  atmosphere_.ambient = config::Color("light", "loading.ambient", {0.025f, 0.028f, 0.04f, 1.0f});
  torch_light_ = config::Float("light", "torch.loading.intensity", 0.9f);
  atmosphere_.point_pos = Torch::FlamePosition(torch_world_);
  atmosphere_.point_intensity = torch_light_;
  atmosphere_.fog_density = 0.0f;
  atmosphere_.vignette = 0.0f;
  atmosphere_.shadow_strength = 0.0f;
  atmosphere_.camera_pos = {0.0f, 0.0f, S().camera_z};
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
  const math::Mat4 proj = math::Mat4::Perspective(math::Radians(S().fov), aspect, 0.1f, 100.0f);
  return proj * math::Mat4::LookAt({0.0f, 0.0f, S().camera_z}, {0.0f, 0.0f, 0.0f}, {0, 1, 0});
}

void LoadingScene::Update(SceneContext& ctx, float dt_seconds) {
  time_ += SDL_min(dt_seconds, kMaxDt);  // a long frame does not skip the animation
  if (done_) return;
  if (next_ == nullptr) StartPreload(ctx);
  FinishPreloadIfReady(ctx);

  if (loaded_ && time_ >= S().min_seconds) {
    ctx.preloaded = std::move(next_);
    ctx.fade_out_seconds = S().fade_out;
    ctx.fade_in_seconds = S().next_fade_in;
    ctx.next_scene = target_;
    done_ = true;
  }
}

void LoadingScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color black = colors::Get("stoneDarker");
  SDL_SetRenderDrawColor(r, black.r, black.g, black.b, 255);
  SDL_RenderClear(r);

  if (ready_ && gpu_->BeginFrame(colors::GetF("stoneDarker"))) {
    const math::Mat4 view_proj = ViewProj(ctx.logical_width, ctx.logical_height);
    atmosphere_.point_intensity = torch_light_ * Flicker(time_);
    gpu_->SetAtmosphere(atmosphere_);
    torch_->DrawBody(*gpu_, torch_world_, view_proj);
    torch_->DrawFlame(*gpu_, torch_world_, view_proj, atmosphere_.camera_pos);
    gpu_->EndFrame();
    SDL_RenderTexture(r, gpu_->Target(), nullptr, nullptr);
  }
}

void LoadingScene::RenderHud(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const int dots = static_cast<int>(time_ * S().dots_per_second) % 4;
  const std::string loading = i18n::Get("loading.label");
  const std::string label = loading + std::string(static_cast<size_t>(dots), '.');
  // Full label width, so the text does not shift as the dots change.
  const float x =
      ctx.logical_width - S().text_margin - text::Width((loading + "...").c_str(), Style());
  text::Draw(r, label.c_str(), x, ctx.logical_height - S().text_margin, Style(),
             colors::Get("slate"));
}
