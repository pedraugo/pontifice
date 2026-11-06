#include "scripts/frame/Frame.h"

#include <algorithm>

#include "scripts/colors/Colors.h"

namespace frame {

namespace {

SDL_FRect Shrink(const SDL_FRect& rect, float by) {
  return SDL_FRect{rect.x + by, rect.y + by, rect.w - 2.0f * by, rect.h - 2.0f * by};
}

void Fill(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c, float alpha) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, static_cast<Uint8>(c.a * alpha));
  SDL_RenderFillRect(r, &rect);
}

// Anel de `thickness` px rente à borda de `outer`, em quatro faixas (sem sobrepor, para o alpha
// não somar nos cantos).
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

}  // namespace

SDL_FRect Inner(const SDL_FRect& rect) { return Shrink(rect, kInset); }

void Draw(SDL_Renderer* r, const SDL_FRect& rect, Kind kind, State state, float alpha) {
  if (rect.w <= 0.0f || rect.h <= 0.0f) return;
  alpha = std::clamp(alpha, 0.0f, 1.0f);
  // Placas menores que duas vezes a margem encolhem outline e borda na proporção.
  const float shrink = std::min(1.0f, std::min(rect.w, rect.h) / (2.0f * kInset));
  const float outline = kOutline * shrink;
  const float border = kBorder * shrink;

  const bool danger = kind == Kind::kButtonDanger;
  const bool pressed = state == State::kPressed;
  const char* fill = danger ? (pressed ? "frame.dangerFillPressed" : "frame.dangerFill")
                            : (pressed ? "frame.fillPressed" : "frame.fill");
  const char* edge = state == State::kHighlight ? "frame.borderHighlight" : "frame.border";

  Ring(r, rect, outline, colors::Get("frame.outline"), alpha);
  const SDL_FRect border_rect = Shrink(rect, outline);
  Ring(r, border_rect, border, colors::Get(edge), alpha);
  Fill(r, Shrink(border_rect, border), colors::Get(fill), alpha);
}

}  // namespace frame
