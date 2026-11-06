#pragma once

#include <string>

#include "scripts/button/ButtonText.h"
#include "scripts/flag/Flag.h"

// Item da cena de idioma (docs/idioma.md): ButtonText com a bandeira à esquerda do nome,
// alinhados à esquerda do retângulo. Sem hover/foco a bandeira fica em preto e branco e o
// texto apagado; com hover/foco a bandeira ganha cor e o texto clareia. Ao clicar, define o
// idioma (i18n) e abre o menu.
class ButtonLanguage : public ButtonText {
 public:
  ButtonLanguage(std::string code, std::string name, flag::Country country);
  void Render(SDL_Renderer* r) const override;
  const std::string& Code() const { return code_; }

  // Largura do conteúdo (bandeira + espaço + nome) e altura da linha, para a cena alinhar
  // todos os itens numa mesma coluna.
  float ContentWidth() const;
  static constexpr float RowHeight() { return ui::Grid(4); }

 private:
  std::string code_;
  flag::Country country_;
};
