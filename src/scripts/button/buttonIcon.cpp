#include "scripts/button/buttonIcon.h"

#include <algorithm>

#include "scripts/colors/colors.h"
#include "scripts/draw/draw.h"
#include "scripts/text/text.h"

ButtonIcon::ButtonIcon(Icon icon) : Button(""), icon_(icon) {}

std::string ButtonIcon::VariantKey() const {
  return Danger() ? "buttonIcon.danger" : std::string("buttonIcon.") + SizeName();
}

void ButtonIcon::RenderContent(SDL_Renderer* r) const {
  const SDL_Color ink = colors::Get(active_ ? "silver" : "slate");
  const SDL_FRect& full = Rect();
  // Fraction of the button size kept clear between the frame and the icon, per side.
  const float pad = std::min(full.w, full.h) * MeasureNumber("padding", 0.12f);
  const SDL_FRect rect{full.x + pad, full.y + pad, full.w - 2.0f * pad, full.h - 2.0f * pad};
  switch (icon_) {
    case Icon::kSound:
      RenderSound(r, rect, ink);
      break;
    case Icon::kHint:
      RenderHint(r, rect, ink);
      break;
    case Icon::kExit:
      RenderExit(r, rect, ink);
      break;
  }
}

void ButtonIcon::RenderSound(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const {
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
    const SDL_Color x = colors::Get("red");
    draw::Line(r, P(0.60f, 0.38f), P(0.80f, 0.62f), x, 3.0f);
    draw::Line(r, P(0.80f, 0.38f), P(0.60f, 0.62f), x, 3.0f);
  }
}

void ButtonIcon::RenderHint(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const {
  // "?" at about 2/3 of the button height.
  const float scale = rect.h * 0.66f / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
  text::DrawCentered(r, "?", rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, scale, ink);
  if (!active_) {
    draw::Line(r, {rect.x + rect.w * 0.28f, rect.y + rect.h * 0.72f},
               {rect.x + rect.w * 0.72f, rect.y + rect.h * 0.28f}, colors::Get("red"),
               3.0f);
  }
}

void ButtonIcon::RenderExit(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const {
  // Door frame open on the right, arrow leaving through it.
  auto P = [&](float u, float v) { return SDL_FPoint{rect.x + u * rect.w, rect.y + v * rect.h}; };
  const float t = rect.w * 0.07f;
  draw::Line(r, P(0.46f, 0.22f), P(0.20f, 0.22f), ink, t);
  draw::Line(r, P(0.20f, 0.22f), P(0.20f, 0.78f), ink, t);
  draw::Line(r, P(0.20f, 0.78f), P(0.46f, 0.78f), ink, t);
  draw::Line(r, P(0.38f, 0.50f), P(0.82f, 0.50f), ink, t);
  const SDL_FPoint head[3] = {P(0.68f, 0.34f), P(0.86f, 0.50f), P(0.68f, 0.66f)};
  draw::FillPolygon(r, head, 3, ink);
}
