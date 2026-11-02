#pragma once

#include <memory>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/button/ButtonLanguage.h"
#include "scripts/clouds/Clouds.h"

// Primeira cena do jogo (docs/idioma.md): escolha do idioma antes do menu. Só o fundo de
// nuvens do menu (sem ícone nem logo) e um botão por idioma de assets/translate.json, com a
// bandeira e o nome no próprio idioma. Clicar (ou Enter) define o idioma e abre o menu.
class LanguageScene : public Scene {
 public:
  LanguageScene();

  void OnEnter(SceneContext& ctx) override;
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;

 private:
  void LayoutButtons(const SceneContext& ctx);
  void MoveFocus(int delta);

  std::vector<std::unique_ptr<ButtonLanguage>> buttons_;
  Clouds clouds_;
  bool clouds_ready_ = false;
  int focused_ = 0;
};
