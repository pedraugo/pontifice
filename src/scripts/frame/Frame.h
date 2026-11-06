#pragma once

#include <SDL3/SDL.h>

// Placa da interface desenhada só com retângulos, sem textura: fundo RGBA(0,0,0,0,75), borda
// clara de 3 px RGBA(255,255,255,0,15) e, por fora dela, outline escura de 3 px
// RGBA(0,0,0,0,50) (a "borda da borda"). Tudo cabe em `rect`: a outline ocupa os 3 px de
// fora, a borda os 3 px seguintes e o fundo o resto. Serve a botões (Button, ButtonIcon),
// janelas (ConfirmDialog), painéis do HUD e tooltips. Cores em
// assets/colors.json (papéis "frame.*").
namespace frame {

// Botão e janela são iguais; botão de perigo (Sair do menu, confirmar abandono) tem fundo em
// vermelho escuro.
enum class Kind { kButton, kWindow, kButtonDanger };
// Estado de botão: destaque (hover/foco) clareia a borda; pressionado escurece o fundo.
enum class State { kNormal, kHighlight, kPressed };

constexpr float kOutline = 3.0f;
constexpr float kBorder = 3.0f;
constexpr float kInset = kOutline + kBorder;  // de `rect` até o fundo

// Retângulo do fundo (rect sem outline e borda).
SDL_FRect Inner(const SDL_FRect& rect);

// Desenha a placa em `rect`. alpha multiplica a opacidade de tudo (0..1).
void Draw(SDL_Renderer* r, const SDL_FRect& rect, Kind kind, State state = State::kNormal,
          float alpha = 1.0f);

}  // namespace frame
