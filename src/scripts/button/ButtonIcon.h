#pragma once

#include "scripts/button/Button.h"

// Botão quadrado com placa e um ícone desenhado com primitivas do renderer, sem sprite:
// alto-falante (liga/desliga a música) ou "?" (liga/desliga a dica do topo da batalha).
// Inativo (SetActive(false): música desligada, dicas desligadas) o ícone fica esmaecido e
// riscado (X ao lado do alto-falante, risco diagonal sobre o "?"). Mesmos estados visuais dos
// outros botões (hover, pressionado). A cena trata o clique com HandleEvent.
class ButtonIcon : public Button {
 public:
  enum class Icon { kSound, kHint };

  explicit ButtonIcon(Icon icon);

  void SetActive(bool active) { active_ = active; }

 protected:
  void RenderContent(SDL_Renderer* r) const override;

 private:
  void RenderSound(SDL_Renderer* r, SDL_Color ink) const;
  void RenderHint(SDL_Renderer* r, SDL_Color ink) const;

  Icon icon_;
  bool active_ = true;
};
