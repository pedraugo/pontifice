#include "scripts/button/ButtonPlay.h"

#include "scenes/Scene.h"
#include "scripts/i18n/I18n.h"

ButtonPlay::ButtonPlay() : Button(i18n::Get("menu.play")) {}

// Passa pela tela de loading (portão), que prepara a batalha e abre em seguida.
void ButtonPlay::OnClick(SceneContext& ctx) {
  ctx.loading_target = SceneId::kBattle;
  ctx.next_scene = SceneId::kLoading;
}
