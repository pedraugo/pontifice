#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

#include "scripts/math/Math.h"

#include "scripts/model/Model.h"

class Gpu;

// Mão de cartas em 3D, disposta como um dock na parte de baixo da tela.
// Em repouso a mão fica recolhida (parcialmente fora da tela) para não esconder a arena;
// quando o mouse chega até ela, a mão inteira sobe. Passar o mouse sobre uma carta a
// levanta, aproxima da câmera e afasta as vizinhas.
// Gestos (clicar numa carta e arrastar): para CIMA seleciona/desseleciona a carta (várias
// podem ficar selecionadas, erguidas); para BAIXO inspeciona (uma por vez): a carta vai
// ampliada para o centro da tela; arrastar na horizontal gira a carta em Y (360°, mostra o
// verso) e na vertical a inclina um pouco em X (volta sozinha); um clique na carta a vira
// (meia-volta em Y, animada); clique fora dela, botão direito ou Esc encerra a inspeção.
class Hand {
 public:
  struct Card {
    SDL_GPUTexture* front = nullptr;
    bool foil = false;   // carta rara (hierarquia 1): frente com brilho iridescente
    float hover = 0.0f;  // 0 = em repouso, 1 = totalmente destacada (animado)
    float shift = 0.0f;  // deslocamento lateral causado pela carta destacada (animado)
    float tilt_x = 0.0f; // inclinação em X (rad) conforme o mouse sobe/desce na carta (animado)
    float tilt_y = 0.0f; // inclinação em Y (rad) conforme o mouse vai à esquerda/direita
    float enter = 0.0f;  // entrada: 0 = fora da tela à esquerda, 1 = no lugar (animado)
    float enter_delay = 0.0f;  // segundos até começar a entrar (cartas entram uma a uma)
    bool selected = false;
    float select = 0.0f;  // 0 = normal, 1 = selecionada (erguida; animado)
    math::Mat4 world;    // matriz do último quadro, com inclinação (render)
    math::Mat4 pick;     // idem sem inclinação: o teste de hover não oscila nas quinas
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
    float tilt_max = 0.3f;        // inclinação máxima (rad) com o mouse na quina da carta
    float tilt_speed = 12.0f;     // velocidade da inclinação
    float hidden_drop = 1.0f;     // quanto a mão inteira desce quando o mouse está longe
    float raise_speed = 10.0f;    // velocidade com que a mão sobe/desce
    float select_lift = 0.5f;     // quanto a carta selecionada sobe
    float select_forward = 0.15f; // quanto se aproxima da câmera
    float drag_threshold = 36.0f; // px lógicos (1920x1080) de arrasto vertical para virar gesto
    float inspect_z = 4.2f;       // profundidade da carta inspecionada
    float inspect_y = -0.05f;
    float camera_z = 10.0f;       // câmera da mão (BattleScene): em z = 10 olhando a origem
    float fov_y = 0.6108652f;     // 35° em rad, a mesma da projeção da mão
    float zoom_min = 0.75f;       // altura da carta inspecionada como fração da altura da tela
    float zoom_max = 1.0f;
    float zoom_start = 0.8f;
    float zoom_step = 0.05f;      // por "clique" da roda
    float inspect_speed = 9.0f;   // velocidade de ir/voltar da inspeção
    float rotate_speed = 0.010f;  // rad por px de arrasto ao girar a carta inspecionada (Y)
    float tilt_limit = 0.35f;     // inclinação máxima (rad, em X) ao arrastar na vertical
    float flip_speed = 9.0f;      // velocidade da meia-volta ao clicar na carta inspecionada
    float deal_from_x = -8.5f;    // x de onde a carta entra (fora da tela, à esquerda)
    float deal_speed = 13.0f;     // velocidade da entrada (unidades/s): quem vai mais longe
                                  // demora mais e nunca ultrapassa a carta anterior
    float deal_stagger = 0.12f;   // intervalo entre a entrada de uma carta e a da próxima
  };

  void SetModel(const Model* model) { model_ = model; }
  void SetBack(SDL_GPUTexture* back) { back_ = back; }
  // Normal maps da frente e do verso (relevo da moldura), aplicados a todas as cartas.
  void SetNormalMaps(SDL_GPUTexture* front, SDL_GPUTexture* back) {
    front_normal_ = front;
    back_normal_ = back;
  }
  Layout& GetLayout() { return layout_; }

  // A carta entra deslizando da esquerda da tela até o seu lugar; cartas adicionadas em
  // sequência entram uma depois da outra (deal_stagger).
  void Add(SDL_GPUTexture* front, bool foil = false);
  // Atrasa a entrada das próximas cartas em pelo menos `seconds` (ex.: esperar o fade in).
  void DelayDeal(float seconds) { pending_delay_ = std::max(pending_delay_, seconds); }
  void Clear() { cards_.clear(); }
  size_t Size() const { return cards_.size(); }

  // mouse em coordenadas da textura alvo (0..width, 0..height). Negativo = fora da tela.
  void Update(float dt, math::Vec2 mouse, const math::Mat4& view_proj, int width, int height);
  void Render(Gpu& gpu, const math::Mat4& view_proj) const;

  // Botões do mouse (mesmas coordenadas de Update). Esquerdo inicia um arrasto sobre a
  // carta sob o cursor; direito encerra a inspeção.
  void MouseDown(math::Vec2 mouse, bool left);
  void MouseUp(math::Vec2 mouse);

  int Hovered() const { return hovered_; }
  bool Raised() const { return raised_; }
  bool Inspecting() const { return inspecting_ >= 0; }
  void StopInspect() { inspecting_ = -1; }  // a carta volta animada para a mão
  void Flip();  // vira a carta inspecionada (meia-volta em Y, animada)
  // Roda do mouse durante a inspeção: aproxima/afasta entre zoom_min e zoom_max da altura.
  void Wheel(float dy);
  bool IsSelected(size_t i) const { return i < cards_.size() && cards_[i].selected; }
  std::vector<size_t> Selected() const;

 private:
  int PickCard(math::Vec2 mouse, const math::Mat4& view_proj, int width, int height) const;
  // true se o mouse está sobre uma carta ou abaixo do topo da mão levantada.
  bool MouseOverHand(math::Vec2 mouse, const math::Mat4& view_proj, int height) const;
  float CenterOffset(int index) const;  // x de repouso da carta em relação ao centro da mão
  Model::Skin SkinFor(const Card& card) const;
  math::Mat4 CardMatrix(const Card& card, int index, bool with_tilt) const;
  // Posição do mouse relativa ao centro da carta, em [-1, 1] nos dois eixos (x direita,
  // y cima). Usa a matriz sem inclinação projetada na tela.
  math::Vec2 MouseOnCard(const Card& card, math::Vec2 mouse, const math::Mat4& view_proj,
                         int width, int height) const;

  const Model* model_ = nullptr;
  SDL_GPUTexture* back_ = nullptr;
  SDL_GPUTexture* front_normal_ = nullptr;
  SDL_GPUTexture* back_normal_ = nullptr;
  Layout layout_;
  std::vector<Card> cards_;
  int hovered_ = -1;
  bool raised_ = false;  // alvo: mão levantada (mouse sobre ela) ou recolhida
  float raise_ = 0.0f;   // 0 = recolhida, 1 = levantada (animado)
  float pending_delay_ = 0.0f;  // atraso que a próxima carta adicionada vai receber

  // Arrasto: carta pressionada e onde; -1 quando nada está pressionado.
  int press_card_ = -1;
  math::Vec2 press_pos_{0, 0};
  bool press_consumed_ = false;  // o gesto já foi reconhecido neste arrasto
  math::Vec2 last_mouse_{-1, -1};
  // Inspeção: índice da carta (ou -1), progresso animado 0..1 e rotação pelo mouse.
  int inspecting_ = -1;
  int inspect_card_ = -1;  // carta ainda em animação de ida/volta (pode já não estar inspecionada)
  float inspect_t_ = 0.0f;
  float inspect_yaw_ = 0.0f;    // Y: giro pelo arrasto horizontal (360°) e pelo Flip
  float inspect_pitch_ = 0.0f;  // X: inclinação leve pelo arrasto vertical, volta a zero
  float yaw_target_ = 0.0f;     // alvo do giro animado (Flip); segue o arrasto ao soltar
  float zoom_ = 0.8f;        // fração da altura da tela ocupada pela carta inspecionada
  float zoom_shown_ = 0.8f;  // idem, animado
  bool rotating_ = false;
  // Último quadro, para PickCard em MouseDown.
  math::Mat4 last_view_proj_;
  int last_width_ = 1;
  int last_height_ = 1;
};
