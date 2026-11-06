#pragma once

#include <SDL3/SDL.h>

// Primitivas 2D com espessura, em coordenadas de tela, via SDL_RenderGeometry (ícones de
// ButtonIcon). Respeitam o blend mode atual do renderer.
namespace draw {

// Segmento de `thickness` px.
void Line(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_Color color, float thickness = 1.0f);
// Arco de centro c e raio `radius`, do ângulo a0 ao a1 (radianos, sentido horário na tela),
// como polilinha de `thickness` px.
void Arc(SDL_Renderer* r, SDL_FPoint c, float radius, float a0, float a1, SDL_Color color,
         float thickness = 1.0f);
// Polígono convexo preenchido (leque de triângulos).
void FillPolygon(SDL_Renderer* r, const SDL_FPoint* pts, int n, SDL_Color color);

}  // namespace draw
