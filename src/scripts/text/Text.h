#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "scripts/image/Image.h"

// Texto com as fontes do jogo via stb_truetype: por fonte, um atlas de glifos (ASCII e
// Latin-1, 0xA0..0xFF) em três tamanhos (16, 32 e 64 px) numa SDL_Texture, desenhado escalado
// para o tamanho pedido. `scale` mantém a convenção antiga da fonte de depuração: altura da
// linha = 8 px * scale (scale 2 = 16 px, 6 = 48 px). Codepoints que a fonte não tem são
// desenhados sem acento. Sem a fonte de corpo, o texto usa a de título; sem a de título, cai
// na fonte de depuração do SDL.
namespace text {

// kTitle: assets/PirataOne.ttf (padrão: títulos, botões, rótulos). kBody: assets/Newsreader.ttf
// (texto corrido: linha de situação e dica da batalha, descrição das cartas).
enum class Face { kTitle, kBody };

bool Init(SDL_Renderer* r, const std::string& ttf_path, Face face = Face::kTitle);
void Shutdown();

float Width(const char* str, float scale, Face face = Face::kTitle);
void Draw(SDL_Renderer* r, const char* str, float x, float y, float scale, SDL_Color color,
          Face face = Face::kTitle);
void DrawCentered(SDL_Renderer* r, const char* str, float center_x, float center_y, float scale,
                  SDL_Color color, Face face = Face::kTitle);

// Desenho na CPU, em uma Image RGBA (texturas compostas, como a frente das cartas). `px` é a
// altura do em em pixels; `y` é o topo da caixa do em. Precisa de Init antes; é seguro chamar
// de outra thread (só lê a fonte). Com a fonte indisponível não desenha nada.
float ImageWidth(const char* str, float px, Face face = Face::kTitle);
void DrawOnImage(Image& img, const char* str, float x, float y, float px, SDL_Color color,
                 Face face = Face::kTitle);

}  // namespace text
