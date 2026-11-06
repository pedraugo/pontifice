#pragma once

#include <memory>
#include <string>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/button/ButtonText.h"

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
  void AddLanguage(const std::string& code, const std::string& name, flag::Country country);
  void LayoutButtons(const SceneContext& ctx);
  void MoveFocus(int delta);

  std::vector<std::unique_ptr<ButtonText>> buttons_;
  int focused_ = -1;  // -1: nenhum item focado
};
