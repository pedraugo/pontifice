#include "scripts/gpu/Gpu.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>

#include "scripts/image/Image.h"

namespace {

// Fonte MSL embutida: o Metal compila em tempo de execução, sem ferramenta externa.
const unsigned char kMslSource[] = {
#include "scripts/gpu/card_metal.inc"
};

std::vector<Uint8> ReadFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  return std::vector<Uint8>(std::istreambuf_iterator<char>(in), {});
}

Uint32 MipLevels(int w, int h) {
  Uint32 levels = 1;
  int size = std::max(w, h);
  while (size > 1) {
    size /= 2;
    ++levels;
  }
  return levels;
}

}  // namespace

Gpu::~Gpu() {
  if (device_ == nullptr) return;
  if (pipeline_ != nullptr) SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
  if (sampler_ != nullptr) SDL_ReleaseGPUSampler(device_, sampler_);
  if (depth_ != nullptr) SDL_ReleaseGPUTexture(device_, depth_);
  if (white_ != nullptr) SDL_ReleaseGPUTexture(device_, white_);
  if (target_ != nullptr) SDL_DestroyTexture(target_);
}

bool Gpu::Init(SDL_Renderer* renderer, int width, int height, const std::string& shader_dir) {
  renderer_ = renderer;
  width_ = width;
  height_ = height;

  device_ = SDL_GetGPURendererDevice(renderer);
  if (device_ == nullptr) {
    SDL_Log("Renderer nao e GPU: %s", SDL_GetError());
    return false;
  }

  // Textura alvo: criada pelo renderer para poder ser desenhada com SDL_RenderTexture,
  // e acessada aqui como SDL_GPUTexture para servir de color target.
  target_ = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, width,
                              height);
  if (target_ == nullptr) {
    SDL_Log("SDL_CreateTexture (alvo 3D): %s", SDL_GetError());
    return false;
  }
  SDL_SetTextureBlendMode(target_, SDL_BLENDMODE_BLEND);
  target_gpu_ = static_cast<SDL_GPUTexture*>(SDL_GetPointerProperty(
      SDL_GetTextureProperties(target_), SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr));
  if (target_gpu_ == nullptr) {
    SDL_Log("Textura alvo sem SDL_GPUTexture associada");
    return false;
  }
  const SDL_GPUTextureFormat color_format =
      SDL_GetGPUTextureFormatFromPixelFormat(SDL_PIXELFORMAT_RGBA32);

  SDL_GPUTextureFormat depth_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
  if (SDL_GPUTextureSupportsFormat(device_, SDL_GPU_TEXTUREFORMAT_D24_UNORM,
                                   SDL_GPU_TEXTURETYPE_2D,
                                   SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
    depth_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
  }
  SDL_GPUTextureCreateInfo depth_info{};
  depth_info.type = SDL_GPU_TEXTURETYPE_2D;
  depth_info.format = depth_format;
  depth_info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
  depth_info.width = width;
  depth_info.height = height;
  depth_info.layer_count_or_depth = 1;
  depth_info.num_levels = 1;
  depth_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  depth_ = SDL_CreateGPUTexture(device_, &depth_info);
  if (depth_ == nullptr) {
    SDL_Log("SDL_CreateGPUTexture (depth): %s", SDL_GetError());
    return false;
  }

  SDL_GPUShader* vs = LoadShader(SDL_GPU_SHADERSTAGE_VERTEX, shader_dir);
  SDL_GPUShader* fs = LoadShader(SDL_GPU_SHADERSTAGE_FRAGMENT, shader_dir);
  if (vs == nullptr || fs == nullptr) return false;

  SDL_GPUVertexBufferDescription vb_desc{};
  vb_desc.slot = 0;
  vb_desc.pitch = sizeof(Vertex);
  vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

  SDL_GPUVertexAttribute attrs[2]{};
  attrs[0].location = 0;
  attrs[0].buffer_slot = 0;
  attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
  attrs[0].offset = offsetof(Vertex, px);
  attrs[1].location = 1;
  attrs[1].buffer_slot = 0;
  attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
  attrs[1].offset = offsetof(Vertex, u);

  SDL_GPUColorTargetDescription color_desc{};
  color_desc.format = color_format;
  color_desc.blend_state.enable_blend = true;
  color_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
  color_desc.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  color_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
  color_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
  color_desc.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
  color_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

  SDL_GPUGraphicsPipelineCreateInfo info{};
  info.vertex_shader = vs;
  info.fragment_shader = fs;
  info.vertex_input_state.vertex_buffer_descriptions = &vb_desc;
  info.vertex_input_state.num_vertex_buffers = 1;
  info.vertex_input_state.vertex_attributes = attrs;
  info.vertex_input_state.num_vertex_attributes = 2;
  info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
  info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
  info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
  info.rasterizer_state.enable_depth_clip = true;
  info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
  info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
  info.depth_stencil_state.enable_depth_test = true;
  info.depth_stencil_state.enable_depth_write = true;
  info.target_info.color_target_descriptions = &color_desc;
  info.target_info.num_color_targets = 1;
  info.target_info.depth_stencil_format = depth_format;
  info.target_info.has_depth_stencil_target = true;

  pipeline_ = SDL_CreateGPUGraphicsPipeline(device_, &info);
  SDL_ReleaseGPUShader(device_, vs);
  SDL_ReleaseGPUShader(device_, fs);
  if (pipeline_ == nullptr) {
    SDL_Log("SDL_CreateGPUGraphicsPipeline: %s", SDL_GetError());
    return false;
  }

  SDL_GPUSamplerCreateInfo sampler_info{};
  sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
  sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
  sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
  sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
  sampler_info.max_anisotropy = 8.0f;
  sampler_info.enable_anisotropy = true;
  sampler_info.max_lod = 1000.0f;
  sampler_ = SDL_CreateGPUSampler(device_, &sampler_info);
  if (sampler_ == nullptr) {
    SDL_Log("SDL_CreateGPUSampler: %s", SDL_GetError());
    return false;
  }

  Image white;
  white.width = 1;
  white.height = 1;
  white.rgba = {255, 255, 255, 255};
  white_ = CreateTexture(white);
  return white_ != nullptr;
}

SDL_GPUShader* Gpu::LoadShader(SDL_GPUShaderStage stage, const std::string& shader_dir) {
  const bool vertex = stage == SDL_GPU_SHADERSTAGE_VERTEX;
  const SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(device_);

  SDL_GPUShaderCreateInfo info{};
  info.stage = stage;
  info.num_samplers = vertex ? 0 : 1;
  info.num_uniform_buffers = 1;

  std::vector<Uint8> bytes;
  if (formats & SDL_GPU_SHADERFORMAT_MSL) {
    info.format = SDL_GPU_SHADERFORMAT_MSL;
    info.code = reinterpret_cast<const Uint8*>(kMslSource);
    info.code_size = sizeof(kMslSource) - 1;  // sem o terminador nulo
    info.entrypoint = vertex ? "vs_main" : "fs_main";
  } else if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
    const std::string path = shader_dir + (vertex ? "/card.vert.spv" : "/card.frag.spv");
    bytes = ReadFile(path);
    if (bytes.empty()) {
      SDL_Log("Shader SPIR-V nao encontrado: %s (compile com glslc, veja o README)",
              path.c_str());
      return nullptr;
    }
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.code = bytes.data();
    info.code_size = bytes.size();
    info.entrypoint = "main";
  } else {
    SDL_Log("Nenhum formato de shader suportado neste backend (formats=0x%x)", formats);
    return nullptr;
  }

  SDL_GPUShader* shader = SDL_CreateGPUShader(device_, &info);
  if (shader == nullptr) SDL_Log("SDL_CreateGPUShader: %s", SDL_GetError());
  return shader;
}

SDL_GPUBuffer* Gpu::UploadBuffer(SDL_GPUBufferUsageFlags usage, const void* data, Uint32 size) {
  SDL_GPUBufferCreateInfo buf_info{};
  buf_info.usage = usage;
  buf_info.size = size;
  SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device_, &buf_info);
  if (buffer == nullptr) {
    SDL_Log("SDL_CreateGPUBuffer: %s", SDL_GetError());
    return nullptr;
  }

  SDL_GPUTransferBufferCreateInfo tb_info{};
  tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  tb_info.size = size;
  SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(device_, &tb_info);
  void* mapped = SDL_MapGPUTransferBuffer(device_, tb, false);
  std::memcpy(mapped, data, size);
  SDL_UnmapGPUTransferBuffer(device_, tb);

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device_);
  SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
  SDL_GPUTransferBufferLocation src{tb, 0};
  SDL_GPUBufferRegion dst{buffer, 0, size};
  SDL_UploadToGPUBuffer(copy, &src, &dst, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_ReleaseGPUTransferBuffer(device_, tb);
  return buffer;
}

GpuMesh Gpu::CreateMesh(const std::vector<Vertex>& vertices, const std::vector<Uint32>& indices) {
  GpuMesh mesh;
  mesh.vertex_buffer =
      UploadBuffer(SDL_GPU_BUFFERUSAGE_VERTEX, vertices.data(),
                   static_cast<Uint32>(vertices.size() * sizeof(Vertex)));
  mesh.index_buffer = UploadBuffer(SDL_GPU_BUFFERUSAGE_INDEX, indices.data(),
                                   static_cast<Uint32>(indices.size() * sizeof(Uint32)));
  mesh.index_count = static_cast<Uint32>(indices.size());
  return mesh;
}

void Gpu::DestroyMesh(GpuMesh& mesh) {
  if (mesh.vertex_buffer != nullptr) SDL_ReleaseGPUBuffer(device_, mesh.vertex_buffer);
  if (mesh.index_buffer != nullptr) SDL_ReleaseGPUBuffer(device_, mesh.index_buffer);
  mesh = GpuMesh{};
}

SDL_GPUTexture* Gpu::CreateTexture(const Image& image) {
  if (!image.Valid()) return nullptr;
  const Uint32 w = static_cast<Uint32>(image.width);
  const Uint32 h = static_cast<Uint32>(image.height);
  const Uint32 levels = MipLevels(image.width, image.height);

  SDL_GPUTextureCreateInfo info{};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  if (levels > 1) info.usage |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;  // exigido para gerar mips
  info.width = w;
  info.height = h;
  info.layer_count_or_depth = 1;
  info.num_levels = levels;
  info.sample_count = SDL_GPU_SAMPLECOUNT_1;
  SDL_GPUTexture* texture = SDL_CreateGPUTexture(device_, &info);
  if (texture == nullptr) {
    SDL_Log("SDL_CreateGPUTexture: %s", SDL_GetError());
    return nullptr;
  }

  const Uint32 size = w * h * 4;
  SDL_GPUTransferBufferCreateInfo tb_info{};
  tb_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  tb_info.size = size;
  SDL_GPUTransferBuffer* tb = SDL_CreateGPUTransferBuffer(device_, &tb_info);
  void* mapped = SDL_MapGPUTransferBuffer(device_, tb, false);
  std::memcpy(mapped, image.rgba.data(), size);
  SDL_UnmapGPUTransferBuffer(device_, tb);

  SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(device_);
  SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
  SDL_GPUTextureTransferInfo src{};
  src.transfer_buffer = tb;
  src.pixels_per_row = w;
  src.rows_per_layer = h;
  SDL_GPUTextureRegion dst{};
  dst.texture = texture;
  dst.w = w;
  dst.h = h;
  dst.d = 1;
  SDL_UploadToGPUTexture(copy, &src, &dst, false);
  SDL_EndGPUCopyPass(copy);
  if (levels > 1) SDL_GenerateMipmapsForGPUTexture(cmd, texture);
  SDL_SubmitGPUCommandBuffer(cmd);
  SDL_ReleaseGPUTransferBuffer(device_, tb);
  return texture;
}

void Gpu::DestroyTexture(SDL_GPUTexture* texture) {
  if (texture != nullptr) SDL_ReleaseGPUTexture(device_, texture);
}

bool Gpu::BeginFrame(SDL_FColor clear) {
  // Garante que tudo que o renderer 2D enfileirou antes vai para a GPU antes do passe 3D.
  SDL_FlushRenderer(renderer_);

  cmd_ = SDL_AcquireGPUCommandBuffer(device_);
  if (cmd_ == nullptr) {
    SDL_Log("SDL_AcquireGPUCommandBuffer: %s", SDL_GetError());
    return false;
  }

  return BeginPass(SDL_GPU_LOADOP_CLEAR, clear);
}

void Gpu::NewLayer() {
  if (pass_ == nullptr) return;
  SDL_EndGPURenderPass(pass_);
  pass_ = nullptr;
  BeginPass(SDL_GPU_LOADOP_LOAD, SDL_FColor{0, 0, 0, 0});
}

bool Gpu::BeginPass(SDL_GPULoadOp color_load, SDL_FColor clear) {
  SDL_GPUColorTargetInfo color{};
  color.texture = target_gpu_;
  color.clear_color = clear;
  color.load_op = color_load;
  color.store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPUDepthStencilTargetInfo depth{};
  depth.texture = depth_;
  depth.clear_depth = 1.0f;
  depth.load_op = SDL_GPU_LOADOP_CLEAR;
  depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
  depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
  depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

  pass_ = SDL_BeginGPURenderPass(cmd_, &color, 1, &depth);
  SDL_BindGPUGraphicsPipeline(pass_, pipeline_);

  SDL_GPUViewport viewport{0, 0, static_cast<float>(width_), static_cast<float>(height_), 0.0f,
                           1.0f};
  SDL_SetGPUViewport(pass_, &viewport);
  return true;
}

void Gpu::Draw(const GpuMesh& mesh, SDL_GPUTexture* texture, const math::Mat4& model,
               const math::Mat4& view_proj, SDL_FColor tint) {
  if (pass_ == nullptr || mesh.index_count == 0) return;

  struct VertexUniforms {
    math::Mat4 mvp;
    math::Mat4 model;
  } vu{view_proj * model, model};
  SDL_PushGPUVertexUniformData(cmd_, 0, &vu, sizeof(vu));

  struct FragmentUniforms {
    float color[4];
    float params[4];
  } fu{{tint.r, tint.g, tint.b, tint.a}, {texture != nullptr ? 1.0f : 0.0f, 0, 0, 0}};
  SDL_PushGPUFragmentUniformData(cmd_, 0, &fu, sizeof(fu));

  SDL_GPUTextureSamplerBinding tex_binding{texture != nullptr ? texture : white_, sampler_};
  SDL_BindGPUFragmentSamplers(pass_, 0, &tex_binding, 1);

  SDL_GPUBufferBinding vb{mesh.vertex_buffer, 0};
  SDL_BindGPUVertexBuffers(pass_, 0, &vb, 1);
  SDL_GPUBufferBinding ib{mesh.index_buffer, 0};
  SDL_BindGPUIndexBuffer(pass_, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  SDL_DrawGPUIndexedPrimitives(pass_, mesh.index_count, 1, 0, 0, 0);
}

void Gpu::EndFrame() {
  if (pass_ != nullptr) SDL_EndGPURenderPass(pass_);
  if (cmd_ != nullptr) SDL_SubmitGPUCommandBuffer(cmd_);
  pass_ = nullptr;
  cmd_ = nullptr;
}
