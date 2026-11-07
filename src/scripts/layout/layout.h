#pragma once

#include <string>

#include "scripts/json/json.h"
#include "scripts/math/math.h"

namespace layout {

// 3D placement, motion and timing from assets/layout.json, by scene ("battle.camera.fov").
float Number(const std::string& path, float fallback);
math::Vec3 Vec3(const std::string& path, const math::Vec3& fallback);
const json::Value* Node(const std::string& path);

}
