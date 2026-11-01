#pragma once

#include <memory>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/Button.h"

// Primeira cena do jogo. Layout descrito em docs/menu.md.
class MenuScene : public Scene {
 public:
  MenuScene();

  void OnEnter(SceneContext& ctx) override;
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Render(SceneContext& ctx) override;

 private:
  void LayoutButtons(const SceneContext& ctx);
  void MoveFocus(int delta);
  void ActivateFocused(SceneContext& ctx);

  std::vector<std::unique_ptr<Button>> buttons_;
  int focused_ = 0;
};
