#include "scripts/button/button.h"

#include "scripts/colors/colors.h"
#include "scripts/config/config.h"
#include "scripts/frame/frame.h"
#include "scripts/text/text.h"
#include "scripts/ui/ui.h"

namespace button {

float Cells(const std::string& path, float fallback_cells) {
  return ui::Grid(config::Float("button", path, fallback_cells));
}

float Number(const std::string& path, float fallback) {
  return config::Float("button", path, fallback);
}

}

namespace {

constexpr int kBorderThin = 1;

void StrokeRect(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c, int thickness) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  for (int i = 0; i < thickness; ++i) {
    const float f = static_cast<float>(i);
    const SDL_FRect inner{rect.x + f, rect.y + f, rect.w - 2 * f, rect.h - 2 * f};
    SDL_RenderRect(r, &inner);
  }
}

}

Button::Button(std::string label, Action action)
    : label_(std::move(label)), action_(std::move(action)) {}

void Button::OnClick(SceneContext& ctx) {
  if (action_) action_(ctx);
}

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

float Button::LabelScale() {
  static const float scale = ui::TextScale("button.label", 2.0f);
  return scale;
}

float Button::LabelWidth() const { return text::Width(label_.c_str(), LabelScale()); }

void Button::Render(SDL_Renderer* r) const {
  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &previous);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  const frame::State state = pressed_                 ? frame::State::kPressed
                             : (hovered_ || focused_) ? frame::State::kHighlight
                                                      : frame::State::kNormal;
  frame::Draw(r, rect_, danger_ ? frame::Kind::kButtonDanger : frame::Kind::kButton, state);
  if (focused_) {
    // Keyboard focus adds a thin stroke around the fill.
    StrokeRect(r, frame::Inner(rect_), colors::Get("button.focusBorder"), kBorderThin);
  }
  RenderContent(r);

  SDL_SetRenderDrawBlendMode(r, previous);
}

void Button::RenderContent(SDL_Renderer* r) const {
  text::DrawCentered(r, label_.c_str(), rect_.x + rect_.w / 2.0f, rect_.y + rect_.h / 2.0f,
                     LabelScale(), colors::Get("button.text"));
}
