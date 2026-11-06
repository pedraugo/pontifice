#include "scripts/dialog/ConfirmDialog.h"

#include <algorithm>

#include "scripts/colors/Colors.h"
#include "scripts/frame/Frame.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

// Window at least 48 x 16 grid cells, 3-cell padding, 14 x 5 buttons in the bottom corners.
constexpr float kMinWidth = ui::Grid(48);
constexpr float kHeight = ui::Grid(16);
constexpr float kPadding = ui::Grid(3);
constexpr float kButtonWidth = ui::Grid(14);
constexpr float kButtonHeight = ui::Grid(5);
constexpr float kQuestionScale = 2.5f * ui::kScale;

}

ConfirmDialog::ConfirmDialog() : no_(""), yes_("") {}

void ConfirmDialog::Open(const std::string& question, const std::string& no_label,
                         const std::string& yes_label, int width, int height,
                         bool yes_danger) {
  question_ = question;
  no_.SetLabel(no_label);
  yes_.SetLabel(yes_label);
  yes_.SetDanger(yes_danger);
  const float text_w = text::Width(question_.c_str(), kQuestionScale);
  const float w = std::max(kMinWidth, ui::SnapUp(text_w + 2 * kPadding));
  panel_ = SDL_FRect{(width - w) / 2.0f, (height - kHeight) / 2.0f, w, kHeight};
  const float button_y = panel_.y + panel_.h - kPadding - kButtonHeight;
  no_.SetRect(SDL_FRect{panel_.x + kPadding, button_y, kButtonWidth, kButtonHeight});
  yes_.SetRect(SDL_FRect{panel_.x + panel_.w - kPadding - kButtonWidth, button_y, kButtonWidth,
                         kButtonHeight});
  // Focus starts on No, the safe answer.
  SetFocus(0);
  open_ = true;
}

void ConfirmDialog::SetFocus(int index) {
  focused_ = index;
  no_.SetFocused(focused_ == 0);
  yes_.SetFocused(focused_ == 1);
}

ConfirmDialog::Result ConfirmDialog::HandleEvent(const SDL_Event& event) {
  if (!open_) return Result::kNone;
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
        return Result::kNo;
      case SDLK_LEFT:
        SetFocus(0);
        return Result::kNone;
      case SDLK_RIGHT:
        SetFocus(1);
        return Result::kNone;
      case SDLK_TAB:
        SetFocus(1 - focused_);
        return Result::kNone;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
      case SDLK_SPACE:
        return focused_ == 0 ? Result::kNo : Result::kYes;
      default:
        return Result::kNone;
    }
  }
  if (no_.HandleEvent(event)) return Result::kNo;
  if (yes_.HandleEvent(event)) return Result::kYes;
  return Result::kNone;
}

void ConfirmDialog::Render(SDL_Renderer* r, int width, int height) const {
  if (!open_) return;
  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &previous);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  SDL_Color c = colors::Get("background.overlay");
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  const SDL_FRect full{0, 0, static_cast<float>(width), static_cast<float>(height)};
  SDL_RenderFillRect(r, &full);

  frame::Draw(r, panel_, frame::Kind::kWindow);
  const float top = panel_.y + kPadding;
  const float bottom = panel_.y + panel_.h - kPadding - kButtonHeight;
  const float question_y = (top + bottom) / 2.0f;
  text::DrawCentered(r, question_.c_str(), panel_.x + panel_.w / 2.0f, question_y,
                     kQuestionScale, colors::Get("panel.title"));
  no_.Render(r);
  yes_.Render(r);

  SDL_SetRenderDrawBlendMode(r, previous);
}
