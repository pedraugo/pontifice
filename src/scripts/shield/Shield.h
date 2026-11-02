#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "scripts/gpu/Gpu.h"
#include "scripts/math/Math.h"

// Escudo de defesa (docs/batalha.md, "Defesa"): quando uma carta em modo de defesa é
// atacada, um escudo heráldico procedural surge em pé sobre ela, virado para o atacante,
// segura o golpe da espada (recua, brilha e solta uma onda de choque dourada) e some. Se a
// carta é destruída, o escudo fica avermelhado, a onda sai vermelha e ele cai girando.
// Malhas sem textura (polígono extrudado e caixas), Gpu::DrawMode::kMesh; a onda de choque é
// um anel em kEmissive.
class Shield {
 public:
  struct Layout {
    float scale = 0.85f;        // tamanho (escudo de ~1,1 de altura no modelo)
    float height = 0.8f;        // centro do escudo acima da carta
    float lean = 0.3f;          // inclinação para trás (rad), para a câmera ver a frente
    float appear = 0.15f;       // surgimento (s)
    float hold = 0.9f;          // duração total quando a defesa segura (s)
    float broken = 0.8f;        // duração total quando a carta é destruída (s)
    float wave_seconds = 0.4f;  // onda de choque
  };

  bool Init(Gpu& gpu);  // cria as malhas (qualquer thread)
  void Destroy(Gpu& gpu);
  Layout& GetLayout() { return layout_; }

  // Dispara sobre `target` (centro da carta em defesa), virado para `from` (atacante). O
  // golpe chega em `hit_delay` s (o da espada); `broken`: a carta não aguenta e é destruída.
  void Play(const math::Vec3& target, const math::Vec3& from, float hit_delay, bool broken);
  bool Active() const { return time_ < Duration(); }

  void Update(float dt);
  // No passe da arena, depois das cartas (escudo opaco; a onda, transparente).
  void Render(Gpu& gpu, const math::Mat4& view_proj) const;

 private:
  struct Part {
    GpuMesh mesh;
    SDL_FColor color;
  };
  float Duration() const { return broken_ ? layout_.broken : layout_.hold; }

  Layout layout_;
  std::vector<Part> parts_;
  GpuMesh ring_;
  math::Vec3 target_;
  float yaw_ = 0.0f;
  float hit_ = 0.19f;
  bool broken_ = false;
  float time_ = 1e9f;  // segundos desde Play (>= Duration: inativo)
};
