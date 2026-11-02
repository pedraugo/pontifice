#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "scripts/gpu/Gpu.h"
#include "scripts/math/Math.h"

// Efeito de golpe de espada num ataque: uma espada procedural (lâmina, guarda, cabo e pomo,
// caixas coloridas sem textura, iluminadas como o resto da arena) aparece acima do alvo vinda
// do lado do atacante, desce num golpe até a carta e some; no impacto um rastro de corte
// (quad emissivo, branco, diagonal) risca o plano da carta e se apaga. Sem textura nem shader
// novo: usa Gpu::DrawMode::kMesh e kEmissive.
class Sword {
 public:
  struct Layout {
    float scale = 1.0f;         // tamanho da espada (lâmina de 1,6 no modelo)
    float seconds = 0.55f;      // duração total do efeito
    float swing_end = 0.35f;    // fração do tempo em que o golpe chega na carta
    float pivot_height = 1.15f; // altura do punho acima do alvo no fim do golpe
    float pivot_back = 0.45f;   // punho recuado para o lado do atacante
    float angle_start = -1.3f;  // rad: lâmina erguida para trás (lado do atacante)
    float angle_end = 2.75f;    // rad: lâmina apontando para baixo, na carta
    float slash_length = 1.5f;
    float slash_width = 0.14f;
    float slash_seconds = 0.3f;
  };

  bool Init(Gpu& gpu);  // cria as malhas (qualquer thread)
  void Destroy(Gpu& gpu);
  Layout& GetLayout() { return layout_; }

  // Dispara o golpe em `target` (centro da carta, mundo) vindo de `from` (centro do atacante).
  void Play(const math::Vec3& target, const math::Vec3& from);
  bool Active() const { return time_ < layout_.seconds; }

  void Update(float dt);
  // Desenha no passe da arena, depois das cartas (a espada é opaca; o rastro, transparente).
  void Render(Gpu& gpu, const math::Mat4& view_proj) const;

 private:
  struct Part {
    GpuMesh mesh;
    SDL_FColor color;
  };
  static void Box(std::vector<Vertex>& v, std::vector<Uint32>& i, float x0, float y0, float z0,
                  float x1, float y1, float z1);

  Layout layout_;
  std::vector<Part> parts_;
  GpuMesh slash_;
  math::Vec3 target_;
  float yaw_ = 0.0f;   // direção horizontal do atacante para o alvo
  float time_ = 1e9f;  // segundos desde Play (>= seconds: inativo)
};
