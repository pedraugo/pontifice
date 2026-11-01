#pragma once

#include "scripts/button/Button.h"

// Botão "Sair": encerra o jogo.
class ButtonExit : public Button {
 public:
  ButtonExit();
  void OnClick(SceneContext& ctx) override;
};
