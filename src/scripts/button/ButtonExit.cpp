#include "scripts/button/ButtonExit.h"

#include "scenes/Scene.h"

ButtonExit::ButtonExit() : Button("Sair") {}

void ButtonExit::OnClick(SceneContext& ctx) { ctx.quit = true; }
