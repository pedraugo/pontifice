#include "scripts/ui/ui.h"

#include "scripts/config/config.h"

namespace ui {

float Cells(const std::string& path, float fallback_cells) {
  return Grid(config::Float("ui", path, fallback_cells));
}

float TextScale(const std::string& path, float fallback) {
  return config::Float("text", path, fallback) * kScale;
}

float Number(const std::string& path, float fallback) {
  return config::Float("ui", path, fallback);
}

}
