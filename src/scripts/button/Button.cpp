#include "scripts/button/Button.h"

#include "scripts/text/Text.h"

namespace {

constexpr SDL_Color kForeground{230, 220, 200, 255};
constexpr SDL_Color kHoverFill{60, 50, 80, 255};
constexpr SDL_Color kPressedFill{90, 75, 120, 255};
constexpr float kLabelScale = 3.0f;

void FillRect(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(r, &rect);
}

void StrokeRect(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c, int thickness) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  for (int i = 0; i < thickness; ++i) {
    const float f = static_cast<float>(i);
    const SDL_FRect inner{rect.x + f, rect.y + f, rect.w - 2 * f, rect.h - 2 * f};
    SDL_RenderRect(r, &inner);
  }
}

}  // namespace

Button::Button(std::string label) : label_(std::move(label)) {}

bool Button::Contains(float x, float y) const {
  return x >= rect_.x && x < rect_.x + rect_.w && y >= rect_.y && y < rect_.y + rect_.h;
}

bool Button::HandleEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
      hovered_ = Contains(event.motion.x, event.motion.y);
      return false;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      if (event.button.button == SDL_BUTTON_LEFT && Contains(event.button.x, event.button.y)) {
        pressed_ = true;
      }
      return false;
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      if (event.button.button != SDL_BUTTON_LEFT) return false;
      const bool clicked = pressed_ && Contains(event.button.x, event.button.y);
      pressed_ = false;
      return clicked;
    }
    default:
      return false;
  }
}

void Button::Render(SDL_Renderer* r) const {
  const bool highlight = hovered_ || focused_;
  if (pressed_) {
    FillRect(r, rect_, kPressedFill);
  } else if (highlight) {
    FillRect(r, rect_, kHoverFill);
  }
  StrokeRect(r, rect_, kForeground, highlight ? 3 : 1);
  text::DrawCentered(r, label_.c_str(), rect_.x + rect_.w / 2.0f, rect_.y + rect_.h / 2.0f,
                     kLabelScale, kForeground);
}
