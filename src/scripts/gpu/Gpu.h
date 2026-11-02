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
  math::Vec3 point2_pos{0, 0, 0};  // segunda luz pontual (ex.: segunda tocha), mesma cor
  float point2_intensity = 0.0f;
  SDL_FColor fog_color{0, 0, 0, 1};
  float fog_density = 0.0f;         // névoa por distância (0 desliga)
  float fog_start = 0.0f;           // distância da câmera onde a névoa por distância começa
  float fog_full_y = 0.0f;          // abaixo desta altura a névoa de altura é máxima
  float fog_clear_y = 1.0f;         // acima desta altura não há névoa de altura
  float fog_height_strength = 0.0f; // força máxima da névoa de altura (0 desliga)
  math::Vec3 camera_pos{0, 0, 0};
  float vignette = 0.0f;  // 0 desliga
  // Sombras: cada luz pontual tem um shadow map (ver BeginShadowPass). strength 0 desliga;
  // bias em profundidade normalizada do mapa, com inclinação aplicada no shader.
  float shadow_strength = 0.0f;
  float shadow_bias = 0.0015f;
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

  // Sombras (shadow mapping por luz pontual, perspectiva a partir da chama): antes de
  // BeginFrame, para cada luz (0 ou 1) abre um passe só de profundidade com a matriz
  // view-projection da luz, desenha os ocultadores com DrawShadow e fecha. Os Draws do quadro
  // amostram os dois mapas (PCF 3x3) e escurecem a luz correspondente. Falso se o backend não
  // suporta (as cenas seguem sem sombra).
  bool BeginShadowPass(int light, const math::Mat4& light_view_proj);
  void DrawShadow(const GpuMesh& mesh, const math::Mat4& model);
  void EndShadowPass();
  bool ShadowsSupported() const { return shadow_pipeline_ != nullptr; }

  // Um quadro: BeginFrame limpa a textura alvo, Draw enfileira malhas, EndFrame envia à GPU.
  bool BeginFrame(SDL_FColor clear);
  // Fecha o passe atual e abre outro mantendo a cor e limpando só a profundidade.
  // Permite desenhar camadas com câmeras diferentes (ex.: arena e depois a mão por cima).
  void NewLayer();
  // kMist: quad horizontal de névoa; a cor vem do tint e o alpha de ruído animado no shader.
  // kEmissive: sem iluminação nem névoa: a cor da textura sai como está.
  // kFlame: chama procedural animada no shader a partir do UV (u largura, v 0 topo .. 1 base).
  // kFlameGlow: a mesma chama como halo difuso (oval, base esmaecida), para um quad maior atrás.
  // kFoil: como kMesh, mais arco-íris holográfico que corre e se intensifica com a inclinação,
  // reflexo que varre a carta e estrelas que piscam (cartas raras).
  // kAura: brilho procedural transparente em volta de uma carta foil (ver CardAura).
  enum class DrawMode { kMesh, kMist, kEmissive, kFlame, kFlameGlow, kFoil, kAura };
  // normal_map (opcional, só com textura): mapa de normais em espaço tangente (OpenGL, verde
  // para cima); o shader monta o referencial tangente pelas derivadas de posição e UV.
  void Draw(const GpuMesh& mesh, SDL_GPUTexture* texture, const math::Mat4& model,
            const math::Mat4& view_proj, SDL_FColor tint, DrawMode mode = DrawMode::kMesh,
            SDL_GPUTexture* normal_map = nullptr);
  void EndFrame();

  // Vale para os Draws seguintes (pode trocar entre camadas).
  void SetAtmosphere(const Atmosphere& atmosphere) { atmosphere_ = atmosphere; }
  // Dessaturação dos Draws seguintes: 0 = cor normal, 1 = preto e branco (ex.: carta usada).
  void SetDesaturate(float amount) { desaturate_ = amount; }

  SDL_Texture* Target() const { return target_; }
  int Width() const { return width_; }
  int Height() const { return height_; }

 private:
  SDL_GPUShader* LoadShader(SDL_GPUShaderStage stage, const std::string& shader_dir,
                            const char* entrypoint, const char* spv_name, Uint32 num_samplers);
  bool InitShadows(const std::string& shader_dir, const SDL_GPUVertexInputState& vertex_input);
  SDL_GPUBuffer* UploadBuffer(SDL_GPUBufferUsageFlags usage, const void* data, Uint32 size);

  SDL_Renderer* renderer_ = nullptr;
  SDL_GPUDevice* device_ = nullptr;
  SDL_Texture* target_ = nullptr;
  SDL_GPUTexture* target_gpu_ = nullptr;
  SDL_GPUTexture* depth_ = nullptr;
  SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
  // Igual ao pipeline_, mas sem escrita de profundidade: névoa, chama e halo (transparentes)
  // testam profundidade contra o que é opaco, mas não escondem o que vier atrás depois.
  SDL_GPUGraphicsPipeline* pipeline_transparent_ = nullptr;
  SDL_GPUGraphicsPipeline* bound_pipeline_ = nullptr;
  SDL_GPUSampler* sampler_ = nullptr;
  SDL_GPUTexture* white_ = nullptr;        // textura 1x1 para malhas sem textura
  SDL_GPUTexture* flat_normal_ = nullptr;  // normal map 1x1 plano (128, 128, 255)
  // Shadow maps (um por luz pontual), sampler de comparação e pipeline só de profundidade.
  int shadow_size_ = 2048;  // lado do mapa, assets/Shadow.json (mapSize)
  SDL_GPUTexture* shadow_[2] = {nullptr, nullptr};
  SDL_GPUSampler* shadow_sampler_ = nullptr;
  SDL_GPUGraphicsPipeline* shadow_pipeline_ = nullptr;
  math::Mat4 light_vp_[2];
  int shadow_pass_light_ = -1;
  int width_ = 0;
  int height_ = 0;
  Atmosphere atmosphere_;
  float desaturate_ = 0.0f;
  float time_ = 0.0f;  // segundos desde o início, atualizado em BeginFrame

  bool BeginPass(SDL_GPULoadOp color_load, SDL_FColor clear);
  bool EnsureTarget();  // cria a textura alvo no renderer na primeira vez

  SDL_GPUCommandBuffer* cmd_ = nullptr;
  SDL_GPURenderPass* pass_ = nullptr;
};
