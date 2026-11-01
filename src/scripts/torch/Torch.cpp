#include "scripts/torch/Torch.h"

#include <cmath>

void Torch::DrawBody(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj) const {
  for (const Model::Part& part : model_.Parts()) {
    if (part.name == "Flame") continue;
    gpu.Draw(part.mesh, part.texture, world, view_proj, SDL_FColor{1, 1, 1, 1});
  }
}

void Torch::DrawFlame(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj,
                      const math::Vec3& camera_pos) const {
  // Chama: parte da base no mundo, sobe na vertical (ignora a inclinação do cabo), encara a
  // câmera girando em torno de y e tremula de leve em tamanho. O shader faz o resto.
  const float t =
      static_cast<float>(std::fmod(static_cast<double>(SDL_GetTicksNS()) * 1e-9, 3600.0));
  const math::Vec3 base = world.TransformPoint({0.0f, kFlameBaseY, 0.0f});
  const float s = Scale(world);
  const float yaw = std::atan2(camera_pos.x - base.x, camera_pos.z - base.z);
  const float sy = 1.0f + 0.06f * std::sin(t * 13.0f) + 0.04f * std::sin(t * 7.3f + 1.0f);
  const float sx = 1.0f + 0.04f * std::sin(t * 9.1f + 2.0f);
  // O quad escreve profundidade: o halo fica um pouco atrás da chama (ao longo da normal, que
  // aponta para a câmera) e é desenhado antes, senão um dos dois perde o teste de profundidade.
  const math::Vec3 normal{std::sin(yaw), 0.0f, std::cos(yaw)};
  auto flame_matrix = [&](float wx, float wy, float back) {
    return math::Mat4::Translate(base - normal * (back * s)) * math::Mat4::RotateY(yaw) *
           math::Mat4::Scale({s * sx * wx, s * sy * wy, s}) *
           math::Mat4::Translate({0.0f, -kFlameBaseY, 0.0f});
  };
  for (const Model::Part& part : model_.Parts()) {
    if (part.name != "Flame") continue;
    // Halo: a mesma chama, maior e bem translúcida, dá o brilho difuso em volta.
    gpu.Draw(part.mesh, nullptr, flame_matrix(2.1f, 1.3f, 0.04f), view_proj,
             SDL_FColor{1.0f, 0.75f, 0.45f, 0.22f}, Gpu::DrawMode::kFlameGlow);
    gpu.Draw(part.mesh, nullptr, flame_matrix(1.0f, 1.0f, 0.0f), view_proj,
             SDL_FColor{1, 1, 1, 1}, Gpu::DrawMode::kFlame);
  }
}
