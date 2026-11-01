#include "scripts/button/ButtonPlay.h"

#include "scenes/Scene.h"

ButtonPlay::ButtonPlay() : Button("Jogar") {}

// Passa pela tela de loading (portão), que prepara a batalha e abre em seguida.
void ButtonPlay::OnClick(SceneContext& ctx) {
  ctx.loading_target = SceneId::kBattle;
  ctx.next_scene = SceneId::kLoading;
}
