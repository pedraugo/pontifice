#include "scripts/aura/CardAura.h"

#include <vector>

#include "scripts/model/Model.h"

bool CardAura::Init(Gpu& gpu, const Model& card, float margin) {
  const math::Vec3 lo = card.BoundsMin();
  const math::Vec3 hi = card.BoundsMax();
  const float w = hi.x - lo.x;
  const float h = hi.y - lo.y;
  if (w <= 0.0f || h <= 0.0f) return false;
  const float m = margin * h;  // aura width beyond the card edge
  const float r = 0.06f * w;  // inner rect inset: covers the rounded corners
  const float z = (lo.z + hi.z) * 0.5f;
  const float xs[4] = {lo.x - m, lo.x + r, hi.x - r, hi.x + m};
  const float ys[4] = {lo.y - m, lo.y + r, hi.y - r, hi.y + m};
  // 4x4 grid; UV = distance past the inner rect per axis, 0 inside, 1 at the outer edge
  std::vector<Vertex> vertices;
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 4; ++i) {
      const float u = (i == 0 || i == 3) ? 1.0f : 0.0f;
      const float v = (j == 0 || j == 3) ? 1.0f : 0.0f;
      vertices.push_back({xs[i], ys[j], z, u, v});
    }
  }
  std::vector<Uint32> indices;
  for (int j = 0; j < 3; ++j) {
    for (int i = 0; i < 3; ++i) {
      if (i == 1 && j == 1) continue;  // middle cell: covered by the card
      const Uint32 a = static_cast<Uint32>(j * 4 + i);
      indices.insert(indices.end(), {a, a + 1, a + 5, a, a + 5, a + 4});
    }
  }
  mesh_ = gpu.CreateMesh(vertices, indices);
  return Ready();
}

void CardAura::Destroy(Gpu& gpu) { gpu.DestroyMesh(mesh_); }

void CardAura::Draw(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj,
                    float alpha) const {
  if (!Ready() || alpha <= 0.0f) return;
  gpu.Draw(mesh_, nullptr, world, view_proj, SDL_FColor{1.0f, 1.0f, 1.0f, alpha},
           Gpu::DrawMode::kAura);
}
