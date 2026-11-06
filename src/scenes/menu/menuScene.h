#pragma once

#include <memory>
#include <vector>

#include "scenes/scene.h"
#include "scripts/button/button.h"
#include "scripts/button/buttonIcon.h"

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
  Button* difficulty_button_ = nullptr;
  ButtonIcon sound_button_{ButtonIcon::Icon::kSound};
  int focused_ = -1;
};
