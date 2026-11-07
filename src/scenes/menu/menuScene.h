#pragma once

#include <memory>
#include <vector>

#include "scenes/scene.h"
#include "scripts/button/button.h"
#include "scripts/button/buttonIcon.h"
#include "scripts/input/textInput.h"

// Menu pages (docs/menu.md): main, new save form and the save's page.
class MenuScene : public Scene {
 public:
  MenuScene();

  void OnEnter(SceneContext& ctx) override;
  void OnExit(SceneContext& ctx) override;
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;
  void RenderHud(SceneContext& ctx) override;

 private:
  using Buttons = std::vector<std::unique_ptr<Button>>;

  void ShowPage(SceneContext& ctx, MenuPage page);
  Buttons& PageButtons();
  void Layout(const SceneContext& ctx);
  void LayoutColumn(Buttons& buttons, const SceneContext& ctx);
  void LayoutForm(const SceneContext& ctx);
  void MoveFocus(int delta);
  void CreateSave(SceneContext& ctx);

  MenuPage page_ = MenuPage::kMain;
  Buttons main_buttons_;  // Play, Exit
  Buttons form_buttons_;  // Menu, Save
  Buttons save_buttons_;  // Battle, Level, Grimoire, Exit
  Button* battle_button_ = nullptr;
  Button* difficulty_button_ = nullptr;
  Button* confirm_button_ = nullptr;
  TextInput name_input_;
  ButtonIcon sound_button_{ButtonIcon::Icon::kSound};
  int focused_ = -1;
};
