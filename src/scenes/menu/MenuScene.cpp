#include "scenes/menu/MenuScene.h"

#include "scripts/button/ButtonExit.h"
#include "scripts/button/ButtonPlay.h"
#include "scripts/text/Text.h"

namespace {

constexpr SDL_Color kBackground{13, 10, 20, 255};
constexpr SDL_Color kForeground{230, 220, 200, 255};

constexpr float kButtonWidth = 260.0f;
constexpr float kButtonHeight = 56.0f;
constexpr float kButtonGap = 24.0f;
constexpr float kTitleScale = 6.0f;
constexpr float kFooterScale = 2.0f;

}  // namespace

MenuScene::MenuScene() {
  buttons_.push_back(std::make_unique<ButtonPlay>());
  buttons_.push_back(std::make_unique<ButtonExit>());
}

void MenuScene::OnEnter(SceneContext& ctx) {
  LayoutButtons(ctx);
  focused_ = 0;
  for (size_t i = 0; i < buttons_.size(); ++i) buttons_[i]->SetFocused(i == 0);
}

void MenuScene::LayoutButtons(const SceneContext& ctx) {
  const float total = buttons_.size() * kButtonHeight + (buttons_.size() - 1) * kButtonGap;
  const float x = (ctx.logical_width - kButtonWidth) / 2.0f;
  float y = (ctx.logical_height - total) / 2.0f;
  for (auto& button : buttons_) {
    button->SetRect(SDL_FRect{x, y, kButtonWidth, kButtonHeight});
    y += kButtonHeight + kButtonGap;
  }
}

void MenuScene::MoveFocus(int delta) {
  const int count = static_cast<int>(buttons_.size());
  focused_ = (focused_ + delta + count) % count;
  for (int i = 0; i < count; ++i) buttons_[i]->SetFocused(i == focused_);
}

void MenuScene::ActivateFocused(SceneContext& ctx) { buttons_[focused_]->OnClick(ctx); }

void MenuScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_UP:
        MoveFocus(-1);
        return;
      case SDLK_DOWN:
        MoveFocus(+1);
        return;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
      case SDLK_SPACE:
        ActivateFocused(ctx);
        return;
      case SDLK_ESCAPE:
        ctx.quit = true;
        return;
      default:
        break;
    }
  }

  for (size_t i = 0; i < buttons_.size(); ++i) {
    if (buttons_[i]->HandleEvent(event)) {
      focused_ = static_cast<int>(i);
      buttons_[i]->OnClick(ctx);
      return;
    }
  }
}

void MenuScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  SDL_SetRenderDrawColor(r, kBackground.r, kBackground.g, kBackground.b, kBackground.a);
  SDL_RenderClear(r);

  const float center_x = ctx.logical_width / 2.0f;
  text::DrawCentered(r, "PONTIFICE", center_x, ctx.logical_height * 0.22f, kTitleScale,
                     kForeground);

  for (auto& button : buttons_) button->Render(r);

  text::DrawCentered(r, "Criado por pedraugo", center_x, ctx.logical_height - 48.0f,
                     kFooterScale, kForeground);
}
