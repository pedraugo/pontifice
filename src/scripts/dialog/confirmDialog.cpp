#include "scripts/dialog/confirmDialog.h"

#include <algorithm>

#include "scripts/colors/colors.h"
#include "scripts/frame/frame.h"
#include "scripts/text/text.h"
#include "scripts/ui/ui.h"

namespace {

// Window and padding from ui.json ("dialog"), buttons from button.json ("dialog") in the
// bottom corners, question size from text.json.
struct Layout {
  float min_w = ui::Cells("dialog.minWidth", 48.0f);
  float h = ui::Cells("dialog.height", 16.0f);
  float padding = ui::Cells("dialog.padding", 3.0f);
  float question_scale = ui::TextScale("dialog.question", 2.5f);
};

const Layout& L() {
  static const Layout layout;
  return layout;
}

}

ConfirmDialog::ConfirmDialog() : no_(""), yes_("") {}

void ConfirmDialog::Open(const std::string& question, const std::string& no_label,
                         const std::string& yes_label, int width, int height,
                         bool yes_danger) {
  question_ = question;
  no_.SetLabel(no_label);
  yes_.SetLabel(yes_label);
  yes_.SetDanger(yes_danger);
  const float text_w = text::Width(question_.c_str(), L().question_scale);
  const float w = std::max(L().min_w, ui::SnapUp(text_w + 2 * L().padding));
  panel_ = SDL_FRect{(width - w) / 2.0f, (height - L().h) / 2.0f, w, L().h};
  const float button_w = button::Cells("dialog.width", 14.0f);
  const float button_h = button::Cells("dialog.height", 3.5f);
  const float button_y = panel_.y + panel_.h - L().padding - button_h;
  no_.SetRect(SDL_FRect{panel_.x + L().padding, button_y, button_w, button_h});
  yes_.SetRect(SDL_FRect{panel_.x + panel_.w - L().padding - button_w, button_y, button_w,
                         button_h});
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
  const float top = panel_.y + L().padding;
  const float bottom = no_.Rect().y;
  const float question_y = (top + bottom) / 2.0f;
  text::DrawCentered(r, question_.c_str(), panel_.x + panel_.w / 2.0f, question_y,
                     L().question_scale, colors::Get("panel.title"));
  no_.Render(r);
  yes_.Render(r);

  SDL_SetRenderDrawBlendMode(r, previous);
}
