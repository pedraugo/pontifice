#pragma once

#include <memory>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/button/Button.h"
#include "scripts/clouds/Clouds.h"

// Primeira cena do jogo. Layout descrito em docs/menu.md.
class MenuScene : public Scene {
 public:
  MenuScene();

  void OnEnter(SceneContext& ctx) override;
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;

 private:
  void LayoutButtons(const SceneContext& ctx);
  void MoveFocus(int delta);
  void ActivateFocused(SceneContext& ctx);

  std::vector<std::unique_ptr<Button>> buttons_;
  Button* difficulty_button_ = nullptr;  // "Nível: Fácil/Médio/Difícil/Impossível", cicla
  Clouds clouds_;  // nuvens atrás do texto e névoa na frente
  bool clouds_ready_ = false;
  int focused_ = 0;
};
