#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "scripts/gpu/gpu.h"
#include "scripts/math/math.h"
#include "scripts/model/model.h"

class Torch {
 public:
  // Model measurements, same as tools/makeTorch.py
  static constexpr float kFlameBaseY = 0.82f;
  static constexpr float kFlameCenterY = 1.04f;
  static SDL_FColor LightColor();

  bool LoadGlb(Gpu& gpu, const std::string& path) { return model_.LoadGlb(gpu, path); }
  bool Upload(Gpu& gpu, Model::Data&& data) { return model_.Upload(gpu, std::move(data)); }

  // Body and flame are split: draw the body before mist layers and the flame after them
  void DrawBody(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj) const;
  void DrawBodyDepth(Gpu& gpu, const math::Mat4& world) const;
  // Billboard around y toward camera_pos; ignite 0 = out, 1 = full flame grown from the base
  void DrawFlame(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj,
                 const math::Vec3& camera_pos, float ignite = 1.0f) const;

  // Flame center in world space: the flame rises vertically even when the torch is tilted
  static math::Vec3 FlamePosition(const math::Mat4& world) {
    const math::Vec3 base = world.TransformPoint({0.0f, kFlameBaseY, 0.0f});
    return base + math::Vec3{0.0f, (kFlameCenterY - kFlameBaseY) * Scale(world), 0.0f};
  }

 private:
  static float Scale(const math::Mat4& world) {
    return std::sqrt(world.m[0][0] * world.m[0][0] + world.m[0][1] * world.m[0][1] +
                     world.m[0][2] * world.m[0][2]);
  }

  Model model_;
};
