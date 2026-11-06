#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "scripts/math/math.h"

struct Image;

struct Vertex {
  float px, py, pz;
  float u, v;
};

struct GpuMesh {
  SDL_GPUBuffer* vertex_buffer = nullptr;
  SDL_GPUBuffer* index_buffer = nullptr;
  Uint32 index_count = 0;
};

struct Atmosphere {
  math::Vec3 light_dir{0.25f, 0.5f, 1.0f};  // direction toward the light; normalized on upload
  float light_intensity = 0.4f;
  SDL_FColor light_color{1.0f, 1.0f, 1.0f, 1.0f};
  SDL_FColor ambient{0.6f, 0.6f, 0.6f, 1.0f};
  math::Vec3 point_pos{0, 0, 0};
  float point_intensity = 0.0f;
  math::Vec3 point2_pos{0, 0, 0};
  float point2_intensity = 0.0f;
  SDL_FColor fog_color{0, 0, 0, 1};
  float fog_density = 0.0f;
  float fog_start = 0.0f;
  float fog_full_y = 0.0f;
  float fog_clear_y = 1.0f;
  float fog_height_strength = 0.0f;
  math::Vec3 camera_pos{0, 0, 0};
  float vignette = 0.0f;
  float shadow_strength = 0.0f;
  float shadow_bias = 0.0015f;  // normalized depth-map units; slope bias added in shader
  // Ground mist (kMist) fades out between these xz radii; density shifts the noise threshold
  float mist_fade_start = 8.0f;
  float mist_fade_end = 28.0f;
  float mist_noise_scale = 1.0f;
  float mist_density = 0.0f;
};

class Gpu {
 public:
  Gpu() = default;
  ~Gpu();
  Gpu(const Gpu&) = delete;
  Gpu& operator=(const Gpu&) = delete;

  bool Init(SDL_Renderer* renderer, int width, int height, const std::string& shader_dir);
  // Any thread; CreateMesh/CreateTexture work after this (background loading)
  bool InitDevice(SDL_GPUDevice* device, int width, int height, const std::string& shader_dir);
  void SetRenderer(SDL_Renderer* renderer) { renderer_ = renderer; }

  GpuMesh CreateMesh(const std::vector<Vertex>& vertices, const std::vector<Uint32>& indices);
  void DestroyMesh(GpuMesh& mesh);

  SDL_GPUTexture* CreateTexture(const Image& image);
  void DestroyTexture(SDL_GPUTexture* texture);

  // Depth-only pass for point light 0 or 1, run before BeginFrame; Draw samples both maps
  bool BeginShadowPass(int light, const math::Mat4& light_view_proj);
  void DrawShadow(const GpuMesh& mesh, const math::Mat4& model);
  void EndShadowPass();

  bool BeginFrame(SDL_FColor clear);
  // Keeps the color target, clears depth only: the next layer may use another camera
  void NewLayer();
  // kFlame/kFlameGlow: procedural flame from UV, v = 0 at the top, 1 at the base
  enum class DrawMode { kMesh, kMist, kEmissive, kFlame, kFlameGlow, kFoil, kAura };
  // normal_map: tangent space, OpenGL convention (green up); used only with a texture
  void Draw(const GpuMesh& mesh, SDL_GPUTexture* texture, const math::Mat4& model,
            const math::Mat4& view_proj, SDL_FColor tint, DrawMode mode = DrawMode::kMesh,
            SDL_GPUTexture* normal_map = nullptr);
  void EndFrame();

  void SetAtmosphere(const Atmosphere& atmosphere) { atmosphere_ = atmosphere; }
  void SetDesaturate(float amount) { desaturate_ = amount; }

  SDL_Texture* Target() const { return target_; }

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
  // Same as pipeline_ without depth write: mist, flame and glow never occlude later draws
  SDL_GPUGraphicsPipeline* pipeline_transparent_ = nullptr;
  SDL_GPUGraphicsPipeline* bound_pipeline_ = nullptr;
  SDL_GPUSampler* sampler_ = nullptr;
  SDL_GPUTexture* white_ = nullptr;
  SDL_GPUTexture* flat_normal_ = nullptr;
  int shadow_size_ = 2048;
  SDL_GPUTexture* shadow_[2] = {nullptr, nullptr};
  SDL_GPUSampler* shadow_sampler_ = nullptr;
  SDL_GPUGraphicsPipeline* shadow_pipeline_ = nullptr;
  math::Mat4 light_vp_[2];
  int shadow_pass_light_ = -1;
  int width_ = 0;
  int height_ = 0;
  Atmosphere atmosphere_;
  float desaturate_ = 0.0f;
  float time_ = 0.0f;

  bool BeginPass(SDL_GPULoadOp color_load, SDL_FColor clear);
  bool EnsureTarget();

  SDL_GPUCommandBuffer* cmd_ = nullptr;
  SDL_GPURenderPass* pass_ = nullptr;
};
