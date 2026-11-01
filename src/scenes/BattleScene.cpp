#include "scenes/BattleScene.h"

#include "scripts/Text.h"

void BattleScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
    ctx.next_scene = SceneId::kMenu;
  }
}

void BattleScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  SDL_SetRenderDrawColor(r, 10, 20, 14, 255);
  SDL_RenderClear(r);

  const SDL_Color fg{230, 220, 200, 255};
  const float cx = ctx.logical_width / 2.0f;
  text::DrawCentered(r, "BATALHA", cx, ctx.logical_height * 0.4f, 5.0f, fg);
  text::DrawCentered(r, "Em construcao. Esc volta ao menu.", cx, ctx.logical_height * 0.55f, 2.0f,
                     fg);
}
