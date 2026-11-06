#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "scripts/gpu/gpu.h"
#include "scripts/math/math.h"

// Procedural heraldic shield over a defending card, facing the attacker (docs/batalha.md)
class Shield {
 public:
  // Seconds unless noted; hold/broken are total durations when the defense holds/breaks
  struct Layout {
    float scale = 0.85f;
    float height = 0.8f;
    float lean = 0.3f;  // backward tilt (rad) so the camera sees the front
    float appear = 0.15f;
    float hold = 0.9f;
    float broken = 0.8f;
    float wave_seconds = 0.4f;
  };

  bool Init(Gpu& gpu);
  void Destroy(Gpu& gpu);
  Layout& GetLayout() { return layout_; }

  // target: defending card center; from: attacker; hit_delay: seconds until the blow lands
  void Play(const math::Vec3& target, const math::Vec3& from, float hit_delay, bool broken);
  bool Active() const { return time_ < Duration(); }

  void Update(float dt);
  void Render(Gpu& gpu, const math::Mat4& view_proj) const;  // arena pass, after the cards

 private:
  struct Part {
    GpuMesh mesh;
    SDL_FColor color;
  };
  float Duration() const { return broken_ ? layout_.broken : layout_.hold; }

  Layout layout_;
  std::vector<Part> parts_;
  GpuMesh ring_;
  math::Vec3 target_;
  float yaw_ = 0.0f;
  float hit_ = 0.19f;
  bool broken_ = false;
  float time_ = 1e9f;  // seconds since Play; >= Duration() means inactive
};
