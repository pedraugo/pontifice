#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "scripts/math/Math.h"

struct Image;

// Vértice usado por todos os modelos: posição + UV.
struct Vertex {
  float px, py, pz;
  float u, v;
};

struct GpuMesh {
  SDL_GPUBuffer* vertex_buffer = nullptr;
  SDL_GPUBuffer* index_buffer = nullptr;
  Uint32 index_count = 0;
};

// Camada fina sobre o SDL_GPU: desenha malhas texturizadas com profundidade em uma textura
// alvo que depois é apresentada pelo SDL_Renderer como uma SDL_Texture comum.
class Gpu {
 public:
  Gpu() = default;
  ~Gpu();
  Gpu(const Gpu&) = delete;
  Gpu& operator=(const Gpu&) = delete;

  bool Init(SDL_Renderer* renderer, int width, int height, const std::string& shader_dir);

  GpuMesh CreateMesh(const std::vector<Vertex>& vertices, const std::vector<Uint32>& indices);
  void DestroyMesh(GpuMesh& mesh);

  SDL_GPUTexture* CreateTexture(const Image& image);
  void DestroyTexture(SDL_GPUTexture* texture);

  // Um quadro: BeginFrame limpa a textura alvo, Draw enfileira malhas, EndFrame envia à GPU.
  bool BeginFrame(SDL_FColor clear);
  // Fecha o passe atual e abre outro mantendo a cor e limpando só a profundidade.
  // Permite desenhar camadas com câmeras diferentes (ex.: arena e depois a mão por cima).
  void NewLayer();
  void Draw(const GpuMesh& mesh, SDL_GPUTexture* texture, const math::Mat4& model,
            const math::Mat4& view_proj, SDL_FColor tint);
  void EndFrame();

  SDL_Texture* Target() const { return target_; }
  int Width() const { return width_; }
  int Height() const { return height_; }

 private:
  SDL_GPUShader* LoadShader(SDL_GPUShaderStage stage, const std::string& shader_dir);
  SDL_GPUBuffer* UploadBuffer(SDL_GPUBufferUsageFlags usage, const void* data, Uint32 size);

  SDL_Renderer* renderer_ = nullptr;
  SDL_GPUDevice* device_ = nullptr;
  SDL_Texture* target_ = nullptr;
  SDL_GPUTexture* target_gpu_ = nullptr;
  SDL_GPUTexture* depth_ = nullptr;
  SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
  SDL_GPUSampler* sampler_ = nullptr;
  SDL_GPUTexture* white_ = nullptr;  // textura 1x1 para malhas sem textura
  int width_ = 0;
  int height_ = 0;

  bool BeginPass(SDL_GPULoadOp color_load, SDL_FColor clear);

  SDL_GPUCommandBuffer* cmd_ = nullptr;
  SDL_GPURenderPass* pass_ = nullptr;
};
