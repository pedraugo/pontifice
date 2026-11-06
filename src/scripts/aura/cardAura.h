#pragma once

#include "scripts/gpu/gpu.h"
#include "scripts/math/math.h"

class Model;

// Glow frame around a foil card, drawn just before it in DrawMode::kAura (see card.metal)
class CardAura {
 public:
  // margin: aura width as a fraction of the card height
  bool Init(Gpu& gpu, const Model& card, float margin = 0.12f);
  void Destroy(Gpu& gpu);
  bool Ready() const { return mesh_.index_count > 0; }

  void Draw(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj,
            float alpha = 1.0f) const;

 private:
  GpuMesh mesh_;
};
