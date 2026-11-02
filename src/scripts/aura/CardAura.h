#pragma once

#include "scripts/gpu/Gpu.h"
#include "scripts/math/Math.h"

class Model;

// Aura das cartas foil (hierarquia 1): moldura de luz em volta da carta, desenhada logo antes
// dela em Gpu::DrawMode::kAura (energia que flui e pulsa, branco-dourada junto à borda e em
// arco-íris para fora; ver card.metal, Aura). A malha é uma grade 4x4 no espaço local da
// carta, no plano do meio da espessura: as 8 células em volta de um retângulo um pouco menor
// que a carta (cobre os cantos arredondados), com UV = distância além desse retângulo,
// normalizada pela largura da aura, por eixo (0 dentro, 1 na ponta). Nos cantos a
// interpolação bilinear vira distância radial. A célula do meio fica de fora: a carta a
// cobre, e uma carta translúcida (já atacou) não mostra brilho por baixo.
class CardAura {
 public:
  // margin: largura da aura em fração da altura da carta.
  bool Init(Gpu& gpu, const Model& card, float margin = 0.12f);
  void Destroy(Gpu& gpu);
  bool Ready() const { return mesh_.index_count > 0; }

  // `world` é a matriz da carta; alpha esmaece a aura (carta usada, saindo do campo).
  void Draw(Gpu& gpu, const math::Mat4& world, const math::Mat4& view_proj,
            float alpha = 1.0f) const;

 private:
  GpuMesh mesh_;
};
