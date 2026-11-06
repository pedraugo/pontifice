#include "scenes/loading/LoadingScene.h"

#include <chrono>
#include <cmath>
#include <string>
#include <utility>

#include "scenes/SceneManager.h"
#include "scripts/colors/Colors.h"
#include "scripts/config/Config.h"
#include "scripts/i18n/I18n.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

// Câmera fixa olhando para a origem; a tocha fica deslocada para o canto inferior direito.
// Com fov de 35° e a câmera a kCameraZ, o plano z = 0 mostra ~7,2 unidades de altura, então a
// tocha (~1,4 unidades com a chama) ocupa ~20% da tela. y a põe acima do texto
// "Carregando..."; x é calculado em OnEnter para centralizar a tocha com esse texto.
constexpr float kCameraZ = 11.4f;
constexpr float kFov = 35.0f;
constexpr float kTorchY = -2.8f;
// Texto "Carregando...": margem direita e escala (a largura usa os três pontos, fixa).
constexpr float kTextMargin = 60.0f;  // em px de 720p (ui::Px)
constexpr float kTextScale = 2.0f * ui::kScale;

// Largura visível do plano z = 0 (meia largura, em unidades do mundo).
float HalfWidthAtOrigin(int width, int height) {
  const float aspect = static_cast<float>(width) / height;
  return kCameraZ * std::tan(math::Radians(kFov) / 2.0f) * aspect;
}

// Centro em x (px lógicos) do texto "Carregando...", alinhado à direita com margem.
float LoadingTextCenterX(int width) {
  const std::string loading = i18n::Get("loading.label") + "...";
  const float w = text::Width(loading.c_str(), kTextScale);
  return width - ui::Px(kTextMargin) - w / 2.0f;
}
// Linha do tempo (segundos). O fade in desta tela (0,5 s) é pedido pelo menu ao trocar.
constexpr float kMinSeconds = 2.0f;      // tempo mínimo na tela, mesmo com carga rápida
constexpr float kFadeOutSeconds = 0.7f;  // saída para o preto (SceneManager)
constexpr float kNextFadeIn = 1.0f;      // fade in da próxima cena (SceneManager)
constexpr float kMaxDt = 0.05f;  // um quadro longo (envio à GPU) não faz o tempo pular

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

// Cintilar da luz da chama: soma de senos fora de fase, em torno de 1.
float Flicker(float t) {
  return 1.0f + 0.10f * std::sin(t * 11.0f) + 0.06f * std::sin(t * 5.3f + 2.0f) +
         0.04f * std::sin(t * 23.7f + 0.7f);
}

}  // namespace

LoadingScene::~LoadingScene() {
  // A thread de Preload usa next_: espera terminar antes de destruir a cena alvo.
  if (preload_.valid()) preload_.wait();
  next_.reset();
  torch_.reset();
}

void LoadingScene::OnEnter(SceneContext& ctx) {
  target_ = ctx.loading_target;
  const std::string base = BasePath();
  // Depuração: começa a linha do tempo já neste instante (segundos), útil com
  // PONTIFICE_SCREENSHOT.
  if (const char* env = SDL_getenv("PONTIFICE_LOADING_TIME")) time_ = SDL_atof(env);

  gpu_ = std::make_unique<Gpu>();
  if (!gpu_->Init(ctx.renderer, ctx.logical_width, ctx.logical_height, base + "shaders")) {
    SDL_Log("Loading: render 3D indisponivel; segue sem a tocha");
    gpu_.reset();
    return;
  }
  torch_ = std::make_unique<Torch>();
  if (!torch_->LoadGlb(*gpu_, base + "models/Torch.glb")) {
    SDL_Log("Torch.glb nao carregado; gere com tools/make_torch.py");
    torch_.reset();
    return;
  }
  // Tocha centralizada em x com o texto "Carregando...": do centro do texto (px lógicos)
  // para o plano z = 0 do mundo.
  const float ndc_x = (LoadingTextCenterX(ctx.logical_width) - ctx.logical_width / 2.0f) /
                      (ctx.logical_width / 2.0f);
  const float torch_x = ndc_x * HalfWidthAtOrigin(ctx.logical_width, ctx.logical_height);
  torch_world_ = math::Mat4::Translate({torch_x, kTorchY, 0.0f});

  // Preto: toda a luz vem da chama (luz pontual, cintilando); sem luz direcional, ambiente
  // mínimo, sem névoa, vinheta ou sombra. Valores em assets/Light.json (loading.ambient,
  // torch.loading.intensity).
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
  // Leitura, decodificação e envio à GPU em outra thread; a tocha anima enquanto isso.
  preload_ = std::async(std::launch::async, [scene = next_.get(), info = ctx.Preload()] {
    scene->Preload(info);
  });
}

void LoadingScene::FinishPreloadIfReady(SceneContext& ctx) {
  if (loaded_ || !preload_.valid()) return;
  if (preload_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
  preload_.get();
  next_->OnEnter(ctx);  // só o que depende do renderer: rápido, na thread principal
  loaded_ = true;
}

math::Mat4 LoadingScene::ViewProj(int width, int height) const {
  const float aspect = static_cast<float>(width) / height;
  const math::Mat4 proj = math::Mat4::Perspective(math::Radians(kFov), aspect, 0.1f, 100.0f);
  return proj * math::Mat4::LookAt({0.0f, 0.0f, kCameraZ}, {0.0f, 0.0f, 0.0f}, {0, 1, 0});
}

void LoadingScene::Update(SceneContext& ctx, float dt_seconds) {
  time_ += SDL_min(dt_seconds, kMaxDt);
  if (done_) return;  // escurecendo para a batalha; a tocha segue animando
  if (next_ == nullptr) StartPreload(ctx);
  FinishPreloadIfReady(ctx);

  if (loaded_ && time_ >= kMinSeconds) {
    // Entrega a cena pronta e pede a troca: o SceneManager escurece esta tela por
    // kFadeOutSeconds e abre a próxima com fade in.
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

  // "Carregando" com pontos que avançam, no canto inferior direito, abaixo da tocha.
  const int dots = static_cast<int>(time_ * 2.0f) % 4;
  const std::string loading = i18n::Get("loading.label");
  const std::string label = loading + std::string(static_cast<size_t>(dots), '.');
  const float x =
      ctx.logical_width - ui::Px(kTextMargin) - text::Width((loading + "...").c_str(), kTextScale);
  text::Draw(r, label.c_str(), x, ctx.logical_height - ui::Px(kTextMargin), kTextScale,
             colors::Get("text.muted"));
}
