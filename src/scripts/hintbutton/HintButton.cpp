#include "scripts/hintbutton/HintButton.h"

#include "scripts/colors/Colors.h"
#include "scripts/frame/Frame.h"
#include "scripts/text/Text.h"

bool HintButton::Contains(float x, float y) const {
  return x >= rect_.x && x < rect_.x + rect_.w && y >= rect_.y && y < rect_.y + rect_.h;
}

bool HintButton::HandleEvent(const SDL_Event& event) {
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

void HintButton::Render(SDL_Renderer* r, bool enabled) const {
  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &previous);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  // Fundo como os botões de texto: placa de pedra (ou cores "button.*" sem ela).
  if (frame::Ready()) {
    frame::Draw(r, rect_, frame::Kind::kButton, pressed_ ? 0.6f : hovered_ ? 1.0f : 0.8f);
  } else {
    SDL_Color fill = colors::Get(pressed_ ? "button.pressedFill" : hovered_ ? "button.hoverFill" : "button.fill");
    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(r, &rect_);
    SDL_Color border = colors::Get(hovered_ ? "button.hoverBorder" : "button.border");
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
    SDL_RenderRect(r, &rect_);
  }

  // "?" na fonte de título, com ~2/3 da altura do botão.
  const SDL_Color ink = colors::Get(enabled ? "button.text" : "text.muted");
  const float scale = rect_.h * 0.66f / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
  text::DrawCentered(r, "?", rect_.x + rect_.w * 0.5f, rect_.y + rect_.h * 0.5f, scale, ink);

  if (!enabled) {
    // Risco diagonal sobre o "?".
    const SDL_Color x = colors::Get("status.nerf");
    SDL_SetRenderDrawColor(r, x.r, x.g, x.b, x.a);
    for (int k = -1; k <= 1; ++k) {
      const float o = static_cast<float>(k) * 1.5f;
      SDL_RenderLine(r, rect_.x + rect_.w * 0.28f + o, rect_.y + rect_.h * 0.72f,
                     rect_.x + rect_.w * 0.72f + o, rect_.y + rect_.h * 0.28f);
    }
  }
  SDL_SetRenderDrawBlendMode(r, previous);
}
