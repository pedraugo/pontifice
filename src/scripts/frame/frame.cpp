#include "scripts/frame/frame.h"

#include <algorithm>

#include "scripts/colors/colors.h"

namespace frame {

namespace {

struct Box {
  float u0, v0, u1, v1;
};
// Corner knot measured on art/corner.png (3 px line), local px from the outer corner.
constexpr float kLine = 3.0f;
constexpr float kKnotSize = 17.0f;
// u/v grow inward; boxes must not overlap (alpha would add up).
constexpr Box kKnot[] = {
    {0.0f, 3.0f, 3.0f, 10.0f},  // left tail, bottom bar, stem, top bar, right tail
    {0.0f, 0.0f, 7.0f, 3.0f},
    {7.0f, 0.0f, 10.0f, 17.0f},
    {0.0f, 14.0f, 7.0f, 17.0f},
    {14.0f, 0.0f, 17.0f, 10.0f},
};

SDL_FRect Shrink(const SDL_FRect& rect, float by) {
  return SDL_FRect{rect.x + by, rect.y + by, rect.w - 2.0f * by, rect.h - 2.0f * by};
}

void Fill(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c, float alpha) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, static_cast<Uint8>(c.a * alpha));
  SDL_RenderFillRect(r, &rect);
}

void Ring(SDL_Renderer* r, const SDL_FRect& outer, float thickness, SDL_Color c, float alpha) {
  const SDL_FRect top{outer.x, outer.y, outer.w, thickness};
  const SDL_FRect bottom{outer.x, outer.y + outer.h - thickness, outer.w, thickness};
  const SDL_FRect left{outer.x, outer.y + thickness, thickness, outer.h - 2.0f * thickness};
  const SDL_FRect right{outer.x + outer.w - thickness, outer.y + thickness, thickness,
                        outer.h - 2.0f * thickness};
  Fill(r, top, c, alpha);
  Fill(r, bottom, c, alpha);
  Fill(r, left, c, alpha);
  Fill(r, right, c, alpha);
}

void Knot(SDL_Renderer* r, float cx, float cy, float su, float sv, float s, SDL_Color line) {
  for (const Box& b : kKnot) {
    const float xa = cx + su * b.u0 * s, xb = cx + su * b.u1 * s;
    const float ya = cy + sv * b.v0 * s, yb = cy + sv * b.v1 * s;
    Fill(r, SDL_FRect{std::min(xa, xb), std::min(ya, yb), (b.u1 - b.u0) * s, (b.v1 - b.v0) * s},
         line, 1.0f);
  }
}

void BorderWithKnots(SDL_Renderer* r, const SDL_FRect& edge, float border, SDL_Color line) {
  // Scale 1 = 3 px line; smaller on tiny plates so two knots fit per side.
  const float s = std::min(border / kLine, std::min(edge.w, edge.h) / (2.0f * kKnotSize));
  const float knot = kKnotSize * s;
  const float thick = kLine * s;
  const float x0 = edge.x, x1 = edge.x + edge.w, y0 = edge.y, y1 = edge.y + edge.h;
  Knot(r, x0, y1, +1.0f, -1.0f, s, line);
  Knot(r, x0, y0, +1.0f, +1.0f, s, line);
  Knot(r, x1, y1, -1.0f, -1.0f, s, line);
  Knot(r, x1, y0, -1.0f, +1.0f, s, line);
  const float span_w = edge.w - 2.0f * knot;
  const float span_h = edge.h - 2.0f * knot;
  if (span_w > 0.0f) {
    Fill(r, SDL_FRect{x0 + knot, y0, span_w, thick}, line, 1.0f);
    Fill(r, SDL_FRect{x0 + knot, y1 - thick, span_w, thick}, line, 1.0f);
  }
  if (span_h > 0.0f) {
    Fill(r, SDL_FRect{x0, y0 + knot, thick, span_h}, line, 1.0f);
    Fill(r, SDL_FRect{x1 - thick, y0 + knot, thick, span_h}, line, 1.0f);
  }
}

}

SDL_FRect Inner(const SDL_FRect& rect) { return Shrink(rect, kInset); }

void Draw(SDL_Renderer* r, const SDL_FRect& rect, Kind kind, State state, float alpha) {
  if (rect.w <= 0.0f || rect.h <= 0.0f) return;
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  // Plates smaller than twice the inset shrink outline and border proportionally.
  const float shrink = std::min(1.0f, std::min(rect.w, rect.h) / (2.0f * kInset));
  const float outline = kOutline * shrink;
  const float border = kBorder * shrink;

  const bool danger = kind == Kind::kButtonDanger;
  const bool pressed = state == State::kPressed;
  const char* fill = danger ? (pressed ? "frame.dangerFillPressed" : "frame.dangerFill")
                            : (pressed ? "frame.fillPressed" : "frame.fill");
  SDL_Color line = colors::Get(state == State::kHighlight ? "frame.borderHighlight" : "frame.border");
  line.a = static_cast<Uint8>(line.a * alpha);

  Ring(r, rect, outline, colors::Get("frame.outline"), alpha);
  const SDL_FRect edge = Shrink(rect, outline);
  Fill(r, edge, colors::Get(fill), alpha);  // fill also runs under the border
  BorderWithKnots(r, edge, border, line);
}

}
