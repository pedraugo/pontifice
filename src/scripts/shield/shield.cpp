#include "scripts/shield/shield.h"

#include <algorithm>
#include <cmath>

#include "scripts/colors/colors.h"
#include "scripts/opacity/opacity.h"

namespace {

float SmoothStep(float t) {
  t = math::Clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float EaseOutBack(float t, float c) {
  t = math::Clamp(t, 0.0f, 1.0f);
  const float u = t - 1.0f;
  return 1.0f + (c + 1.0f) * u * u * u + c * u * u;
}

// Heater shield outline in XY (y up): flat top, straight sides, curved base; convex, CCW
std::vector<math::Vec2> HeaterOutline(float s, int n = 14) {
  const float w = 0.48f, top = 0.55f, shoulder = 0.05f, tip = -0.6f;
  std::vector<math::Vec2> pts;
  pts.push_back({-w * s, top * s});
  pts.push_back({-w * s, shoulder * s});
  for (int i = 1; i < n; ++i) {
    const float a = static_cast<float>(i) / n * (math::kPi / 2.0f);
    pts.push_back({-w * std::cos(a) * s, (shoulder - (shoulder - tip) * std::sin(a)) * s});
  }
  pts.push_back({0.0f, tip * s});
  for (int i = n - 1; i >= 1; --i) {
    const float a = static_cast<float>(i) / n * (math::kPi / 2.0f);
    pts.push_back({w * std::cos(a) * s, (shoulder - (shoulder - tip) * std::sin(a)) * s});
  }
  pts.push_back({w * s, shoulder * s});
  pts.push_back({w * s, top * s});
  return pts;
}

// Convex polygon extruded from z0 to z1: front/back as fans, sides as quads
void Extrude(const std::vector<math::Vec2>& pts, float z0, float z1, std::vector<Vertex>& v,
             std::vector<Uint32>& idx) {
  const Uint32 n = static_cast<Uint32>(pts.size());
  for (float z : {z1, z0}) {
    const Uint32 base = static_cast<Uint32>(v.size());
    v.push_back({0.0f, 0.0f, z, 0.5f, 0.5f});
    for (const math::Vec2& p : pts) v.push_back({p.x, p.y, z, 0.5f + p.x, 0.5f - p.y});
    for (Uint32 i = 0; i < n; ++i) {
      idx.insert(idx.end(), {base, base + 1 + i, base + 1 + (i + 1) % n});
    }
  }
  for (Uint32 i = 0; i < n; ++i) {
    const math::Vec2 a = pts[i], b = pts[(i + 1) % n];
    const Uint32 base = static_cast<Uint32>(v.size());
    v.push_back({a.x, a.y, z1, 0, 0});
    v.push_back({b.x, b.y, z1, 1, 0});
    v.push_back({b.x, b.y, z0, 1, 1});
    v.push_back({a.x, a.y, z0, 0, 1});
    idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  }
}

void Box(std::vector<Vertex>& v, std::vector<Uint32>& idx, float x0, float y0, float x1,
         float y1, float z0, float z1) {
  Extrude({{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}}, z0, z1, v, idx);
}

}

bool Shield::Init(Gpu& gpu) {
  const SDL_FColor gold = colors::GetF("goldDark");
  const SDL_FColor silver = colors::GetF("steel");
  auto add = [&](std::vector<Vertex>& v, std::vector<Uint32>& i, SDL_FColor color) {
    Part part;
    part.mesh = gpu.CreateMesh(v, i);
    part.color = color;
    if (part.mesh.index_count == 0) return false;
    parts_.push_back(part);
    return true;
  };
  {
    std::vector<Vertex> v;
    std::vector<Uint32> i;
    // Gold border: full outline, behind
    Extrude(HeaterOutline(1.0f), -0.05f, 0.0f, v, i);
    if (!add(v, i, gold)) return false;
  }
  {
    std::vector<Vertex> v;
    std::vector<Uint32> i;
    // Silver body: slightly smaller, in front of the border
    Extrude(HeaterOutline(0.84f), 0.0f, 0.035f, v, i);
    if (!add(v, i, silver)) return false;
  }
  {
    std::vector<Vertex> v;
    std::vector<Uint32> i;
    // Gold cross in relief on the body
    Box(v, i, -0.055f, -0.36f, 0.055f, 0.36f, 0.035f, 0.07f);
    Box(v, i, -0.27f, 0.1f, 0.27f, 0.21f, 0.035f, 0.07f);
    if (!add(v, i, gold)) return false;
  }
  std::vector<Vertex> v;
  std::vector<Uint32> i;
  // Shock wave: flat ring in XY, inner radius 0.85, outer 1
  const int segments = 48;
  for (int k = 0; k <= segments; ++k) {
    const float a = 2.0f * math::kPi * static_cast<float>(k) / segments;
    const float c = std::cos(a), s = std::sin(a);
    v.push_back({0.85f * c, 0.85f * s, 0.0f, 0, 0});
    v.push_back({c, s, 0.0f, 1, 0});
  }
  for (Uint32 k = 0; k < static_cast<Uint32>(segments); ++k) {
    const Uint32 a = k * 2;
    i.insert(i.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
  }
  ring_ = gpu.CreateMesh(v, i);
  return ring_.index_count > 0;
}

void Shield::Destroy(Gpu& gpu) {
  for (Part& part : parts_) gpu.DestroyMesh(part.mesh);
  parts_.clear();
  gpu.DestroyMesh(ring_);
}

void Shield::Play(const math::Vec3& target, const math::Vec3& from, float hit_delay,
                  bool broken) {
  target_ = target;
  const math::Vec3 d = from - target;
  yaw_ = std::atan2(d.x, d.z);  // RotateY(yaw) points local +z at the attacker
  hit_ = std::max(0.0f, hit_delay);
  broken_ = broken;
  time_ = 0.0f;
}

void Shield::Update(float dt) {
  if (Active()) time_ += dt;
}

void Shield::Render(Gpu& gpu, const math::Mat4& view_proj) const {
  if (!Active()) return;
  const float t = time_;
  const float since_hit = t - hit_;
  float scale = EaseOutBack(t / layout_.appear, layout_.overshoot);
  float recoil = 0.0f, spin = 0.0f, drop = 0.0f, flash = 0.0f;
  if (since_hit >= 0.0f) {
    recoil = layout_.recoil * std::exp(-since_hit * layout_.recoil_decay);
    flash = std::exp(-since_hit * layout_.flash_decay);
    if (broken_) {
      const float f = SmoothStep(since_hit / (layout_.broken - hit_));
      spin = layout_.break_spin * f;
      drop = layout_.break_drop * f * f;
      scale *= 1.0f - f;
    }
  }
  if (!broken_) {
    const float out = (t - (layout_.hold - layout_.fade_seconds)) / layout_.fade_seconds;
    scale *= 1.0f - SmoothStep(out);
  }
  if (scale <= 0.001f) return;

  const math::Vec3 center{target_.x, target_.y + layout_.height - drop, target_.z};
  const math::Mat4 world = math::Mat4::Translate(center) * math::Mat4::RotateY(yaw_) *
                           math::Mat4::Translate({0.0f, 0.0f, -layout_.recoil_push * recoil}) *
                           math::Mat4::RotateX(-(layout_.lean + recoil)) *
                           math::Mat4::RotateZ(spin) * math::Mat4::Scale(layout_.scale * scale);
  // Hit flash pushes colors above 1 (shader multiplies by light); broken tints red
  for (const Part& part : parts_) {
    SDL_FColor c = part.color;
    const float boost = 1.0f + layout_.flash_boost * flash;
    c.r *= boost;
    c.g *= boost;
    c.b *= boost;
    if (broken_ && since_hit >= 0.0f) {
      c.g *= 0.45f;
      c.b *= 0.4f;
      c.r = std::max(c.r, 0.9f);
    }
    gpu.Draw(part.mesh, nullptr, world, view_proj, c, Gpu::DrawMode::kMesh);
  }
  if (since_hit >= 0.0f && since_hit < layout_.wave_seconds) {
    const float w = since_hit / layout_.wave_seconds;
    const float radius = layout_.scale * (layout_.wave_start + layout_.wave_grow * SmoothStep(w));
    const float wave_alpha = opacity::Get("shieldWave") * (1.0f - w);
    SDL_FColor color = colors::GetF(broken_ ? "vermilion" : "goldLight");
    color.a = wave_alpha;
    const math::Mat4 ring = math::Mat4::Translate(center) * math::Mat4::RotateY(yaw_) *
                            math::Mat4::RotateX(-layout_.lean) *
                            math::Mat4::Translate({0.0f, 0.0f, layout_.wave_lift}) *
                            math::Mat4::Scale({radius, radius, 1.0f});
    gpu.Draw(ring_, nullptr, ring, view_proj, color, Gpu::DrawMode::kEmissive);
  }
}
