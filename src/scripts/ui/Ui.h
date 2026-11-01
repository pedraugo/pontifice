#pragma once

// Escala da interface 2D. O layout (HUD, menu, botões, textos) foi medido em 1280x720; a
// resolução lógica do jogo é 1920x1080, então medidas em px passam por ui::Px.
namespace ui {

constexpr float kScale = 1.5f;
constexpr float Px(float v) { return v * kScale; }

}  // namespace ui
