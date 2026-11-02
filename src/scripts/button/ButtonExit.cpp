#include "scripts/button/ButtonExit.h"

#include "scenes/Scene.h"
#include "scripts/i18n/I18n.h"

ButtonExit::ButtonExit() : Button(i18n::Get("menu.exit")) {}

void ButtonExit::OnClick(SceneContext& ctx) { ctx.quit = true; }
