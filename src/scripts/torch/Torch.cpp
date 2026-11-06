#include "scripts/torch/Torch.h"

#include "scripts/config/Config.h"

#include <cmath>

void Torch::DrawBody(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj) const {
  for (const Model::Part& part : model_.Parts()) {
    if (part.name == "Flame") continue;
    gpu.Draw(part.mesh, part.texture, world, view_proj, SDL_FColor{1, 1, 1, 1},
             Gpu::DrawMode::kMesh, part.normal);
  }
}

void Torch::DrawBodyDepth(Gpu& gpu, const math::Mat4& world) const {
  for (const Model::Part& part : model_.Parts()) {
    if (part.name != "Flame") gpu.DrawShadow(part.mesh, world);
  }
}

void Torch::DrawFlame(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj,
                      const math::Vec3& camera_pos, float ignite) const {
  if (ignite <= 0.0f) return;
  const float t =
      static_cast<float>(std::fmod(static_cast<double>(SDL_GetTicksNS()) * 1e-9, 3600.0));
  const math::Vec3 base = world.TransformPoint({0.0f, kFlameBaseY, 0.0f});
  const float s = Scale(world);
  const float yaw = std::atan2(camera_pos.x - base.x, camera_pos.z - base.z);
  const float grow = 0.25f + 0.75f * ignite;  // grows from the base while igniting
  const float sy = grow * (1.0f + 0.06f * std::sin(t * 13.0f) + 0.04f * std::sin(t * 7.3f + 1.0f));
  const float sx = grow * (1.0f + 0.04f * std::sin(t * 9.1f + 2.0f));
  const math::Vec3 normal{std::sin(yaw), 0.0f, std::cos(yaw)};
  // The quad writes depth: the glow sits slightly behind the flame and is drawn first
  auto flame_matrix = [&](float wx, float wy, float back) {
    return math::Mat4::Translate(base - normal * (back * s)) * math::Mat4::RotateY(yaw) *
           math::Mat4::Scale({s * sx * wx, s * sy * wy, s}) *
           math::Mat4::Translate({0.0f, -kFlameBaseY, 0.0f});
  };
  const SDL_FColor light = LightColor();
  const float glow_alpha = config::Float("light", "torch.glowAlpha", 0.22f);
  for (const Model::Part& part : model_.Parts()) {
    if (part.name != "Flame") continue;
    gpu.Draw(part.mesh, nullptr, flame_matrix(2.1f, 1.3f, 0.04f), view_proj,
             SDL_FColor{light.r, light.g, light.b, glow_alpha * ignite},
             Gpu::DrawMode::kFlameGlow);
    gpu.Draw(part.mesh, nullptr, flame_matrix(1.0f, 1.0f, 0.0f), view_proj,
             SDL_FColor{1, 1, 1, ignite}, Gpu::DrawMode::kFlame);
  }
}

SDL_FColor Torch::LightColor() {
  return config::Color("light", "torch.color", SDL_FColor{1.0f, 0.96f, 0.82f, 1.0f});
}
