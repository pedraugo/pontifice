#pragma once

#include <SDL3/SDL.h>

// Texto com a fonte de depuração do SDL (8x8 px por caractere), escalada.
// Provisório até a direção de arte definir fonte própria.
namespace text {

float Width(const char* str, float scale);
void Draw(SDL_Renderer* r, const char* str, float x, float y, float scale, SDL_Color color);
void DrawCentered(SDL_Renderer* r, const char* str, float center_x, float center_y, float scale,
                  SDL_Color color);

}  // namespace text
