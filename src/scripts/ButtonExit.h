#pragma once

#include "scripts/Button.h"

// Botão "Sair": encerra o jogo.
class ButtonExit : public Button {
 public:
  ButtonExit();
  void OnClick(SceneContext& ctx) override;
};
