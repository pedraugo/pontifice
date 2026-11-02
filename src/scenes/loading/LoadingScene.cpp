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

// Medidas do portão, as mesmas de tools/make_door.py.
constexpr float kDoorR = 1.07f;     // meia largura da porta: a dobradiça fica em x = -kDoorR
constexpr float kDoorZ = -0.42f;    // frente da porta
// Tochas presas nos pilares (faixa de aduelas), uma de cada lado, inclinadas para fora.
constexpr float kTorchX = 1.72f;
constexpr float kTorchY = 1.75f;      // ponta do cabo
constexpr float kTorchZ = 0.16f;      // à frente da parede
constexpr float kTorchTilt = 0.22f;   // rad, para fora
// Linha do tempo (segundos).
constexpr float kFadeInSeconds = 0.5f;   // entrada do preto
constexpr float kOpenStart = 0.6f;       // porta começa a abrir
constexpr float kOpenSeconds = 2.8f;     // duração da abertura
constexpr float kHoldSeconds = 0.5f;     // pausa com a porta aberta
constexpr float kFadeOutSeconds = 0.7f;  // saída para o preto
constexpr float kNextFadeIn = 1.0f;      // fade in da próxima cena
constexpr float kMaxAngle = 100.0f * math::kPi / 180.0f;
constexpr float kMaxDt = 0.05f;  // um quadro longo (envio à GPU) não faz a animação pular

// Névoa densa e espessa no chão: camadas empilhadas (assets/Fog.json, loading.mist.layers).
// De baixo para cima cada camada é mais transparente, menos densa (mais fiapos) e alcança
// menos longe do portão, para o manto ter volume (largo embaixo, estreito em cima) em vez de
// um lençol com borda reta.
std::vector<LoadingScene::MistLayer> LoadMistLayers() {
  std::vector<LoadingScene::MistLayer> layers;
  const json::Value* list = config::Node("fog", "loading.mist.layers");
  if (list != nullptr && list->IsArray()) {
    for (const json::Value& item : list->array) {
      if (!item.IsObject()) continue;
      layers.push_back({config::Float(item, "y", 0.0f), config::Float(item, "alpha", 0.5f),
                        config::Float(item, "density", 0.1f),
                        config::Float(item, "fadeStart", 3.0f),
                        config::Float(item, "fadeEnd", 8.0f)});
    }
  }
  if (layers.empty()) layers.push_back({0.02f, 0.95f, 0.20f, 4.0f, 12.0f});
  return layers;
}

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

float SmoothStep(float t) {
  t = math::Clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

LoadingScene::~LoadingScene() {
  // A thread de Preload usa next_: espera terminar antes de destruir a cena alvo.
  if (preload_.valid()) preload_.wait();
  next_.reset();
  if (gpu_ == nullptr) return;
  door_.reset();
  torch_.reset();
  gpu_->DestroyMesh(mist_quad_);
}

void LoadingScene::OnEnter(SceneContext& ctx) {
  target_ = ctx.loading_target;
  const std::string base = BasePath();
  // Depuração: começa a linha do tempo já neste instante (segundos), para inspecionar a porta
  // em um ponto fixo da abertura com PONTIFICE_SCREENSHOT.
  if (const char* env = SDL_getenv("PONTIFICE_LOADING_TIME")) time_ = SDL_atof(env);

  gpu_ = std::make_unique<Gpu>();
  if (!gpu_->Init(ctx.renderer, ctx.logical_width, ctx.logical_height, base + "shaders")) {
    SDL_Log("Loading: render 3D indisponivel; segue sem o portao");
    gpu_.reset();
    return;
  }
  door_ = std::make_unique<Model>();
  if (!door_->LoadGlb(*gpu_, base + "models/Door.glb")) {
    SDL_Log("Door.glb nao carregado; gere com tools/make_door.py");
    door_.reset();
    return;
  }
  torch_ = std::make_unique<Torch>();
  if (!torch_->LoadGlb(*gpu_, base + "models/Torch.glb")) {
    SDL_Log("Torch.glb nao carregado; gere com tools/make_torch.py");
    torch_.reset();
  }
  torch_world_[0] = math::Mat4::Translate({-kTorchX, kTorchY, kTorchZ}) *
                    math::Mat4::RotateZ(kTorchTilt);
  torch_world_[1] = math::Mat4::Translate({kTorchX, kTorchY, kTorchZ}) *
                    math::Mat4::RotateZ(-kTorchTilt);

  const float h = config::Float("fog", "loading.mist.halfSize", 40.0f);
  mist_quad_ = gpu_->CreateMesh(
      {{-h, 0, -h, 0, 0}, {h, 0, -h, 1, 0}, {h, 0, h, 1, 1}, {-h, 0, h, 0, 1}},
      {0, 1, 2, 0, 2, 3});
  mist_color_ = config::Color("fog", "loading.mist.color", colors::GetF("background.loadingMist"));
  mist_layers_ = LoadMistLayers();

  // Vazio preto: toda a luz vem das duas tochas (luzes pontuais nas chamas); sem luz
  // direcional e com ambiente mínimo só para o que fica de costas não sumir. Névoa por
  // distância preta (a passagem some no escuro) e vinheta forte. A névoa de chão é feita
  // pelas camadas kMist, não pela névoa de altura (que escureceria para o preto).
  // Valores em assets/Light.json (loading.*, torch.*), Fog.json (loading.fog) e Shadow.json
  // (loading.*).
  atmosphere_.light_dir = config::Vec3("light", "loading.lightDir", {0.45f, 0.55f, 1.0f});
  atmosphere_.light_intensity = config::Float("light", "loading.lightIntensity", 0.0f);
  atmosphere_.light_color = Torch::LightColor();
  atmosphere_.ambient = config::Color("light", "loading.ambient", {0.025f, 0.028f, 0.04f, 1.0f});
  const float torch_light = config::Float("light", "torch.loading.intensity", 0.9f);
  atmosphere_.point_pos = Torch::FlamePosition(torch_world_[0]);
  atmosphere_.point_intensity = torch_light;
  atmosphere_.point2_pos = Torch::FlamePosition(torch_world_[1]);
  atmosphere_.point2_intensity = torch_light;
  // Sombras: cada chama olha para o centro do portão; o muro, a porta (ao abrir) e as cestas
  // das tochas projetam sombra no muro, na passagem e na névoa do chão.
  const math::Mat4 shadow_proj = math::Mat4::Perspective(
      math::Radians(config::Float("shadow", "loading.fov", 125.0f)), 1.0f,
      config::Float("shadow", "loading.near", 0.2f), config::Float("shadow", "loading.far", 40.0f));
  const math::Vec3 shadow_target = config::Vec3("shadow", "loading.target", {0.0f, 1.4f, -0.4f});
  for (int k = 0; k < 2; ++k) {
    const math::Vec3 flame = k == 0 ? atmosphere_.point_pos : atmosphere_.point2_pos;
    torch_light_vp_[k] = shadow_proj * math::Mat4::LookAt(flame, shadow_target, {0, 1, 0});
  }
  atmosphere_.shadow_strength = config::Float("shadow", "loading.strength", 0.9f);
  atmosphere_.shadow_bias = config::Float("shadow", "bias", 0.0015f);
  atmosphere_.fog_color = config::Color("fog", "loading.fog.color", {0, 0, 0, 1});
  atmosphere_.fog_density = config::Float("fog", "loading.fog.density", 0.13f);
  atmosphere_.fog_start = config::Float("fog", "loading.fog.start", 6.0f);
  atmosphere_.fog_height_strength = config::Float("fog", "loading.fog.heightStrength", 0.0f);
  atmosphere_.vignette = config::Float("light", "loading.vignette", 0.8f);
  // Alcance e densidade vêm de cada camada (mist_layers_).
  atmosphere_.mist_noise_scale = config::Float("fog", "loading.mist.noiseScale", 1.4f);
  ready_ = true;
}

void LoadingScene::StartPreload(const SceneContext& ctx) {
  next_ = SceneManager::Create(target_);
  // Leitura, decodificação e envio à GPU em outra thread; o portão anima enquanto isso.
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

float LoadingScene::DoorAngle() const {
  return kMaxAngle * SmoothStep((time_ - kOpenStart) / kOpenSeconds);
}

math::Mat4 LoadingScene::ViewProj(int width, int height) const {
  const float aspect = static_cast<float>(width) / height;
  const math::Mat4 proj = math::Mat4::Perspective(math::Radians(35.0f), aspect, 0.1f, 100.0f);
  // Câmera um pouco à direita e abaixo do centro do arco (como em art/Door.jpg), aproximando-se
  // devagar enquanto a porta abre.
  const float push = SmoothStep((time_ - kOpenStart) / (kOpenSeconds + kHoldSeconds));
  const math::Vec3 eye{1.4f, 1.8f, 8.4f - 1.0f * push};
  return proj * math::Mat4::LookAt(eye, {0.0f, 2.05f, -0.4f}, {0, 1, 0});
}

void LoadingScene::Update(SceneContext& ctx, float dt_seconds) {
  ++frames_;
  time_ += SDL_min(dt_seconds, kMaxDt);
  if (next_ == nullptr) StartPreload(ctx);
  FinishPreloadIfReady(ctx);

  const float animation_end = kOpenStart + kOpenSeconds + kHoldSeconds;
  if (loaded_ && time_ >= animation_end) {
    fade_out_ = math::Clamp((time_ - animation_end) / kFadeOutSeconds, 0.0f, 1.0f);
    if (fade_out_ >= 1.0f) {
      ctx.preloaded = std::move(next_);
      ctx.fade_in_seconds = kNextFadeIn;
      ctx.next_scene = target_;
    }
  }
}

void LoadingScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color black = colors::Get("background.loading");
  SDL_SetRenderDrawColor(r, black.r, black.g, black.b, 255);
  SDL_RenderClear(r);

  // Porta gira em torno da dobradiça (ombreira esquerda); o resto fica parado.
  const math::Vec3 hinge{-kDoorR, 0.0f, kDoorZ};
  const math::Mat4 door_world = math::Mat4::Translate(hinge) *
                                math::Mat4::RotateY(DoorAngle()) *
                                math::Mat4::Translate({-hinge.x, -hinge.y, -hinge.z});
  if (ready_) {
    for (int k = 0; k < 2; ++k) {
      if (!gpu_->BeginShadowPass(k, torch_light_vp_[k])) break;
      for (const Model::Part& part : door_->Parts()) {
        gpu_->DrawShadow(part.mesh, part.name == "Door" ? door_world : math::Mat4::Identity());
      }
      if (torch_ != nullptr) {
        for (const math::Mat4& world : torch_world_) torch_->DrawBodyDepth(*gpu_, world);
      }
      gpu_->EndShadowPass();
    }
  }
  if (ready_ && gpu_->BeginFrame(SDL_FColor{0, 0, 0, 1})) {
    const math::Mat4 view_proj = ViewProj(ctx.logical_width, ctx.logical_height);
    atmosphere_.camera_pos = {1.4f, 1.8f, 7.6f};
    gpu_->SetAtmosphere(atmosphere_);
    for (const Model::Part& part : door_->Parts()) {
      const math::Mat4& world = part.name == "Door" ? door_world : math::Mat4::Identity();
      gpu_->Draw(part.mesh, part.texture, world, view_proj, SDL_FColor{1, 1, 1, 1},
                 Gpu::DrawMode::kMesh, part.normal);
    }
    if (torch_ != nullptr) {
      for (const math::Mat4& world : torch_world_) torch_->DrawBody(*gpu_, world, view_proj);
    }
    // Névoa no chão depois do portão (transparente), de baixo para cima; cada camada com
    // seu alcance e densidade.
    for (const MistLayer& layer : mist_layers_) {
      Atmosphere mist = atmosphere_;
      mist.mist_density = layer.density;
      mist.mist_fade_start = layer.fade_start;
      mist.mist_fade_end = layer.fade_end;
      gpu_->SetAtmosphere(mist);
      gpu_->Draw(mist_quad_, nullptr, math::Mat4::Translate({0, layer.y, 0}), view_proj,
                 SDL_FColor{mist_color_.r, mist_color_.g, mist_color_.b, layer.alpha},
                 Gpu::DrawMode::kMist);
    }
    // Chamas por cima da névoa (transparentes, escrevem profundidade).
    if (torch_ != nullptr) {
      gpu_->SetAtmosphere(atmosphere_);
      for (const math::Mat4& world : torch_world_) {
        torch_->DrawFlame(*gpu_, world, view_proj, atmosphere_.camera_pos);
      }
    }
    gpu_->EndFrame();
    SDL_RenderTexture(r, gpu_->Target(), nullptr, nullptr);
  }

  // "Carregando" com pontos que avançam, no canto inferior direito.
  const int dots = static_cast<int>(time_ * 2.0f) % 4;
  const std::string loading = i18n::Get("loading.label");
  const std::string label = loading + std::string(static_cast<size_t>(dots), '.');
  const float scale = 2.0f * ui::kScale;
  const float x = ctx.logical_width - ui::Px(60.0f) - text::Width((loading + "...").c_str(), scale);
  text::Draw(r, label.c_str(), x, ctx.logical_height - ui::Px(60.0f), scale,
             colors::Get("text.muted"));

  // Véu preto: entrada (clareia) e saída (escurece).
  const float fade_in = 1.0f - math::Clamp(time_ / kFadeInSeconds, 0.0f, 1.0f);
  const float veil = SDL_max(fade_in, fade_out_);
  if (veil > 0.0f) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, static_cast<Uint8>(veil * 255.0f));
    const SDL_FRect full{0, 0, static_cast<float>(ctx.logical_width),
                         static_cast<float>(ctx.logical_height)};
    SDL_RenderFillRect(r, &full);
  }
}
