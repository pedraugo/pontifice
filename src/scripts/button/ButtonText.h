#pragma once

#include <functional>
#include <string>

#include "scripts/button/Button.h"

// Botão só de texto, sem placa nem borda (menu e cena de idioma): o rótulo centrado no
// retângulo, cor "buttonText.text" e, com hover/foco/pressionado, "buttonText.highlight".
// A ação é um callback da cena, como em ButtonAction.
class ButtonText : public Button {
 public:
  using Action = std::function<void(SceneContext&)>;
  ButtonText(std::string label, Action action);

  void OnClick(SceneContext& ctx) override;
  void Render(SDL_Renderer* r) const override;

 private:
  Action action_;
};
