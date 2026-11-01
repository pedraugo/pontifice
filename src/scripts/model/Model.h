#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "scripts/gpu/Gpu.h"
#include "scripts/math/Math.h"

// Modelo carregado de um .glb. Cada parte é uma malha com textura própria ou cor sólida.
// Para a carta, as partes são classificadas pelo nome do nó: frente, verso e borda.
class Model {
 public:
  enum class Role { kEdge, kFront, kBack };

  struct Part {
    std::string name;
    Role role = Role::kEdge;
    GpuMesh mesh;
    SDL_GPUTexture* texture = nullptr;  // textura embutida no glb, pode ser nula
    SDL_FColor color{1, 1, 1, 1};       // baseColorFactor do material
  };

  Model() = default;
  ~Model();
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  bool LoadGlb(Gpu& gpu, const std::string& path);

  // Desenha todas as partes. front/back substituem as texturas embutidas quando não nulos.
  void Draw(Gpu& gpu, const math::Mat4& model, const math::Mat4& view_proj, SDL_FColor tint,
            SDL_GPUTexture* front = nullptr, SDL_GPUTexture* back = nullptr) const;

  const std::vector<Part>& Parts() const { return parts_; }
  math::Vec3 BoundsMin() const { return min_; }
  math::Vec3 BoundsMax() const { return max_; }

 private:
  Gpu* gpu_ = nullptr;
  std::vector<Part> parts_;
  math::Vec3 min_{0, 0, 0};
  math::Vec3 max_{0, 0, 0};
};
