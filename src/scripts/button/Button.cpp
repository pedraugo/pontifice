#include "scripts/button/Button.h"

#include "scripts/colors/Colors.h"
#include "scripts/frame/Frame.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

constexpr float kLabelScale = 3.0f * ui::kScale;
constexpr int kBorderThin = 1;
constexpr int kBorderThick = static_cast<int>(3.0f * ui::kScale + 0.5f);

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
  // Cores de assets/colors.json (papéis "button.*"); o preenchimento normal pode ter alpha.
  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &previous);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  const bool highlight = hovered_ || focused_;
  if (frame::Ready()) {
    // Placa de pedra: normal um pouco escurecida, destaque na cor cheia, pressionado escuro.
    frame::Draw(r, rect_, frame::Kind::kButton, pressed_ ? 0.6f : highlight ? 1.0f : 0.8f);
    if (focused_) StrokeRect(r, rect_, colors::Get("button.focusBorder"), kBorderThin);
  } else {
    if (pressed_) {
      FillRect(r, rect_, colors::Get("button.pressedFill"));
    } else if (highlight) {
      FillRect(r, rect_, colors::Get("button.hoverFill"));
    } else {
      FillRect(r, rect_, colors::Get("button.fill"));
    }
    const char* border = focused_ ? "button.focusBorder"
                         : hovered_ ? "button.hoverBorder"
                                    : "button.border";
    StrokeRect(r, rect_, colors::Get(border), highlight ? kBorderThick : kBorderThin);
  }
  text::DrawCentered(r, label_.c_str(), rect_.x + rect_.w / 2.0f, rect_.y + rect_.h / 2.0f,
                     kLabelScale, colors::Get("button.text"));

  SDL_SetRenderDrawBlendMode(r, previous);
}
