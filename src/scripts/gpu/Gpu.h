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

// Luz e atmosfera de um passe: luz principal direcional, luz pontual, névoa e vinheta.
// Valores padrão = iluminação neutra sem névoa (usado pela mão).
struct Atmosphere {
  math::Vec3 light_dir{0.25f, 0.5f, 1.0f};  // direção PARA a luz; normalizada no envio
  float light_intensity = 0.4f;
  SDL_FColor light_color{1.0f, 1.0f, 1.0f, 1.0f};
  SDL_FColor ambient{0.6f, 0.6f, 0.6f, 1.0f};
  math::Vec3 point_pos{0, 0, 0};
  float point_intensity = 0.0f;  // 0 desliga
  SDL_FColor fog_color{0, 0, 0, 1};
  float fog_density = 0.0f;         // névoa por distância (0 desliga)
  float fog_start = 0.0f;           // distância da câmera onde a névoa por distância começa
  float fog_full_y = 0.0f;          // abaixo desta altura a névoa de altura é máxima
  float fog_clear_y = 1.0f;         // acima desta altura não há névoa de altura
  float fog_height_strength = 0.0f; // força máxima da névoa de altura (0 desliga)
  math::Vec3 camera_pos{0, 0, 0};
  float vignette = 0.0f;  // 0 desliga
  // Névoa de chão (DrawMode::kMist): some entre estes raios a partir da origem (xz) e usa
  // esta escala de ruído (maior = fiapos menores).
  float mist_fade_start = 8.0f;
  float mist_fade_end = 28.0f;
  float mist_noise_scale = 1.0f;
  float mist_density = 0.0f;  // desloca o limiar do ruído: 0 = fiapos da arena, ~0.3 = manto
};

// Camada fina sobre o SDL_GPU: desenha malhas texturizadas com profundidade em uma textura
// alvo que depois é apresentada pelo SDL_Renderer como uma SDL_Texture comum.
class Gpu {
 public:
  Gpu() = default;
  ~Gpu();
  Gpu(const Gpu&) = delete;
  Gpu& operator=(const Gpu&) = delete;

  // Thread principal: device do renderer + InitDevice + SetRenderer.
  bool Init(SDL_Renderer* renderer, int width, int height, const std::string& shader_dir);
  // Qualquer thread: pipeline, sampler, profundidade e textura branca a partir do device.
  // Depois disso CreateMesh/CreateTexture já funcionam (carga em segundo plano).
  bool InitDevice(SDL_GPUDevice* device, int width, int height, const std::string& shader_dir);
  // Thread principal, antes do primeiro BeginFrame: renderer que apresenta a textura alvo
  // (criada aí, preguiçosamente).
  void SetRenderer(SDL_Renderer* renderer) { renderer_ = renderer; }

  GpuMesh CreateMesh(const std::vector<Vertex>& vertices, const std::vector<Uint32>& indices);
  void DestroyMesh(GpuMesh& mesh);

  SDL_GPUTexture* CreateTexture(const Image& image);
  void DestroyTexture(SDL_GPUTexture* texture);

  // Um quadro: BeginFrame limpa a textura alvo, Draw enfileira malhas, EndFrame envia à GPU.
  bool BeginFrame(SDL_FColor clear);
  // Fecha o passe atual e abre outro mantendo a cor e limpando só a profundidade.
  // Permite desenhar camadas com câmeras diferentes (ex.: arena e depois a mão por cima).
  void NewLayer();
  // kMist: quad horizontal de névoa; a cor vem do tint e o alpha de ruído animado no shader.
  enum class DrawMode { kMesh, kMist };
  void Draw(const GpuMesh& mesh, SDL_GPUTexture* texture, const math::Mat4& model,
            const math::Mat4& view_proj, SDL_FColor tint, DrawMode mode = DrawMode::kMesh);
  void EndFrame();

  // Vale para os Draws seguintes (pode trocar entre camadas).
  void SetAtmosphere(const Atmosphere& atmosphere) { atmosphere_ = atmosphere; }

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
  Atmosphere atmosphere_;
  float time_ = 0.0f;  // segundos desde o início, atualizado em BeginFrame

  bool BeginPass(SDL_GPULoadOp color_load, SDL_FColor clear);
  bool EnsureTarget();  // cria a textura alvo no renderer na primeira vez

  SDL_GPUCommandBuffer* cmd_ = nullptr;
  SDL_GPURenderPass* pass_ = nullptr;
};
