#pragma once

#include <string>

namespace ui {

// Text sizes follow the debug font scale, measured at 1280x720.
constexpr float kScale = 1.5f;
constexpr float Px(float v) { return v * kScale; }

// Layout grid of the 1920x1080 logical resolution: 120 x 67.5 cells.
constexpr float kGrid = 16.0f;
constexpr float Grid(float cells) { return cells * kGrid; }
// Rounds up to a whole grid multiple (widths computed from text).
constexpr float SnapUp(float v) {
  const float cells = v / kGrid;
  const float whole = static_cast<float>(static_cast<int>(cells));
  return (cells > whole ? whole + 1.0f : whole) * kGrid;
}

// Layout measure from assets/ui.json, in grid cells, converted to logical px.
float Cells(const std::string& path, float fallback_cells);
// Text size from assets/text.json, in debug font units, times kScale.
float TextScale(const std::string& path, float fallback);
// Plain number from assets/ui.json (fractions, counts).
float Number(const std::string& path, float fallback);

}
