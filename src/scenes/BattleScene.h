#pragma once

#include "scenes/Scene.h"

// Cena de batalha. Por enquanto um marcador: Esc volta ao menu.
class BattleScene : public Scene {
 public:
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Render(SceneContext& ctx) override;
};
