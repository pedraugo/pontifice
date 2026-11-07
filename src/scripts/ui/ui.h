#pragma once

#include <string>

#include "scripts/math/math.h"

namespace ui {

// Logical resolution and grid from assets/ui.json ("screen"), read on first use.
int Width();
int Height();
float GridSize();
float TextScale();

// Text sizes follow the debug font scale, measured at 1280x720.
inline float Px(float v) { return v * TextScale(); }

// Layout grid of the logical resolution (1920x1080 with 16 px cells: 120 x 67.5 cells).
inline float Grid(float cells) { return cells * GridSize(); }
// Rounds up to a whole grid multiple (widths computed from text).
inline float SnapUp(float v) {
  const float cells = v / GridSize();
  const float whole = static_cast<float>(static_cast<int>(cells));
  return (cells > whole ? whole + 1.0f : whole) * GridSize();
}

// Layout measure from assets/ui.json, in grid cells, converted to logical px.
float Cells(const std::string& path, float fallback_cells);
// Plain number from assets/ui.json (px, seconds, world units, fractions, counts).
float Number(const std::string& path, float fallback);
// [x, y, z] from assets/ui.json (world positions, camera eyes and targets).
math::Vec3 Vec3(const std::string& path, const math::Vec3& fallback);

}
