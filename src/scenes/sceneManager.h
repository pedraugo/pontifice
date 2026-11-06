#pragma once

#include <SDL3/SDL.h>

#include <memory>

#include "scenes/scene.h"

class SceneManager {
 public:
  explicit SceneManager(SceneContext ctx);

  void SwitchTo(SceneId id);
  void HandleEvent(const SDL_Event& event);
  void Update(float dt_seconds);
  void Render();

  bool ShouldQuit() const { return ctx_.quit; }
  void RequestQuit() { ctx_.quit = true; }

  static std::unique_ptr<Scene> Create(SceneId id);

 private:
  void ApplyPendingSwitch();

  SceneContext ctx_;
  std::unique_ptr<Scene> current_;
  float fade_in_total_ = 0.0f;
  float fade_in_left_ = 0.0f;
  float fade_out_total_ = 0.0f;
  float fade_out_left_ = 0.0f;
};
