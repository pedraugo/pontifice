#include "scripts/model/Model.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <utility>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "scripts/image/Image.h"

namespace {

std::string Lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

Model::Role RoleFromName(const std::string& name) {
  const std::string n = Lower(name);
  if (n.find("front") != std::string::npos || n.find("frente") != std::string::npos) {
    return Model::Role::kFront;
  }
  if (n.find("back") != std::string::npos || n.find("verso") != std::string::npos) {
    return Model::Role::kBack;
  }
  return Model::Role::kEdge;
}

Image DecodeTexture(const cgltf_texture* texture) {
  if (texture == nullptr || texture->image == nullptr) return Image{};
  const cgltf_image* image = texture->image;
  if (image->buffer_view == nullptr) return Image{};
  const cgltf_buffer_view* view = image->buffer_view;
  const auto* bytes = static_cast<const uint8_t*>(view->buffer->data) + view->offset;
  return Image::LoadMemory(bytes, view->size);
}

Image DecodeEmbeddedTexture(const cgltf_material* material) {
  if (material == nullptr || !material->has_pbr_metallic_roughness) return Image{};
  return DecodeTexture(material->pbr_metallic_roughness.base_color_texture.texture);
}

Image DecodeNormalTexture(const cgltf_material* material) {
  if (material == nullptr) return Image{};
  return DecodeTexture(material->normal_texture.texture);
}

}

Model::~Model() {
  if (gpu_ == nullptr) return;
  for (Part& part : parts_) {
    gpu_->DestroyMesh(part.mesh);
    gpu_->DestroyTexture(part.texture);
    gpu_->DestroyTexture(part.normal);
  }
}

Model::Data Model::Parse(const std::string& path) {
  Data data;
  data.path = path;

  cgltf_options options{};
  cgltf_data* gltf = nullptr;
  if (cgltf_parse_file(&options, path.c_str(), &gltf) != cgltf_result_success) {
    SDL_Log("cgltf_parse_file falhou: %s", path.c_str());
    return data;
  }
  if (cgltf_load_buffers(&options, gltf, path.c_str()) != cgltf_result_success) {
    SDL_Log("cgltf_load_buffers falhou: %s", path.c_str());
    cgltf_free(gltf);
    return data;
  }

  data.min = {FLT_MAX, FLT_MAX, FLT_MAX};
  data.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};

  for (cgltf_size n = 0; n < gltf->nodes_count; ++n) {
    const cgltf_node& node = gltf->nodes[n];
    if (node.mesh == nullptr) continue;

    float world[16];
    cgltf_node_transform_world(&node, world);
    // cgltf and math::Mat4 are both column-major
    math::Mat4 xf;
    for (int c = 0; c < 4; ++c) {
      for (int l = 0; l < 4; ++l) xf.m[c][l] = world[c * 4 + l];
    }

    for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p) {
      const cgltf_primitive& prim = node.mesh->primitives[p];
      if (prim.type != cgltf_primitive_type_triangles || prim.indices == nullptr) continue;

      const cgltf_accessor* positions = nullptr;
      const cgltf_accessor* uvs = nullptr;
      for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
        const cgltf_attribute& attr = prim.attributes[a];
        if (attr.type == cgltf_attribute_type_position) positions = attr.data;
        if (attr.type == cgltf_attribute_type_texcoord && attr.index == 0) uvs = attr.data;
      }
      if (positions == nullptr) continue;

      Data::PartData part;
      part.vertices.resize(positions->count);
      for (cgltf_size i = 0; i < positions->count; ++i) {
        float pos[3] = {0, 0, 0};
        cgltf_accessor_read_float(positions, i, pos, 3);
        const math::Vec3 wp = xf.TransformPoint({pos[0], pos[1], pos[2]});
        float uv[2] = {0, 0};
        if (uvs != nullptr) cgltf_accessor_read_float(uvs, i, uv, 2);
        part.vertices[i] = Vertex{wp.x, wp.y, wp.z, uv[0], uv[1]};
        data.min = {std::min(data.min.x, wp.x), std::min(data.min.y, wp.y),
                    std::min(data.min.z, wp.z)};
        data.max = {std::max(data.max.x, wp.x), std::max(data.max.y, wp.y),
                    std::max(data.max.z, wp.z)};
      }

      part.indices.resize(prim.indices->count);
      for (cgltf_size i = 0; i < prim.indices->count; ++i) {
        part.indices[i] = static_cast<Uint32>(cgltf_accessor_read_index(prim.indices, i));
      }

      part.name = node.name != nullptr ? node.name : "";
      part.texture = DecodeEmbeddedTexture(prim.material);
      part.normal = DecodeNormalTexture(prim.material);
      if (prim.material != nullptr && prim.material->has_pbr_metallic_roughness) {
        const float* f = prim.material->pbr_metallic_roughness.base_color_factor;
        part.color = SDL_FColor{f[0], f[1], f[2], f[3]};
      }
      data.parts.push_back(std::move(part));
    }
  }

  cgltf_free(gltf);
  data.ok = !data.parts.empty();
  return data;
}

bool Model::Upload(Gpu& gpu, Data&& data) {
  gpu_ = &gpu;
  if (!data.ok) return false;
  min_ = data.min;
  max_ = data.max;
  for (Data::PartData& src : data.parts) {
    Part part;
    part.name = std::move(src.name);
    part.role = RoleFromName(part.name);
    part.mesh = gpu.CreateMesh(src.vertices, src.indices);
    part.texture = src.texture.Valid() ? gpu.CreateTexture(src.texture) : nullptr;
    part.normal = src.normal.Valid() ? gpu.CreateTexture(src.normal) : nullptr;
    part.color = src.color;
    parts_.push_back(part);
  }
  SDL_Log("Modelo %s: %zu partes, bounds x[%.2f %.2f] y[%.2f %.2f] z[%.3f %.3f]",
          data.path.c_str(), parts_.size(), min_.x, max_.x, min_.y, max_.y, min_.z, max_.z);
  return !parts_.empty();
}

void Model::Draw(Gpu& gpu, const math::Mat4& model, const math::Mat4& view_proj, SDL_FColor tint,
                 const Skin& skin) const {
  for (const Part& part : parts_) {
    SDL_GPUTexture* texture = part.texture;
    SDL_GPUTexture* normal_map = part.normal;
    if (part.role == Role::kFront) {
      if (skin.front != nullptr) texture = skin.front;
      if (skin.front_normal != nullptr) normal_map = skin.front_normal;
    }
    if (part.role == Role::kBack) {
      if (skin.back != nullptr) texture = skin.back;
      if (skin.back_normal != nullptr) normal_map = skin.back_normal;
    }

    SDL_FColor color = part.color;
    if (texture != nullptr) color = SDL_FColor{1, 1, 1, 1};
    color.r *= tint.r;
    color.g *= tint.g;
    color.b *= tint.b;
    color.a *= tint.a;
    gpu.Draw(part.mesh, texture, model, view_proj, color,
             part.role == Role::kFront ? skin.front_mode : Gpu::DrawMode::kMesh, normal_map);
  }
}
