#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "scripts/gpu/gpu.h"
#include "scripts/math/math.h"
#include "scripts/layout/layout.h"

// Procedural heraldic shield over a defending card, facing the attacker (docs/batalha.md)
class Shield {
 public:
  // Seconds unless noted; hold/broken are total durations when the defense holds/breaks
  struct Layout {
    float scale = layout::Number("battle.shield.scale", 0.85f);
    float height = layout::Number("battle.shield.height", 0.8f);
    // backward tilt (rad) so the camera sees the front
    float lean = layout::Number("battle.shield.lean", 0.3f);
    float appear = layout::Number("battle.shield.appear", 0.15f);
    float hold = layout::Number("battle.shield.hold", 0.9f);
    float broken = layout::Number("battle.shield.broken", 0.8f);
    float wave_seconds = layout::Number("battle.shield.waveSeconds", 0.4f);
    // ease-out-back strength when it appears
    float overshoot = layout::Number("battle.shield.overshoot", 1.9f);
    // shrink at the end of hold
    float fade_seconds = layout::Number("battle.shield.fadeSeconds", 0.25f);
    float recoil = layout::Number("battle.shield.recoil", 0.35f);  // radians tilted back by the hit
    float recoil_decay = layout::Number("battle.shield.recoilDecay", 9.0f);
    // backward offset per radian of recoil
    float recoil_push = layout::Number("battle.shield.recoilPush", 0.25f);
    float flash_boost = layout::Number("battle.shield.flashBoost", 1.4f);
    float flash_decay = layout::Number("battle.shield.flashDecay", 7.0f);
    float break_spin = layout::Number("battle.shield.breakSpin", 1.6f);
    float break_drop = layout::Number("battle.shield.breakDrop", 0.7f);
    // ring radius at the hit, times scale
    float wave_start = layout::Number("battle.shield.waveStart", 0.55f);
    float wave_grow = layout::Number("battle.shield.waveGrow", 1.1f);
    // In front of the shield face.
    float wave_lift = layout::Number("battle.shield.waveLift", 0.09f);
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
