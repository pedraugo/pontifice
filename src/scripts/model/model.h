#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "scripts/gpu/gpu.h"
#include "scripts/image/image.h"
#include "scripts/math/math.h"

class Model {
 public:
  // Role comes from the glb node name: front/frente, back/verso, otherwise edge
  enum class Role { kEdge, kFront, kBack };

  struct Part {
    std::string name;
    Role role = Role::kEdge;
    GpuMesh mesh;
    SDL_GPUTexture* texture = nullptr;
    SDL_GPUTexture* normal = nullptr;
    SDL_FColor color{1, 1, 1, 1};
  };

  // Decoded glb, vertices in world space; Parse may run off the main thread, Upload cannot
  struct Data {
    struct PartData {
      std::string name;
      std::vector<Vertex> vertices;
      std::vector<Uint32> indices;
      Image texture;
      Image normal;
      SDL_FColor color{1, 1, 1, 1};
    };
    std::string path;
    std::vector<PartData> parts;
    math::Vec3 min{0, 0, 0};
    math::Vec3 max{0, 0, 0};
    bool ok = false;
  };

  Model() = default;
  ~Model();
  Model(const Model&) = delete;
  Model& operator=(const Model&) = delete;

  static Data Parse(const std::string& path);
  bool Upload(Gpu& gpu, Data&& data);
  bool LoadGlb(Gpu& gpu, const std::string& path) { return Upload(gpu, Parse(path)); }

  // Front/back textures override the embedded ones when non-null; front_mode e.g. kFoil
  struct Skin {
    SDL_GPUTexture* front = nullptr;
    SDL_GPUTexture* back = nullptr;
    SDL_GPUTexture* front_normal = nullptr;
    SDL_GPUTexture* back_normal = nullptr;
    Gpu::DrawMode front_mode = Gpu::DrawMode::kMesh;
  };

  void Draw(Gpu& gpu, const math::Mat4& model, const math::Mat4& view_proj, SDL_FColor tint,
            const Skin& skin) const;
  void DrawDepth(Gpu& gpu, const math::Mat4& model) const {
    for (const Part& part : parts_) gpu.DrawShadow(part.mesh, model);
  }
  void Draw(Gpu& gpu, const math::Mat4& model, const math::Mat4& view_proj, SDL_FColor tint,
            SDL_GPUTexture* front = nullptr, SDL_GPUTexture* back = nullptr,
            Gpu::DrawMode front_mode = Gpu::DrawMode::kMesh) const {
    Draw(gpu, model, view_proj, tint, Skin{front, back, nullptr, nullptr, front_mode});
  }

  const std::vector<Part>& Parts() const { return parts_; }
  math::Vec3 BoundsMin() const { return min_; }
  math::Vec3 BoundsMax() const { return max_; }

 private:
  Gpu* gpu_ = nullptr;
  std::vector<Part> parts_;
  math::Vec3 min_{0, 0, 0};
  math::Vec3 max_{0, 0, 0};
};
