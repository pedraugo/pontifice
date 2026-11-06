#pragma once

#include <memory>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/button/Button.h"
#include "scripts/soundbutton/SoundButton.h"

// Menu, logo depois da escolha de idioma. Fundo liso, título "PONTIFICE" em texto (PirataOne)
// e itens só de texto (Jogar, Nível, Sair). Layout descrito em docs/menu.md.
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
  Button* difficulty_button_ = nullptr;  // "Nível: <nome>" (menu.level), cicla
  SoundButton sound_button_;  // canto inferior esquerdo: liga/desliga a música
  int focused_ = -1;  // -1: nenhum item focado
};
