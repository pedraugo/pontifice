#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "scripts/gpu/Gpu.h"
#include "scripts/image/Image.h"
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
    SDL_GPUTexture* normal = nullptr;   // normalTexture do material, pode ser nula
    SDL_FColor color{1, 1, 1, 1};       // baseColorFactor do material
  };

  // Conteúdo de um .glb já decodificado na CPU (vértices em coordenadas do mundo, índices,
  // textura embutida como Image). Parse pode rodar em outra thread; Upload cria os recursos
  // de GPU e precisa da thread principal.
  struct Data {
    struct PartData {
      std::string name;
      std::vector<Vertex> vertices;
      std::vector<Uint32> indices;
      Image texture;  // vazia se a parte não tem textura
      Image normal;   // normal map (vazia se não tem)
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
  // Parse + Upload na thread principal.
  bool LoadGlb(Gpu& gpu, const std::string& path) { return Upload(gpu, Parse(path)); }

  // Texturas por papel para um desenho: frente e verso substituem as embutidas quando não
  // nulas; normal maps são opcionais; front_mode é o modo da frente (ex.: kFoil).
  struct Skin {
    SDL_GPUTexture* front = nullptr;
    SDL_GPUTexture* back = nullptr;
    SDL_GPUTexture* front_normal = nullptr;
    SDL_GPUTexture* back_normal = nullptr;
    Gpu::DrawMode front_mode = Gpu::DrawMode::kMesh;
  };

  // Desenha todas as partes.
  void Draw(Gpu& gpu, const math::Mat4& model, const math::Mat4& view_proj, SDL_FColor tint,
            const Skin& skin) const;
  // Só profundidade, para o passe de sombra (Gpu::BeginShadowPass).
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
