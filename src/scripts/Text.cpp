#include "scripts/Text.h"

#include <cstring>

namespace text {

float Width(const char* str, float scale) {
  return static_cast<float>(std::strlen(str)) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

void Draw(SDL_Renderer* r, const char* str, float x, float y, float scale, SDL_Color color) {
  float prev_x = 1.0f;
  float prev_y = 1.0f;
  SDL_GetRenderScale(r, &prev_x, &prev_y);

  SDL_SetRenderScale(r, scale, scale);
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
  SDL_RenderDebugText(r, x / scale, y / scale, str);
  SDL_SetRenderScale(r, prev_x, prev_y);
}

void DrawCentered(SDL_Renderer* r, const char* str, float center_x, float center_y, float scale,
                  SDL_Color color) {
  const float w = Width(str, scale);
  const float h = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
  Draw(r, str, center_x - w / 2.0f, center_y - h / 2.0f, scale, color);
}

}  // namespace text
