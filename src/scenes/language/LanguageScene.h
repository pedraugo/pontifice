#pragma once

#include <memory>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/button/ButtonLanguage.h"

// Primeira cena do jogo (docs/idioma.md): escolha do idioma antes do menu. Fundo liso
// ("background.language") e um item por idioma de assets/translate.json: texto clicável com a
// bandeira à esquerda e o nome no próprio idioma. Clicar (ou Enter) define o idioma e abre o
// menu.
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
  int focused_ = -1;  // -1: nenhum item focado
};
