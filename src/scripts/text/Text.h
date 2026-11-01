#pragma once

#include <SDL3/SDL.h>

#include <string>

// Texto com a fonte do jogo (assets/PirataOne.ttf) via stb_truetype: um atlas de glifos
// (ASCII e Latin-1) em três tamanhos (16, 32 e 64 px) numa SDL_Texture, desenhado escalado
// para o tamanho pedido. `scale` mantém a convenção antiga da fonte de depuração: altura da
// linha = 8 px * scale (scale 2 = 16 px, 6 = 48 px). Codepoints que a fonte não tem são
// desenhados sem acento. Se Init falhar, cai na fonte de depuração do SDL.
namespace text {

bool Init(SDL_Renderer* r, const std::string& ttf_path);
void Shutdown();

float Width(const char* str, float scale);
void Draw(SDL_Renderer* r, const char* str, float x, float y, float scale, SDL_Color color);
void DrawCentered(SDL_Renderer* r, const char* str, float center_x, float center_y, float scale,
                  SDL_Color color);

}  // namespace text
