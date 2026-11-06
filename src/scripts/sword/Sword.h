#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "scripts/gpu/Gpu.h"
#include "scripts/math/Math.h"

// Procedural sword strike over a target card, plus an emissive slash quad on impact
class Sword {
 public:
  // Seconds and radians; swing_end is the fraction of seconds at which the blade lands
  struct Layout {
    float scale = 1.0f;
    float seconds = 0.55f;
    float swing_end = 0.35f;
    float pivot_height = 1.15f;
    float pivot_back = 0.45f;  // hilt offset toward the attacker
    float angle_start = -1.3f;  // blade raised back toward the attacker
    float angle_end = 2.75f;  // blade pointing down into the card
    float slash_length = 1.5f;
    float slash_width = 0.14f;
    float slash_seconds = 0.3f;
  };

  bool Init(Gpu& gpu);
  void Destroy(Gpu& gpu);
  Layout& GetLayout() { return layout_; }

  void Play(const math::Vec3& target, const math::Vec3& from);
  bool Active() const { return time_ < layout_.seconds; }
  float HitDelay() const { return layout_.seconds * layout_.swing_end; }

  void Update(float dt);
  void Render(Gpu& gpu, const math::Mat4& view_proj) const;

 private:
  struct Part {
    GpuMesh mesh;
    SDL_FColor color;
  };
  static void Box(std::vector<Vertex>& v, std::vector<Uint32>& i, float x0, float y0, float z0,
                  float x1, float y1, float z1);

  Layout layout_;
  std::vector<Part> parts_;
  GpuMesh slash_;
  math::Vec3 target_;
  float yaw_ = 0.0f;
  float time_ = 1e9f;  // seconds since Play; >= seconds means inactive
};
