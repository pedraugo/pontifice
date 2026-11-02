#pragma once

#include <memory>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/button/Button.h"
#include "scripts/clouds/Clouds.h"
#include "scripts/soundbutton/SoundButton.h"

// Primeira cena do jogo. Layout descrito em docs/menu.md.
class MenuScene : public Scene {
 public:
  MenuScene();

  ~MenuScene() override;

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
  SoundButton sound_button_;  // canto inferior esquerdo: liga/desliga a música
  SDL_Texture* logo_ = nullptr;  // sprites/Logo.png (título); nulo cai no texto
  int logo_w_ = 0;
  int logo_h_ = 0;
  SDL_Texture* icon_ = nullptr;  // assets/icon.png, acima do logo
  bool clouds_ready_ = false;
  int focused_ = 0;
};
