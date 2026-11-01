#include "scripts/button/ButtonPlay.h"

#include "scenes/Scene.h"

ButtonPlay::ButtonPlay() : Button("Jogar") {}

void ButtonPlay::OnClick(SceneContext& ctx) { ctx.next_scene = SceneId::kBattle; }
