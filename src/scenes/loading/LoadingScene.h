#pragma once

#include <SDL3/SDL.h>

#include <future>
#include <memory>

#include "scenes/Scene.h"
#include "scripts/gpu/Gpu.h"
#include "scripts/math/Math.h"
#include "scripts/torch/Torch.h"

// Tela de loading (docs/loading.md): fundo preto e uma única tocha 3D (models/Torch.glb)
// cintilando no canto inferior direito, acima do texto "Carregando...". Enquanto isso a cena
// alvo (ctx.loading_target) é criada e carregada em outra thread; quando a carga acabou (e a
// tela ficou um mínimo de tempo), pede a troca com fade out, e a próxima cena entra em fade
// in (os dois véus são do SceneManager; o fade in desta tela vem do menu, que o pede ao
// trocar).
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
  math::Mat4 ViewProj(int width, int height) const;

  std::unique_ptr<Gpu> gpu_;
  std::unique_ptr<Torch> torch_;
  math::Mat4 torch_world_;
  std::unique_ptr<Scene> next_;  // cena alvo já carregada, entregue ao SceneManager no fim
  std::future<void> preload_;    // Preload da cena alvo rodando em outra thread
  SceneId target_ = SceneId::kBattle;
  Atmosphere atmosphere_;
  float torch_light_ = 0.9f;  // intensidade base da luz da chama (Light.json, torch.loading)
  float time_ = 0.0f;       // segundos desde a entrada
  int frames_ = 0;
  bool loaded_ = false;
  bool done_ = false;       // cena alvo já entregue ao SceneManager (troca com fade out)
  bool ready_ = false;      // render 3D disponível
};
