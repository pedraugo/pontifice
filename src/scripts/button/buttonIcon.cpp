#include "scripts/button/buttonIcon.h"

#include <algorithm>

#include "scripts/colors/colors.h"
#include "scripts/config/config.h"
#include "scripts/draw/draw.h"
#include "scripts/frame/frame.h"
#include "scripts/text/text.h"
#include "scripts/ui/ui.h"

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
    case Icon::kFlag:
      flag::Draw(r, flag_, rect);
      break;
    case Icon::kLevel:
      RenderLevel(r, rect, ink);
      break;
  }
}

void ButtonIcon::RenderTooltip(SDL_Renderer* r) const {
  if (!Hovered() || tooltip_.empty()) return;
  const text::Style style =
      text::StyleOf(config::String("button", "buttonIcon.tooltip.label", "small+body"));
  const float gap = button::Cells("buttonIcon.tooltip.gap", 0.5f);
  const float padding = button::Cells("buttonIcon.tooltip.padding", 1.0f);
  const SDL_FRect& b = Rect();
  const float w = ui::SnapUp(text::Width(tooltip_.c_str(), style) + 2.0f * padding);
  const SDL_FRect panel{b.x + b.w + gap, b.y, w, b.h};
  frame::Draw(r, panel, frame::Kind::kWindow);
  text::DrawCentered(r, tooltip_.c_str(), panel.x + panel.w / 2.0f, panel.y + panel.h / 2.0f,
                     style, colors::Get("silver"));
}

void ButtonIcon::RenderLevel(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const {
  // Four rising bars; Impossible fills them all in red.
  constexpr int kBars = 4;
  const float gap = rect.w * 0.08f;
  const float bar_w = (rect.w * 0.8f - gap * (kBars - 1)) / kBars;
  const float bottom = rect.y + rect.h * 0.82f;
  const SDL_Color filled = level_ >= kBars - 1 ? colors::Get("red") : ink;
  const SDL_Color empty = colors::Get("slate");
  for (int i = 0; i < kBars; ++i) {
    const float h = rect.h * (0.2f + 0.16f * static_cast<float>(i));
    const SDL_FRect bar{rect.x + rect.w * 0.1f + static_cast<float>(i) * (bar_w + gap), bottom - h,
                        bar_w, h};
    const SDL_Color c = i <= level_ ? filled : empty;
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &bar);
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
