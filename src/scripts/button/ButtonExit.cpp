#include "scripts/button/ButtonExit.h"

#include "scenes/Scene.h"
#include "scripts/i18n/I18n.h"

// Ação destrutiva (encerra o jogo): placa vermelha.
ButtonExit::ButtonExit() : Button(i18n::Get("menu.exit")) { SetDanger(true); }

void ButtonExit::OnClick(SceneContext& ctx) { ctx.quit = true; }
