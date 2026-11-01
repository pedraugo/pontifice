#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "scripts/gpu/Gpu.h"
#include "scripts/math/Math.h"
#include "scripts/model/Model.h"

// Tocha (models/Torch.glb, gerada por tools/make_torch.py): corpo iluminado normalmente e
// chama procedural no shader (Gpu::DrawMode::kFlame) num quad que fica sempre na vertical e
// virado para a câmera, mais um halo maior e translúcido. É a fonte de luz das cenas:
// FlamePosition dá onde colocar a luz pontual para uma dada matriz de mundo.
class Torch {
 public:
  // Medidas do modelo (mesmas de tools/make_torch.py).
  static constexpr float kFlameBaseY = 0.82f;
  static constexpr float kFlameCenterY = 1.04f;

  bool LoadGlb(Gpu& gpu, const std::string& path) { return model_.LoadGlb(gpu, path); }
  bool Upload(Gpu& gpu, Model::Data&& data) { return model_.Upload(gpu, std::move(data)); }

  // Corpo (opaco) e chama (transparente) separados: a cena desenha o corpo antes das camadas
  // de névoa e a chama depois delas, senão o quad da chama, que escreve profundidade, abre um
  // buraco retangular na névoa atrás dele. camera_pos orienta o quad (billboard em torno de y).
  void DrawBody(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj) const;
  void DrawBodyDepth(Gpu& gpu, const math::Mat4& world) const;  // passe de sombra (sem chama)
  // ignite: 0 = apagada, 1 = chama inteira (cresce a partir da base, como ao acender).
  void DrawFlame(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj,
                 const math::Vec3& camera_pos, float ignite = 1.0f) const;

  // Centro da chama no mundo: a chama sobe na vertical a partir da base, mesmo com a tocha
  // inclinada, então o centro fica acima da base e não ao longo do cabo.
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
