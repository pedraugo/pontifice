#pragma once

#include <SDL3/SDL.h>

#include <memory>

#include "scenes/Scene.h"

// Mantém a cena ativa e executa as trocas pedidas via SceneContext. Se a troca vier com
// fade_out_seconds, a cena atual escurece até o preto antes de sair; se vier com
// fade_in_seconds, desenha um véu preto que clareia sobre a nova cena.
class SceneManager {
 public:
  explicit SceneManager(SceneContext ctx);

  void SwitchTo(SceneId id);
  void HandleEvent(const SDL_Event& event);
  void Update(float dt_seconds);
  void Render();

  bool ShouldQuit() const { return ctx_.quit; }
  void RequestQuit() { ctx_.quit = true; }

  // Fábrica de cenas; a tela de loading a usa para preparar a próxima cena.
  static std::unique_ptr<Scene> Create(SceneId id);

 private:
  void ApplyPendingSwitch();

  SceneContext ctx_;
  std::unique_ptr<Scene> current_;
  float fade_in_total_ = 0.0f;
  float fade_in_left_ = 0.0f;
  float fade_out_total_ = 0.0f;  // > 0 enquanto a cena atual escurece para a troca
  float fade_out_left_ = 0.0f;
};
