#include "scenes/menu/menuScene.h"

#include <algorithm>
#include <string>

#include "scripts/ai/ai.h"
#include "scripts/button/buttonText.h"
#include "scripts/colors/colors.h"
#include "scripts/i18n/i18n.h"
#include "scripts/audios/music.h"
#include "scripts/text/text.h"
#include "scripts/ui/ui.h"

namespace {

constexpr float kMenuFadeOut = 0.5f;
constexpr float kLoadingFadeIn = 0.5f;

std::string DifficultyLabel(int level) {
  const auto difficulty = static_cast<ai::Difficulty>(std::clamp(level, 0, 3));
  return i18n::Format("menu.level", {i18n::Get(ai::DifficultyKey(difficulty))});
}

}

MenuScene::MenuScene() {
  buttons_.push_back(std::make_unique<ButtonText>(i18n::Get("menu.play"), [](SceneContext& ctx) {
    ctx.loading_target = SceneId::kBattle;
    ctx.fade_out_seconds = kMenuFadeOut;
    ctx.fade_in_seconds = kLoadingFadeIn;
    ctx.next_scene = SceneId::kLoading;
  }));
  auto difficulty = std::make_unique<ButtonText>(
      DifficultyLabel(kDefaultDifficulty), [this](SceneContext& ctx) {
        ctx.difficulty = static_cast<int>(
            ai::NextDifficulty(static_cast<ai::Difficulty>(std::clamp(ctx.difficulty, 0, 3))));
        difficulty_button_->SetLabel(DifficultyLabel(ctx.difficulty));
        LayoutButtons(ctx);
      });
  difficulty_button_ = difficulty.get();
  buttons_.push_back(std::move(difficulty));
  buttons_.push_back(std::make_unique<ButtonText>(i18n::Get("menu.exit"),
                                                  [](SceneContext& ctx) { ctx.quit = true; }));
}

void MenuScene::OnEnter(SceneContext& ctx) {
  difficulty_button_->SetLabel(DifficultyLabel(ctx.difficulty));
  LayoutButtons(ctx);
  const float icon_size = button::Cells("icon.size", 3.0f);
  const float icon_margin = button::Cells("icon.margin", 1.0f);
  sound_button_.SetRect(SDL_FRect{icon_margin, ctx.logical_height - icon_margin - icon_size,
                                  icon_size, icon_size});
  focused_ = -1;
  for (auto& button : buttons_) button->SetFocused(false);
}

void MenuScene::LayoutButtons(const SceneContext& ctx) {
  const float row_h = button::Cells("menu.rowHeight", 4.0f);
  const float row_gap = button::Cells("menu.rowGap", 2.0f);
  const float total = buttons_.size() * row_h + (buttons_.size() - 1) * row_gap;
  float y = ui::SnapUp((ctx.logical_height - total) / 2.0f);
  for (auto& button : buttons_) {
    const float w = button->LabelWidth();
    button->SetRect(SDL_FRect{(ctx.logical_width - w) / 2.0f, y, w, row_h});
    y += row_h + row_gap;
  }
}

void MenuScene::MoveFocus(int delta) {
  const int count = static_cast<int>(buttons_.size());
  if (focused_ < 0) {
    focused_ = delta > 0 ? 0 : count - 1;
  } else {
    focused_ = (focused_ + delta + count) % count;
  }
  for (int i = 0; i < count; ++i) buttons_[i]->SetFocused(i == focused_);
}

void MenuScene::ActivateFocused(SceneContext& ctx) {
  if (focused_ >= 0) buttons_[focused_]->OnClick(ctx);
}

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

  if (sound_button_.HandleEvent(event)) {
    if (ctx.music != nullptr) ctx.music->ToggleMuted();
    return;
  }
  for (size_t i = 0; i < buttons_.size(); ++i) {
    if (buttons_[i]->HandleEvent(event)) {
      focused_ = static_cast<int>(i);
      buttons_[i]->OnClick(ctx);
      return;
    }
  }
}

void MenuScene::Update(SceneContext&, float) {}

void MenuScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color background = colors::Get("background.menu");
  SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
  SDL_RenderClear(r);

  const float center_x = ctx.logical_width / 2.0f;
  text::DrawCentered(r, "PONTIFICE", center_x, ui::Cells("menu.titleCenterY", 12.0f),
                     ui::TextScale("menu.title", 8.0f),
                     colors::Get("text.title"));

  for (auto& button : buttons_) button->Render(r);
  sound_button_.SetActive(ctx.music == nullptr || !ctx.music->Muted());
  sound_button_.Render(r);

  text::DrawCentered(r, i18n::Get("menu.credits").c_str(), center_x,
                     ctx.logical_height - ui::Cells("menu.footerBottom", 3.0f),
                     ui::TextScale("menu.footer", 2.0f),
                     colors::Get("text.secondary"));
}
