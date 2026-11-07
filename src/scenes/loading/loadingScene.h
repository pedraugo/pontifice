#pragma once

#include <SDL3/SDL.h>

#include <future>
#include <memory>

#include "scenes/scene.h"
#include "scripts/gpu/gpu.h"
#include "scripts/math/math.h"
#include "scripts/torch/torch.h"

class LoadingScene : public Scene {
 public:
  ~LoadingScene() override;

  void OnEnter(SceneContext& ctx) override;
  void HandleEvent(SceneContext&, const SDL_Event&) override {}
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;
  void RenderHud(SceneContext& ctx) override;

 private:
  void StartPreload(const SceneContext& ctx);
  void FinishPreloadIfReady(SceneContext& ctx);
  math::Mat4 ViewProj(int width, int height) const;

  std::unique_ptr<Gpu> gpu_;
  std::unique_ptr<Torch> torch_;
  math::Mat4 torch_world_;
  std::unique_ptr<Scene> next_;
  std::future<void> preload_;
  SceneId target_ = SceneId::kBattle;
  Atmosphere atmosphere_;
  float torch_light_ = 0.9f;
  float time_ = 0.0f;
  bool loaded_ = false;
  bool done_ = false;
  bool ready_ = false;
};
