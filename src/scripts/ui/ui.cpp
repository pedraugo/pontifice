#include "scripts/ui/ui.h"

#include "scripts/config/config.h"

namespace ui {

int Width() {
  static const int width = config::Int("ui", "screen.width", 1920);
  return width;
}

int Height() {
  static const int height = config::Int("ui", "screen.height", 1080);
  return height;
}

float GridSize() {
  static const float grid = config::Float("ui", "screen.grid", 16.0f);
  return grid;
}

float TextScale() {
  static const float scale = config::Float("ui", "screen.textScale", 1.5f);
  return scale;
}

float Cells(const std::string& path, float fallback_cells) {
  return Grid(config::Float("ui", path, fallback_cells));
}

float Number(const std::string& path, float fallback) {
  return config::Float("ui", path, fallback);
}

math::Vec3 Vec3(const std::string& path, const math::Vec3& fallback) {
  return config::Vec3("ui", path, fallback);
}

}
