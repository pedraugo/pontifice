#include "scripts/button/ButtonIcon.h"

#include <cmath>
#include <vector>

#include "scripts/colors/Colors.h"
#include "scripts/text/Text.h"

namespace {

SDL_FColor ToF(SDL_Color c) { return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f}; }

// Polígono convexo preenchido (leque de triângulos), em coordenadas de tela.
void FillPolygon(SDL_Renderer* r, const SDL_FPoint* pts, int n, SDL_Color color) {
  std::vector<SDL_Vertex> v(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = SDL_Vertex{pts[i], ToF(color), {0, 0}};
  std::vector<int> idx;
  for (int i = 1; i + 1 < n; ++i) idx.insert(idx.end(), {0, i, i + 1});
  SDL_RenderGeometry(r, nullptr, v.data(), n, idx.data(), static_cast<int>(idx.size()));
}

// Arco (polilinha) de centro c, raio rad, entre os ângulos a0 e a1 (rad), com 2 px de traço.
void Arc(SDL_Renderer* r, SDL_FPoint c, float rad, float a0, float a1, SDL_Color color) {
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
  const int steps = 10;
  for (int pass = 0; pass < 2; ++pass) {
    const float rr = rad + static_cast<float>(pass);
    SDL_FPoint prev{c.x + rr * std::cos(a0), c.y + rr * std::sin(a0)};
    for (int i = 1; i <= steps; ++i) {
      const float a = a0 + (a1 - a0) * static_cast<float>(i) / steps;
      const SDL_FPoint p{c.x + rr * std::cos(a), c.y + rr * std::sin(a)};
      SDL_RenderLine(r, prev.x, prev.y, p.x, p.y);
      prev = p;
    }
  }
}

// Linha com 3 px de traço (três linhas deslocadas em x).
void ThickLine(SDL_Renderer* r, float x0, float y0, float x1, float y1, SDL_Color color) {
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
  for (int k = -1; k <= 1; ++k) {
    const float o = static_cast<float>(k) * 1.5f;
    SDL_RenderLine(r, x0 + o, y0, x1 + o, y1);
  }
}

}  // namespace

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
  // Ícone em coordenadas 0..1 da caixa: corpo do alto-falante, cone, ondas (ou X).
  const SDL_FRect& rect = Rect();
  auto P = [&](float u, float v) { return SDL_FPoint{rect.x + u * rect.w, rect.y + v * rect.h}; };
  const SDL_FPoint body[4] = {P(0.20f, 0.40f), P(0.34f, 0.40f), P(0.34f, 0.60f), P(0.20f, 0.60f)};
  FillPolygon(r, body, 4, ink);
  const SDL_FPoint cone[4] = {P(0.34f, 0.40f), P(0.52f, 0.24f), P(0.52f, 0.76f), P(0.34f, 0.60f)};
  FillPolygon(r, cone, 4, ink);
  if (active_) {
    const SDL_FPoint c = P(0.52f, 0.50f);
    Arc(r, c, rect.w * 0.12f, -0.9f, 0.9f, ink);
    Arc(r, c, rect.w * 0.22f, -0.9f, 0.9f, ink);
  } else {
    // X ao lado do cone.
    const SDL_Color x = colors::Get("status.nerf");
    ThickLine(r, P(0.60f, 0.38f).x, P(0.60f, 0.38f).y, P(0.80f, 0.62f).x, P(0.80f, 0.62f).y, x);
    ThickLine(r, P(0.80f, 0.38f).x, P(0.80f, 0.38f).y, P(0.60f, 0.62f).x, P(0.60f, 0.62f).y, x);
  }
}

void ButtonIcon::RenderHint(SDL_Renderer* r, SDL_Color ink) const {
  // "?" na fonte de título, com ~2/3 da altura do botão.
  const SDL_FRect& rect = Rect();
  const float scale = rect.h * 0.66f / SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
  text::DrawCentered(r, "?", rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, scale, ink);
  if (!active_) {
    // Risco diagonal sobre o "?".
    ThickLine(r, rect.x + rect.w * 0.28f, rect.y + rect.h * 0.72f, rect.x + rect.w * 0.72f,
              rect.y + rect.h * 0.28f, colors::Get("status.nerf"));
  }
}
