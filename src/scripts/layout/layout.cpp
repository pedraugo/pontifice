#include "scripts/layout/layout.h"

#include "scripts/config/config.h"

namespace layout {

float Number(const std::string& path, float fallback) {
  return config::Float("layout", path, fallback);
}

math::Vec3 Vec3(const std::string& path, const math::Vec3& fallback) {
  return config::Vec3("layout", path, fallback);
}

const json::Value* Node(const std::string& path) { return config::Node("layout", path); }

}
