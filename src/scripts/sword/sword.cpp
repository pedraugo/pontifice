#include "scripts/sword/sword.h"

#include <algorithm>
#include <cmath>

#include "scripts/colors/colors.h"

namespace {

float SmoothStep(float t) {
  t = math::Clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}

// 6 faces, 4 vertices each, UV (0,0)-(1,1); normals come from derivatives in the shader
void Sword::Box(std::vector<Vertex>& v, std::vector<Uint32>& idx, float x0, float y0, float z0,
                float x1, float y1, float z1) {
  const float xs[2] = {x0, x1}, ys[2] = {y0, y1}, zs[2] = {z0, z1};
  auto face = [&](math::Vec3 a, math::Vec3 b, math::Vec3 c, math::Vec3 d) {
    const Uint32 base = static_cast<Uint32>(v.size());
    v.push_back({a.x, a.y, a.z, 0, 0});
    v.push_back({b.x, b.y, b.z, 1, 0});
    v.push_back({c.x, c.y, c.z, 1, 1});
    v.push_back({d.x, d.y, d.z, 0, 1});
    idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
  };
  face({xs[0], ys[0], zs[1]}, {xs[1], ys[0], zs[1]}, {xs[1], ys[1], zs[1]}, {xs[0], ys[1], zs[1]});
  face({xs[1], ys[0], zs[0]}, {xs[0], ys[0], zs[0]}, {xs[0], ys[1], zs[0]}, {xs[1], ys[1], zs[0]});
  face({xs[0], ys[0], zs[0]}, {xs[0], ys[0], zs[1]}, {xs[0], ys[1], zs[1]}, {xs[0], ys[1], zs[0]});
  face({xs[1], ys[0], zs[1]}, {xs[1], ys[0], zs[0]}, {xs[1], ys[1], zs[0]}, {xs[1], ys[1], zs[1]});
  face({xs[0], ys[1], zs[1]}, {xs[1], ys[1], zs[1]}, {xs[1], ys[1], zs[0]}, {xs[0], ys[1], zs[0]});
  face({xs[0], ys[0], zs[0]}, {xs[1], ys[0], zs[0]}, {xs[1], ys[0], zs[1]}, {xs[0], ys[0], zs[1]});
}

bool Sword::Init(Gpu& gpu) {
  struct Spec {
    float x0, y0, z0, x1, y1, z1;
    SDL_FColor color;
  };
  const SDL_FColor steel = colors::GetF("silver");
  const SDL_FColor gold = colors::GetF("brass");
  const SDL_FColor leather = colors::GetF("brown");
  // Blade, tip, guard, grip, pommel; blade runs along +y from the guard (y = 0), grip toward -y
  const Spec specs[] = {
      {-0.07f, 0.0f, -0.018f, 0.07f, 1.35f, 0.018f, steel},
      {-0.045f, 1.35f, -0.012f, 0.045f, 1.6f, 0.012f, steel},
      {-0.30f, -0.04f, -0.05f, 0.30f, 0.04f, 0.05f, gold},
      {-0.045f, -0.40f, -0.045f, 0.045f, -0.04f, 0.045f, leather},
      {-0.08f, -0.52f, -0.08f, 0.08f, -0.40f, 0.08f, gold},
  };
  for (const Spec& s : specs) {
    std::vector<Vertex> v;
    std::vector<Uint32> i;
    Box(v, i, s.x0, s.y0, s.z0, s.x1, s.y1, s.z1);
    Part part;
    part.mesh = gpu.CreateMesh(v, i);
    part.color = s.color;
    if (part.mesh.index_count == 0) return false;
    parts_.push_back(part);
  }
  slash_ = gpu.CreateMesh({{-0.5f, 0, -0.5f, 0, 0},
                           {0.5f, 0, -0.5f, 1, 0},
                           {0.5f, 0, 0.5f, 1, 1},
                           {-0.5f, 0, 0.5f, 0, 1}},
                          {0, 1, 2, 0, 2, 3});
  return slash_.index_count > 0;
}

void Sword::Destroy(Gpu& gpu) {
  for (Part& part : parts_) gpu.DestroyMesh(part.mesh);
  parts_.clear();
  gpu.DestroyMesh(slash_);
}

void Sword::Play(const math::Vec3& target, const math::Vec3& from) {
  target_ = target;
  const math::Vec3 d = target - from;
  yaw_ = std::atan2(d.x, d.z);  // RotateY(yaw) points local +z along the strike
  time_ = 0.0f;
}

void Sword::Update(float dt) {
  if (Active()) time_ += dt;
}

void Sword::Render(Gpu& gpu, const math::Mat4& view_proj) const {
  if (!Active()) return;
  const float t = time_ / layout_.seconds;
  const float swing = SmoothStep(t / layout_.swing_end);
  const float angle = math::Lerp(layout_.angle_start, layout_.angle_end, swing);
  const float fade = t <= layout_.swing_end
                         ? 1.0f
                         : 1.0f - SmoothStep((t - layout_.swing_end) / (1.0f - layout_.swing_end));
  const float drop = layout_.drop * swing;
  const math::Mat4 world =
      math::Mat4::Translate({target_.x, target_.y + layout_.pivot_height - drop, target_.z}) *
      math::Mat4::RotateY(yaw_) * math::Mat4::Translate({0.0f, 0.0f, -layout_.pivot_back}) *
      math::Mat4::RotateX(angle) * math::Mat4::Scale(layout_.scale * fade);
  for (const Part& part : parts_) {
    gpu.Draw(part.mesh, nullptr, world, view_proj, part.color, Gpu::DrawMode::kMesh);
  }
  const float since_hit = time_ - layout_.seconds * layout_.swing_end;
  if (since_hit >= 0.0f && since_hit < layout_.slash_seconds) {
    const float s = since_hit / layout_.slash_seconds;
    const float length = layout_.slash_length * std::min(1.0f, s * layout_.slash_grow);
    SDL_FColor color = colors::GetF("ivory");
    color.a = layout_.slash_alpha * (1.0f - s);
    const math::Mat4 slash =
        math::Mat4::Translate({target_.x, target_.y + layout_.slash_lift, target_.z}) *
        math::Mat4::RotateY(yaw_ + layout_.slash_angle) *
        math::Mat4::Scale({layout_.slash_width * (1.0f + s), 1.0f, length});
    gpu.Draw(slash_, nullptr, slash, view_proj, color, Gpu::DrawMode::kEmissive);
  }
}
