#pragma once

#include <SDL3/SDL.h>

#include <future>
#include <memory>

#include "scenes/Scene.h"
#include "scripts/gpu/Gpu.h"
#include "scripts/math/Math.h"
#include "scripts/model/Model.h"
#include "scripts/torch/Torch.h"

// Tela de loading (docs/loading.md), no estilo de Resident Evil Survivor: fundo todo preto e
// um objeto 3D em ação. Aqui, um portão medieval (models/Door.glb) se abrindo para dentro.
// Enquanto o portão abre, a cena alvo (ctx.loading_target) é criada e carregada; quando a
// animação termina e a carga acabou, a tela escurece e a próxima cena entra em fade in.
class LoadingScene : public Scene {
 public:
  ~LoadingScene() override;

  void OnEnter(SceneContext& ctx) override;
  void HandleEvent(SceneContext& /*ctx*/, const SDL_Event& /*event*/) override {}
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;

 private:
  void StartPreload(const SceneContext& ctx);    // cria a cena alvo; Preload em outra thread
  void FinishPreloadIfReady(SceneContext& ctx);  // OnEnter (GPU) na thread principal
  float DoorAngle() const;  // radianos, 0 = fechada
  math::Mat4 ViewProj(int width, int height) const;

  std::unique_ptr<Gpu> gpu_;
  std::unique_ptr<Model> door_;
  std::unique_ptr<Torch> torch_;
  math::Mat4 torch_world_[2];  // tochas nos pilares esquerdo e direito do portão
  math::Mat4 torch_light_vp_[2];  // câmera de sombra de cada chama
  GpuMesh mist_quad_;           // quad horizontal reusado nas camadas de névoa do chão
  SDL_FColor mist_color_{};     // background.mist (colors.json)
  std::unique_ptr<Scene> next_;  // cena alvo já carregada, entregue ao SceneManager no fim
  std::future<void> preload_;    // Preload da cena alvo rodando em outra thread
  SceneId target_ = SceneId::kBattle;
  Atmosphere atmosphere_;
  float time_ = 0.0f;       // segundos desde a entrada
  float fade_out_ = 0.0f;   // 0..1, véu preto no fim
  int frames_ = 0;
  bool loaded_ = false;
  bool ready_ = false;      // render 3D disponível
};
