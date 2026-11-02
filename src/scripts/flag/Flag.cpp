#include "scripts/flag/Flag.h"

#include <cmath>
#include <vector>

namespace flag {

namespace {

constexpr SDL_Color kBorder{12, 16, 24, 255};

void Fill(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(r, &rect);
}

SDL_FColor ToF(SDL_Color c) { return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f}; }

// Polígono convexo preenchido (leque de triângulos).
void FillPolygon(SDL_Renderer* r, const std::vector<SDL_FPoint>& pts, SDL_Color color) {
  const int n = static_cast<int>(pts.size());
  std::vector<SDL_Vertex> v(pts.size());
  for (int i = 0; i < n; ++i) {
    v[static_cast<size_t>(i)] = SDL_Vertex{pts[static_cast<size_t>(i)], ToF(color), {0, 0}};
  }
  std::vector<int> idx;
  for (int i = 1; i + 1 < n; ++i) idx.insert(idx.end(), {0, i, i + 1});
  SDL_RenderGeometry(r, nullptr, v.data(), n, idx.data(), static_cast<int>(idx.size()));
}

void FillCircle(SDL_Renderer* r, SDL_FPoint c, float radius, SDL_Color color) {
  constexpr int kSteps = 40;
  std::vector<SDL_FPoint> pts;
  pts.reserve(kSteps);
  for (int i = 0; i < kSteps; ++i) {
    const float a = 2.0f * 3.14159265f * static_cast<float>(i) / kSteps;
    pts.push_back({c.x + radius * std::cos(a), c.y + radius * std::sin(a)});
  }
  FillPolygon(r, pts, color);
}

// Estados Unidos: 13 listras (7 vermelhas), cantão azul com grade de pontos brancos.
void DrawUnitedStates(SDL_Renderer* r, const SDL_FRect& f) {
  const SDL_Color red{178, 34, 52, 255};
  const SDL_Color white{255, 255, 255, 255};
  const SDL_Color blue{60, 59, 110, 255};
  Fill(r, f, white);
  const float stripe = f.h / 13.0f;
  for (int i = 0; i < 13; i += 2) {
    Fill(r, SDL_FRect{f.x, f.y + i * stripe, f.w, stripe + 0.5f}, red);
  }
  const SDL_FRect canton{f.x, f.y, f.w * 0.4f, stripe * 7.0f};
  Fill(r, canton, blue);
  // Estrelas: grade 6 x 5 de pontos.
  const int cols = 6, rows = 5;
  const float dot = SDL_max(1.0f, f.h / 24.0f);
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      const float cx = canton.x + canton.w * (col + 0.5f) / cols;
      const float cy = canton.y + canton.h * (row + 0.5f) / rows;
      Fill(r, SDL_FRect{cx - dot / 2.0f, cy - dot / 2.0f, dot, dot}, white);
    }
  }
}

// Espanha: vermelho, amarelo (metade), vermelho.
void DrawSpain(SDL_Renderer* r, const SDL_FRect& f) {
  const SDL_Color red{170, 21, 27, 255};
  const SDL_Color yellow{241, 191, 0, 255};
  Fill(r, f, red);
  Fill(r, SDL_FRect{f.x, f.y + f.h * 0.25f, f.w, f.h * 0.5f}, yellow);
}

// Brasil: verde, losango amarelo, círculo azul.
void DrawBrazil(SDL_Renderer* r, const SDL_FRect& f) {
  const SDL_Color green{0, 156, 59, 255};
  const SDL_Color yellow{255, 223, 0, 255};
  const SDL_Color blue{0, 39, 118, 255};
  Fill(r, f, green);
  const float inset_x = f.w * 0.085f;
  const float inset_y = f.h * 0.085f;
  const float cx = f.x + f.w / 2.0f;
  const float cy = f.y + f.h / 2.0f;
  FillPolygon(r,
              {{cx, f.y + inset_y}, {f.x + f.w - inset_x, cy}, {cx, f.y + f.h - inset_y},
               {f.x + inset_x, cy}},
              yellow);
  FillCircle(r, {cx, cy}, f.h * 0.25f, blue);
}

}  // namespace

Country FromCode(const std::string& code) {
  if (code == "us") return Country::kUnitedStates;
  if (code == "es") return Country::kSpain;
  return Country::kBrazil;
}

void Draw(SDL_Renderer* r, Country country, const SDL_FRect& rect) {
  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &previous);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  switch (country) {
    case Country::kUnitedStates:
      DrawUnitedStates(r, rect);
      break;
    case Country::kSpain:
      DrawSpain(r, rect);
      break;
    case Country::kBrazil:
      DrawBrazil(r, rect);
      break;
  }
  SDL_SetRenderDrawColor(r, kBorder.r, kBorder.g, kBorder.b, kBorder.a);
  SDL_RenderRect(r, &rect);
  SDL_SetRenderDrawBlendMode(r, previous);
}

}  // namespace flag
