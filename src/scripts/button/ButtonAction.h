#pragma once

#include <functional>
#include <string>

#include "scripts/button/Button.h"

// Botão genérico: rótulo e ação da cena ao ser clicado (ex.: "Passar turno", "Nível").
class ButtonAction : public Button {
 public:
  using Action = std::function<void(SceneContext&)>;
  ButtonAction(std::string label, Action action);
  void OnClick(SceneContext& ctx) override;

 private:
  Action action_;
};
