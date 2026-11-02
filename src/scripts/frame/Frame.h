#pragma once

#include <SDL3/SDL.h>

#include <string>

// Placa de pedra com moldura de metal e estrelas nos cantos (gerada de art/ui.jpg por
// tools/make_ui_frame.py) em três variantes: sprites/UiFrameButton.png (como na arte, para
// botões), sprites/UiFrameWindow.png (mais escura e cinza, tom de metal, para janelas,
// painéis do HUD e tooltips) e sprites/UiFrameButtonDanger.png (pedra em vermelho escuro,
// para botões de ação destrutiva: Sair do menu, confirmar abandono da batalha).
// A folha é dividida em 9 TILES IGUAIS (terços):
//   tl t  tr
//   ml m  mr
//   bl b  br
// e cada elemento é montado por tiles na escala certa: a linha de cima é tl + N vezes t + tr,
// a de baixo bl + N vezes b + br e, em painéis altos, linhas ml + N vezes m + mr no meio.
// A escala vem da altura (tile = altura / linhas, com no mínimo 2 linhas) e N cobre a largura
// com tiles quase quadrados. O centro da folha é simétrico, então os tiles repetem sem emenda.
namespace frame {

// Qual placa: botão (azulada, como na arte), janela (escura, metálica) ou botão de perigo
// (vermelho escuro).
enum class Kind { kButton, kWindow, kButtonDanger };

// `dir` é a pasta sprites/ (UiFrameButton.png, UiFrameWindow.png e UiFrameButtonDanger.png).
// Sem a de perigo, kButtonDanger usa a de botão.
bool Init(SDL_Renderer* r, const std::string& dir);
void Shutdown();
bool Ready();

// Desenha a placa em `rect`. brightness escurece (0..1; 1 = como na imagem) para estados de
// botão. ideal_tile: tamanho de tile preferido em px (define quantas linhas: altura /
// ideal_tile, mínimo 2); <= 0 usa kIdealTile. alpha: opacidade.
constexpr float kIdealTile = 40.0f;
void Draw(SDL_Renderer* r, const SDL_FRect& rect, Kind kind, float brightness = 1.0f,
          float ideal_tile = 0.0f, float alpha = 1.0f);

}  // namespace frame
