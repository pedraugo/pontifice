#include "scripts/button/ButtonIcon.h"

#include "scripts/colors/Colors.h"
#include "scripts/draw/Draw.h"
#include "scripts/text/Text.h"

ButtonIcon::ButtonIcon(Icon icon) : Button(""), icon_(icon) {}

void ButtonIcon::RenderContent(SDL_Renderer* r) const {
  const SDL_Color ink = colors::Get(active_ ? "button.text" : "text.muted");
  if (icon_ == Icon::kSound) {
    RenderSound(r, ink);
  } else {
    RenderHint(r, ink);
  }
}

void ButtonIcon::RenderSound(SDL_Renderer* r, SDL_Color ink) const {
  const SDL_FRect& rect = Rect();
  // Icon in 0..1 box coordinates: speaker body, cone, then waves (or an X).
  auto P = [&](float u, float v) { return SDL_FPoint{rect.x + u * rect.w, rect.y + v * rect.h}; };
  const SDL_FPoint body[4] = {P(0.20f, 0.40f), P(0.34f, 0.40f), P(0.34f, 0.60f), P(0.20f, 0.60f)};
  draw::FillPolygon(r, body, 4, ink);
  const SDL_FPoint cone[4] = {P(0.34f, 0.40f), P(0.52f, 0.24f), P(0.52f, 0.76f), P(0.34f, 0.60f)};
  draw::FillPolygon(r, cone, 4, ink);
  if (active_) {
    const SDL_FPoint c = P(0.52f, 0.50f);
    draw::Arc(r, c, rect.w * 0.12f, -0.9f, 0.9f, ink, 2.0f);
    draw::Arc(r, c, rect.w * 0.22f, -0.9f, 0.9f, ink, 2.0f);
  } else {
    const SDL_Color x = colors::Get("status.nerf");
    draw::Line(r, P(0.60f, 0.38f), P(0.80f, 0.62f), x, 3.0f);
    draw::Line(r, P(0.80f, 0.38f), P(0.60f, 0.62f), x, 3.0f);
  }
}

void ButtonIcon::RenderHint(SDL_Renderer* r, SDL_Color ink) const {
  const SDL_FRect& rect = Rect();
  // "?" at about 2/3 of the button height.
  const float scale = rect.h * 0.66f / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
  text::DrawCentered(r, "?", rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, scale, ink);
  if (!active_) {
    draw::Line(r, {rect.x + rect.w * 0.28f, rect.y + rect.h * 0.72f},
               {rect.x + rect.w * 0.72f, rect.y + rect.h * 0.28f}, colors::Get("status.nerf"),
               3.0f);
  }
}
