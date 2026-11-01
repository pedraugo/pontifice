#include "scripts/text/Text.h"

#include <string>

namespace {

// A fonte de depuração do SDL só tem ASCII. Troca letras acentuadas (Latin-1) pela base:
// "Dilúvio" vira "Diluvio", "VOCÊ" vira "VOCE". Outros caracteres passam como estão.
std::string FoldToAscii(const char* str) {
  std::string out;
  Uint32 ch = 0;
  while ((ch = SDL_StepUTF8(&str, nullptr)) != 0) {
    char c = 0;
    if (ch < 0x80) {
      c = static_cast<char>(ch);
    } else if (ch >= 0xC0 && ch <= 0xC5) {
      c = 'A';
    } else if (ch == 0xC7) {
      c = 'C';
    } else if (ch >= 0xC8 && ch <= 0xCB) {
      c = 'E';
    } else if (ch >= 0xCC && ch <= 0xCF) {
      c = 'I';
    } else if (ch == 0xD1) {
      c = 'N';
    } else if ((ch >= 0xD2 && ch <= 0xD6) || ch == 0xD8) {
      c = 'O';
    } else if (ch >= 0xD9 && ch <= 0xDC) {
      c = 'U';
    } else if (ch >= 0xE0 && ch <= 0xE5) {
      c = 'a';
    } else if (ch == 0xE7) {
      c = 'c';
    } else if (ch >= 0xE8 && ch <= 0xEB) {
      c = 'e';
    } else if (ch >= 0xEC && ch <= 0xEF) {
      c = 'i';
    } else if (ch == 0xF1) {
      c = 'n';
    } else if ((ch >= 0xF2 && ch <= 0xF6) || ch == 0xF8) {
      c = 'o';
    } else if (ch >= 0xF9 && ch <= 0xFC) {
      c = 'u';
    } else {
      c = '?';
    }
    out.push_back(c);
  }
  return out;
}

}  // namespace

namespace text {

float Width(const char* str, float scale) {
  // Conta caracteres UTF-8, não bytes: a fonte desenha um glifo por caractere.
  size_t count = 0;
  while (SDL_StepUTF8(&str, nullptr) != 0) ++count;
  return static_cast<float>(count) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
}

void Draw(SDL_Renderer* r, const char* str, float x, float y, float scale, SDL_Color color) {
  float prev_x = 1.0f;
  float prev_y = 1.0f;
  SDL_GetRenderScale(r, &prev_x, &prev_y);

  SDL_SetRenderScale(r, scale, scale);
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
  SDL_RenderDebugText(r, x / scale, y / scale, FoldToAscii(str).c_str());
  SDL_SetRenderScale(r, prev_x, prev_y);
}

void DrawCentered(SDL_Renderer* r, const char* str, float center_x, float center_y, float scale,
                  SDL_Color color) {
  const float w = Width(str, scale);
  const float h = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
  Draw(r, str, center_x - w / 2.0f, center_y - h / 2.0f, scale, color);
}

}  // namespace text
