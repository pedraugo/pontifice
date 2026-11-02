#include "scenes/menu/MenuScene.h"

#include <algorithm>
#include <string>

#include "scripts/ai/Ai.h"
#include "scripts/button/ButtonAction.h"
#include "scripts/button/ButtonExit.h"
#include "scripts/button/ButtonPlay.h"
#include "scripts/colors/Colors.h"
#include "scripts/music/Music.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

constexpr float kButtonWidth = ui::Px(260.0f);
constexpr float kButtonHeight = ui::Px(56.0f);
constexpr float kButtonGap = ui::Px(24.0f);
constexpr float kTitleScale = 6.0f * ui::kScale;
constexpr float kFooterScale = 2.0f * ui::kScale;
constexpr float kSoundButtonSize = ui::Px(40.0f);
constexpr float kSoundMargin = ui::Px(16.0f);

std::string DifficultyLabel(int level) {
  return std::string("Nível: ") +
         ai::DifficultyName(static_cast<ai::Difficulty>(std::clamp(level, 0, 3)));
}

}  // namespace

MenuScene::MenuScene() {
  buttons_.push_back(std::make_unique<ButtonPlay>());
  // Nível do oponente (docs/menu.md): cada clique passa ao próximo; vale para a próxima batalha.
  auto difficulty = std::make_unique<ButtonAction>(
      DifficultyLabel(kDefaultDifficulty), [this](SceneContext& ctx) {
        ctx.difficulty = static_cast<int>(
            ai::NextDifficulty(static_cast<ai::Difficulty>(std::clamp(ctx.difficulty, 0, 3))));
        difficulty_button_->SetLabel(DifficultyLabel(ctx.difficulty));
      });
  difficulty_button_ = difficulty.get();
  buttons_.push_back(std::move(difficulty));
  buttons_.push_back(std::make_unique<ButtonExit>());
}

void MenuScene::OnEnter(SceneContext& ctx) {
  difficulty_button_->SetLabel(DifficultyLabel(ctx.difficulty));
  LayoutButtons(ctx);
  sound_button_.SetRect(SDL_FRect{kSoundMargin, ctx.logical_height - kSoundMargin - kSoundButtonSize,
                                  kSoundButtonSize, kSoundButtonSize});
  clouds_ready_ = clouds_.Init(ctx.renderer, ctx.logical_width, ctx.logical_height);
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

void MenuScene::Update(SceneContext& /*ctx*/, float dt_seconds) {
  if (clouds_ready_) clouds_.Update(dt_seconds);
}

void MenuScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color background = colors::Get("background.menu");
  SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
  SDL_RenderClear(r);
  if (clouds_ready_) clouds_.RenderBack(r);

  const float center_x = ctx.logical_width / 2.0f;
  text::DrawCentered(r, "PONTIFICE", center_x, ctx.logical_height * 0.22f, kTitleScale,
                     colors::Get("text.title"));

  for (auto& button : buttons_) button->Render(r);
  sound_button_.Render(r, ctx.music != nullptr && ctx.music->Muted());

  text::DrawCentered(r, "Criado por pedraugo", center_x, ctx.logical_height - ui::Px(48.0f),
                     kFooterScale, colors::Get("text.secondary"));
  if (clouds_ready_) clouds_.RenderFront(r);  // névoa rala por cima do texto
}
