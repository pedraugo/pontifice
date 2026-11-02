#pragma once

#include <string>

#include "scripts/button/Button.h"
#include "scripts/flag/Flag.h"

// Botão da cena de idioma (docs/idioma.md): bandeira à esquerda e o nome do idioma, os dois
// centralizados como um bloco. Ao clicar, define o idioma (i18n) e abre o menu.
class ButtonLanguage : public Button {
 public:
  ButtonLanguage(std::string code, std::string name, flag::Country country);
  void OnClick(SceneContext& ctx) override;
  const std::string& Code() const { return code_; }

 protected:
  void RenderContent(SDL_Renderer* r) const override;

 private:
  std::string code_;
  flag::Country country_;
};
