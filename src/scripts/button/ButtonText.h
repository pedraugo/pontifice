#pragma once

#include <functional>
#include <optional>
#include <string>

#include "scripts/button/Button.h"
#include "scripts/flag/Flag.h"

// Botão só de texto, sem placa nem borda (menu e cena de idioma): o rótulo centrado no
// retângulo, cor "buttonText.text" e, com hover/foco/pressionado, "buttonText.highlight".
// Com bandeira (SetFlag; item da cena de idioma, docs/idioma.md), bandeira e rótulo ficam
// alinhados à esquerda do retângulo: sem hover/foco a bandeira fica em preto e branco e o
// texto apagado; com hover/foco a bandeira ganha cor e o texto clareia.
class ButtonText : public Button {
 public:
  ButtonText(std::string label, Action action);

  void SetFlag(flag::Country country) { flag_ = country; }
  // Largura do conteúdo (bandeira + espaço + rótulo, ou só o rótulo), para a cena alinhar
  // os itens numa mesma coluna.
  float ContentWidth() const;

  void Render(SDL_Renderer* r) const override;

 private:
  std::optional<flag::Country> flag_;
};
