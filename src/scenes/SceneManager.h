#pragma once

#include <SDL3/SDL.h>

#include <memory>

#include "scenes/Scene.h"

// Mantém a cena ativa e executa as trocas pedidas via SceneContext.
class SceneManager {
 public:
  explicit SceneManager(SceneContext ctx);

  void SwitchTo(SceneId id);
  void HandleEvent(const SDL_Event& event);
  void Update(float dt_seconds);
  void Render();

  bool ShouldQuit() const { return ctx_.quit; }
  void RequestQuit() { ctx_.quit = true; }

 private:
  void ApplyPendingSwitch();
  static std::unique_ptr<Scene> Create(SceneId id);

  SceneContext ctx_;
  std::unique_ptr<Scene> current_;
};
