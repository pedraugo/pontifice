#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "scripts/gpu/gpu.h"
#include "scripts/math/math.h"
#include "scripts/layout/layout.h"

// Procedural sword strike over a target card, plus an emissive slash quad on impact
class Sword {
 public:
  // Seconds and radians; swing_end is the fraction of seconds at which the blade lands
  struct Layout {
    float scale = layout::Number("battle.sword.scale", 1.0f);
    float seconds = layout::Number("battle.sword.seconds", 0.55f);
    float swing_end = layout::Number("battle.sword.swingEnd", 0.35f);
    float pivot_height = layout::Number("battle.sword.pivotHeight", 1.15f);
    // hilt offset toward the attacker
    float pivot_back = layout::Number("battle.sword.pivotBack", 0.45f);
    // blade raised back toward the attacker
    float angle_start = layout::Number("battle.sword.angleStart", -1.3f);
    // blade pointing down into the card
    float angle_end = layout::Number("battle.sword.angleEnd", 2.75f);
    float slash_length = layout::Number("battle.sword.slashLength", 1.5f);
    float slash_width = layout::Number("battle.sword.slashWidth", 0.14f);
    float slash_seconds = layout::Number("battle.sword.slashSeconds", 0.3f);
    float drop = layout::Number("battle.sword.drop", 0.35f);  // pivot drop while swinging
    // slash reaches full length at 1/slash_grow of its time
    float slash_grow = layout::Number("battle.sword.slashGrow", 4.0f);
    float slash_alpha = layout::Number("battle.sword.slashAlpha", 0.9f);
    // above the card, avoids depth fighting
    float slash_lift = layout::Number("battle.sword.slashLift", 0.03f);
    // diagonal across the card, radians
    float slash_angle = layout::Number("battle.sword.slashAngle", 0.7f);
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
