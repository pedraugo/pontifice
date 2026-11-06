#pragma once

#include <memory>
#include <string>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/button/ButtonText.h"

class LanguageScene : public Scene {
 public:
  LanguageScene();

  void OnEnter(SceneContext& ctx) override;
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;

 private:
  void AddLanguage(const std::string& code, const std::string& name, flag::Country country);
  void LayoutButtons(const SceneContext& ctx);
  void MoveFocus(int delta);

  std::vector<std::unique_ptr<ButtonText>> buttons_;
  int focused_ = -1;
};
