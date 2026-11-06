#pragma once

#include <SDL3/SDL.h>

// Thick 2D primitives in screen space via SDL_RenderGeometry; use the renderer's blend mode
namespace draw {

void Line(SDL_Renderer* r, SDL_FPoint a, SDL_FPoint b, SDL_Color color, float thickness = 1.0f);
// Angles in radians, clockwise on screen; drawn as a polyline
void Arc(SDL_Renderer* r, SDL_FPoint c, float radius, float a0, float a1, SDL_Color color,
         float thickness = 1.0f);
void FillPolygon(SDL_Renderer* r, const SDL_FPoint* pts, int n, SDL_Color color);

}
