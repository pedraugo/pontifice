#include "scenes/loading/LoadingScene.h"

#include <chrono>
#include <cmath>
#include <string>
#include <utility>

#include "scenes/SceneManager.h"
#include "scripts/colors/Colors.h"
#include "scripts/text/Text.h"

namespace {

// Medidas do portão, as mesmas de tools/make_door.py.
constexpr float kDoorR = 1.07f;     // meia largura da porta: a dobradiça fica em x = -kDoorR
constexpr float kDoorZ = -0.42f;    // frente da porta
// Tochas presas nos pilares (faixa de aduelas), uma de cada lado, inclinadas para fora.
constexpr float kTorchX = 1.72f;
constexpr float kTorchY = 1.75f;      // ponta do cabo
constexpr float kTorchZ = 0.16f;      // à frente da parede
constexpr float kTorchTilt = 0.22f;   // rad, para fora
constexpr float kTorchLight = 0.9f;   // intensidade de cada chama
// Linha do tempo (segundos).
constexpr float kFadeInSeconds = 0.5f;   // entrada do preto
constexpr float kOpenStart = 0.6f;       // porta começa a abrir
constexpr float kOpenSeconds = 2.8f;     // duração da abertura
constexpr float kHoldSeconds = 0.5f;     // pausa com a porta aberta
constexpr float kFadeOutSeconds = 0.7f;  // saída para o preto
constexpr float kNextFadeIn = 1.0f;      // fade in da próxima cena
constexpr float kMaxAngle = 100.0f * math::kPi / 180.0f;
constexpr float kMaxDt = 0.05f;  // um quadro longo (envio à GPU) não faz a animação pular

// Névoa densa e espessa no chão: camadas kMist empilhadas. De baixo para cima cada camada é
// mais transparente, menos densa (mais fiapos) e alcança menos longe do portão, para o manto
// ter volume (largo embaixo, estreito em cima) em vez de um lençol com borda reta.
struct MistLayer {
  float y;
  float alpha;
  float density;     // Atmosphere::mist_density
  float fade_start;  // raios onde some (Atmosphere::mist_fade_*)
  float fade_end;
};
constexpr MistLayer kMistLayers[] = {
    {0.02f, 0.95f, 0.20f, 4.0f, 12.0f}, {0.16f, 0.85f, 0.17f, 3.6f, 10.5f},
    {0.32f, 0.75f, 0.14f, 3.2f, 9.0f},  {0.50f, 0.62f, 0.11f, 2.8f, 7.6f},
    {0.72f, 0.48f, 0.08f, 2.4f, 6.4f},  {0.98f, 0.34f, 0.05f, 2.0f, 5.4f},
    {1.30f, 0.20f, 0.02f, 1.6f, 4.6f},
};
constexpr float kMistHalfSize = 40.0f;

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

  const float h = kMistHalfSize;
  mist_quad_ = gpu_->CreateMesh(
      {{-h, 0, -h, 0, 0}, {h, 0, -h, 1, 0}, {h, 0, h, 1, 1}, {-h, 0, h, 0, 1}},
      {0, 1, 2, 0, 2, 3});
  mist_color_ = colors::GetF("background.loadingMist");

  // Vazio preto: toda a luz vem das duas tochas (luzes pontuais nas chamas); sem luz
  // direcional e com ambiente mínimo só para o que fica de costas não sumir. Névoa por
  // distância preta (a passagem some no escuro) e vinheta forte. A névoa de chão é feita
  // pelas camadas kMist, não pela névoa de altura (que escureceria para o preto).
  atmosphere_.light_dir = {0.45f, 0.55f, 1.0f};
  atmosphere_.light_intensity = 0.0f;
  atmosphere_.light_color = {1.0f, 0.78f, 0.52f, 1.0f};
  atmosphere_.ambient = {0.025f, 0.028f, 0.04f, 1.0f};
  atmosphere_.point_pos = Torch::FlamePosition(torch_world_[0]);
  atmosphere_.point_intensity = kTorchLight;
  atmosphere_.point2_pos = Torch::FlamePosition(torch_world_[1]);
  atmosphere_.point2_intensity = kTorchLight;
  atmosphere_.fog_color = {0, 0, 0, 1};
  atmosphere_.fog_density = 0.13f;
  atmosphere_.fog_start = 6.0f;
  atmosphere_.fog_height_strength = 0.0f;
  atmosphere_.vignette = 0.8f;
  atmosphere_.mist_noise_scale = 1.4f;  // alcance e densidade vêm de cada kMistLayer
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

  if (ready_ && gpu_->BeginFrame(SDL_FColor{0, 0, 0, 1})) {
    const math::Mat4 view_proj = ViewProj(ctx.logical_width, ctx.logical_height);
    atmosphere_.camera_pos = {1.4f, 1.8f, 7.6f};
    gpu_->SetAtmosphere(atmosphere_);
    // Porta gira em torno da dobradiça (ombreira esquerda); o resto fica parado.
    const math::Vec3 hinge{-kDoorR, 0.0f, kDoorZ};
    const math::Mat4 door_world = math::Mat4::Translate(hinge) *
                                  math::Mat4::RotateY(DoorAngle()) *
                                  math::Mat4::Translate({-hinge.x, -hinge.y, -hinge.z});
    for (const Model::Part& part : door_->Parts()) {
      const math::Mat4& world = part.name == "Door" ? door_world : math::Mat4::Identity();
      gpu_->Draw(part.mesh, part.texture, world, view_proj, SDL_FColor{1, 1, 1, 1});
    }
    if (torch_ != nullptr) {
      for (const math::Mat4& world : torch_world_) torch_->DrawBody(*gpu_, world, view_proj);
    }
    // Névoa no chão depois do portão (transparente), de baixo para cima; cada camada com
    // seu alcance e densidade.
    for (const MistLayer& layer : kMistLayers) {
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
  const std::string label = "Carregando" + std::string(static_cast<size_t>(dots), '.');
  const float scale = 2.0f;
  const float x = ctx.logical_width - 60.0f - text::Width("Carregando...", scale);
  text::Draw(r, label.c_str(), x, ctx.logical_height - 60.0f, scale, colors::Get("text.muted"));

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
