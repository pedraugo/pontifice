#pragma once

#include "scripts/Button.h"

// Botão "Jogar": leva à cena de batalha.
class ButtonPlay : public Button {
 public:
  ButtonPlay();
  void OnClick(SceneContext& ctx) override;
};
