#include "scripts/draw/Draw.h"

#include <cmath>
#include <vector>

namespace draw {

namespace {

SDL_FColor ToF(SDL_Color c) { return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f}; }

}

void FillPolygon(SDL_Renderer* r, const SDL_FPoint* pts, int n, SDL_Color color) {
  if (n < 3) return;
  std::vector<SDL_Vertex> v(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = SDL_Vertex{pts[i], ToF(color), {0, 0}};
  std::vector<int> idx;
  for (int i = 1; i + 1 < n; ++i) idx.insert(idx.end(), {0, i, i + 1});
  SDL_RenderGeometry(r, nullptr, v.data(), n, idx.data(), static_cast<int>(idx.size()));
}

void Line(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_Color color, float thickness) {
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float len = std::sqrt(dx * dx + dy * dy);
  if (len <= 0.0f) return;
  // Unit normal times half thickness: the segment becomes a quad
  const float nx = -dy / len * thickness * 0.5f;
  const float ny = dx / len * thickness * 0.5f;
  const SDL_FPoint quad[4] = {{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny},
                              {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}};
  FillPolygon(r, quad, 4, color);
}

void Arc(SDL_Renderer* r, SDL_FPoint c, float radius, float a0, float a1, SDL_Color color,
         float thickness) {
  const int steps = 12;
  SDL_FPoint prev{c.x + radius * std::cos(a0), c.y + radius * std::sin(a0)};
  for (int i = 1; i <= steps; ++i) {
    const float a = a0 + (a1 - a0) * static_cast<float>(i) / steps;
    const SDL_FPoint p{c.x + radius * std::cos(a), c.y + radius * std::sin(a)};
    Line(r, prev, p, color, thickness);
    prev = p;
  }
}

}
