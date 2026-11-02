#pragma once

// Escala e grade da interface 2D. Tamanhos de texto seguem a escala da fonte de depuração
// (ui::kScale, medidos em 1280x720). Posições e tamanhos de painéis, botões e margens ficam
// numa GRADE DE 16 PX da resolução lógica (1920x1080): ui::Grid(n) = n células. 1920 = 120
// células, 1080 = 67,5 células.
namespace ui {

constexpr float kScale = 1.5f;
constexpr float Px(float v) { return v * kScale; }

constexpr float kGrid = 16.0f;
constexpr float Grid(float cells) { return cells * kGrid; }
// Arredonda `v` para cima até um múltiplo da grade (larguras calculadas a partir de texto).
constexpr float SnapUp(float v) {
  const float cells = v / kGrid;
  const float whole = static_cast<float>(static_cast<int>(cells));
  return (cells > whole ? whole + 1.0f : whole) * kGrid;
}

}  // namespace ui
