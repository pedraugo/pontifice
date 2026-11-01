#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "scripts/math/Math.h"

class Gpu;
class Model;

// Mão de cartas em 3D, disposta como um dock na parte de baixo da tela.
// Em repouso a mão fica recolhida (parcialmente fora da tela) para não esconder a arena;
// quando o mouse chega até ela, a mão inteira sobe. Passar o mouse sobre uma carta a
// levanta, aproxima da câmera e afasta as vizinhas.
class Hand {
 public:
  struct Card {
    SDL_GPUTexture* front = nullptr;
    float hover = 0.0f;  // 0 = em repouso, 1 = totalmente destacada (animado)
    float shift = 0.0f;  // deslocamento lateral causado pela carta destacada (animado)
    math::Mat4 world;    // matriz do último quadro, usada no teste de hover
  };

  struct Layout {
    float base_y = -1.85f;        // altura do centro das cartas em repouso
    float base_z = 0.0f;          // profundidade da mão
    float depth_step = 0.03f;     // cada carta fica um pouco à frente da anterior (sem z-fight)
    float spacing = 1.05f;        // distância entre centros
    float max_width = 9.5f;       // largura máxima; acima disso o spacing diminui
    float arc = 0.045f;           // curvatura: y cai com o quadrado da distância ao centro
    float tilt_per_unit = 0.06f;  // rotação em Z por unidade de distância ao centro (rad)
    float scale = 0.5f;           // escala da carta em repouso
    float hover_scale = 0.62f;    // escala da carta destacada
    float hover_lift = 0.55f;     // quanto sobe
    float hover_forward = 0.9f;   // quanto se aproxima da câmera
    float neighbor_push = 0.38f;  // quanto as vizinhas se afastam
    float anim_speed = 14.0f;     // velocidade da animação (maior = mais rápida)
    float hidden_drop = 1.0f;     // quanto a mão inteira desce quando o mouse está longe
    float raise_speed = 10.0f;    // velocidade com que a mão sobe/desce
  };

  void SetModel(const Model* model) { model_ = model; }
  void SetBack(SDL_GPUTexture* back) { back_ = back; }
  Layout& GetLayout() { return layout_; }

  void Add(SDL_GPUTexture* front);
  void Clear() { cards_.clear(); }
  size_t Size() const { return cards_.size(); }

  // mouse em coordenadas da textura alvo (0..width, 0..height). Negativo = fora da tela.
  void Update(float dt, math::Vec2 mouse, const math::Mat4& view_proj, int width, int height);
  void Render(Gpu& gpu, const math::Mat4& view_proj) const;

  int Hovered() const { return hovered_; }
  bool Raised() const { return raised_; }

 private:
  int PickCard(math::Vec2 mouse, const math::Mat4& view_proj, int width, int height) const;
  // true se o mouse está sobre uma carta ou abaixo do topo da mão levantada.
  bool MouseOverHand(math::Vec2 mouse, const math::Mat4& view_proj, int height) const;
  math::Mat4 CardMatrix(const Card& card, int index) const;

  const Model* model_ = nullptr;
  SDL_GPUTexture* back_ = nullptr;
  Layout layout_;
  std::vector<Card> cards_;
  int hovered_ = -1;
  bool raised_ = false;  // alvo: mão levantada (mouse sobre ela) ou recolhida
  float raise_ = 0.0f;   // 0 = recolhida, 1 = levantada (animado)
};
