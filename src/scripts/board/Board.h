#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "scripts/cards/Cards.h"
#include "scripts/gpu/Gpu.h"
#include "scripts/math/Math.h"
#include "scripts/model/Model.h"

// Um lado do campo (docs/campo.md): 10 slots de carta em 5 colunas x 2 linhas, alinhados aos
// ladrilhos de uma plataforma da arena (models/Arena.glb), e as cartas colocadas neles.
// SetSide(+1) é a plataforma da frente (jogador, z > 0); SetSide(-1) a do fundo (oponente,
// z < 0), com as cartas viradas para o dono. Linha A (índices 0 a 4) fica junto da canaleta
// central; linha B (5 a 9) junto do dono.
// Em modo de invocação os slots vazios ficam levemente destacados e o slot sob o mouse ganha
// uma moldura quadrada amarela; a carta colocada desce animada de cima do slot até deitar
// sobre o ladrilho. Invocar num slot ocupado descarta a carta anterior (sobe e some). Uma
// carta troca de posição de batalha girando 90° no plano da mesa (ataque em pé para o
// jogador, defesa deitada de lado).
class Board {
 public:
  static constexpr int kCols = 5;
  static constexpr int kRows = 2;
  static constexpr int kSlots = kCols * kRows;

  // Medidas da arena em unidades do modelo (tools/texture_arena.py: ARENA_*), multiplicadas
  // por arena_scale (kArenaScale da cena de batalha).
  struct Layout {
    float arena_scale = 4.2f;
    float tile = 0.356f;          // ARENA_TILE: lado do ladrilho quadrado
    float channel_half = 0.178f;  // metade de ARENA_CHANNEL_W: a linha A começa aqui em z
    float top_y = 0.30f;          // ARENA_HIGH_Y: topo das plataformas
    float card_scale = 0.36f;     // Card.glb tem 3,5 de altura; cabe no ladrilho (1,5 no mundo)
    float highlight_inset = 0.88f;  // lado do destaque em frações do ladrilho
    float frame_thickness = 0.08f;  // espessura da moldura do slot sob o mouse (fração do lado)
    float highlight_lift = 0.012f;  // acima do topo, para não brigar com a profundidade
    float place_seconds = 0.45f;  // duração da descida da carta até o slot
    math::Vec3 place_from{0.0f, 2.4f, 1.4f};  // de onde ela parte, relativo ao slot
    float place_tilt = -0.9f;     // inclinação inicial em X (rad); deitada é -pi/2
    float place_scale = 1.5f;     // escala inicial relativa à final
    float discard_seconds = 0.35f;  // carta descartada: sobe e encolhe até sumir
    float discard_rise = 1.6f;
    float position_speed = 9.0f;  // velocidade do giro ataque <-> defesa
    float hover_scale = 1.45f;    // carta sob o mouse cresce (relativo à escala normal)
    float hover_lift = 0.35f;     // e sobe um pouco
    float hover_speed = 12.0f;    // velocidade da animação do hover
    float lunge_seconds = 0.5f;   // investida de ataque: vai até perto do alvo e volta
    float lunge_reach = 0.7f;     // fração da distância até o alvo
    float lunge_height = 0.6f;    // arco por cima
    float flash_speed = 5.0f;     // decaimento do flash de dano
    float spent_scale = 0.82f;    // carta que já atacou no turno: menor...
    float spent_alpha = 0.3f;     // ...e translúcida (30%)
    float spent_speed = 8.0f;     // velocidade da transição
    float pulse_speed = 3.0f;     // pulsação do destaque (rad/s)
  };

  struct Card {
    SDL_GPUTexture* front = nullptr;
    bool foil = false;
    const CardInfo* info = nullptr;  // dados da carta (ataque, defesa, categoria)
    int summoned_turn = 0;           // turno em que entrou (não troca de posição nele)
    bool attacked = false;           // já atacou neste turno
    bool defense = false;    // modo de defesa: deitada de lado
    float defense_t = 0.0f;  // giro animado: 0 = ataque, 1 = defesa
    float place = 1.0f;      // animação de colocação, 0 (no ar) a 1 (sobre o ladrilho)
    float discard = 0.0f;    // animação de descarte, 0 (no lugar) a 1 (sumiu)
    float hover = 0.0f;      // sob o mouse: 0 normal, 1 ampliada (animado)
    float flash = 0.0f;      // atingida: 1 logo após o golpe, decai (tinge de vermelho)
    float spent = 0.0f;      // já atacou: 0 normal, 1 menor e translúcida (animado)
    float lunge = 1.0f;      // investida: 0 começa, 1 terminou (parada em 1)
    math::Vec3 lunge_target; // para onde investe (mundo)
    math::Mat4 world;
  };

  void SetModel(const Model* model) { model_ = model; }
  void SetBack(SDL_GPUTexture* back) { back_ = back; }
  void SetNormalMaps(SDL_GPUTexture* front, SDL_GPUTexture* back) {
    front_normal_ = front;
    back_normal_ = back;
  }
  Layout& GetLayout() { return layout_; }
  // +1: plataforma da frente (jogador); -1: do fundo (oponente), cartas viradas para ele.
  void SetSide(float side) { side_ = side < 0 ? -1.0f : 1.0f; }
  float Side() const { return side_; }

  // Cria o quad de destaque e a moldura do slot sob o mouse (qualquer thread, como
  // Gpu::CreateMesh).
  bool Init(Gpu& gpu);
  void Destroy(Gpu& gpu);

  // Centro do ladrilho do slot no mundo (y = topo da arena).
  math::Vec3 SlotPosition(int slot) const;
  bool Empty(int slot) const { return slot >= 0 && slot < kSlots && !cards_[slot].occupied; }
  int Count() const;
  const Card* At(int slot) const {
    return Empty(slot) || slot < 0 || slot >= kSlots ? nullptr : &cards_[slot].card;
  }
  Card* At(int slot) {
    return Empty(slot) || slot < 0 || slot >= kSlots ? nullptr : &cards_[slot].card;
  }
  std::vector<int> Occupied() const;  // slots com carta
  std::vector<int> Free() const;      // slots vazios
  // Centro da carta no mundo (sobre o ladrilho), alvo de investidas.
  math::Vec3 CardPosition(int slot) const;
  // Começo de turno do dono: as cartas podem atacar de novo.
  void ResetAttacks();

  // Slot sob o mouse (ou -1): os quatro cantos de cada ladrilho projetados na tela.
  int PickSlot(math::Vec2 mouse, const math::Mat4& view_proj, int width, int height) const;
  // Coloca a carta no slot vazio; falso se o slot é inválido ou está ocupado.
  bool Place(int slot, SDL_GPUTexture* front, bool foil, bool defense = false,
             const CardInfo* info = nullptr, int turn = 0);
  // Investida de ataque: a carta vai até perto de `target` (mundo) e volta ao lugar.
  void Lunge(int slot, const math::Vec3& target);
  // Carta atingida: flash vermelho que decai.
  void Flash(int slot);
  bool Animating() const;  // alguma carta em colocação, investida ou descarte
  // Tira a carta do slot (vai para o cemitério) com animação de saída; falso se vazio.
  bool Discard(int slot);
  // Alterna ataque <-> defesa da carta do slot (giro animado); falso se vazio.
  bool TogglePosition(int slot);

  // Destaque dos slots vazios (modo de invocação) e moldura do slot sob o mouse.
  void SetHighlight(bool on) { highlight_ = on; }
  void SetHoverSlot(int slot) { hover_slot_ = slot; }
  // Carta sob o mouse: cresce e sobe (animado); -1 nenhuma.
  void SetHoverCard(int slot) { hover_card_ = slot; }

  void Update(float dt);
  void Render(Gpu& gpu, const math::Mat4& view_proj) const;
  void RenderDepth(Gpu& gpu) const;  // passe de sombra (Gpu::BeginShadowPass)

 private:
  struct Slot {
    bool occupied = false;
    Card card;
  };
  math::Mat4 CardMatrix(int slot, const Card& card) const;
  void DrawCard(Gpu& gpu, const math::Mat4& view_proj, const Card& card) const;
  Model::Skin SkinFor(const Card& card) const;

  const Model* model_ = nullptr;
  float side_ = 1.0f;
  SDL_GPUTexture* back_ = nullptr;
  SDL_GPUTexture* front_normal_ = nullptr;
  SDL_GPUTexture* back_normal_ = nullptr;
  Layout layout_;
  Slot cards_[kSlots];
  struct Leaving {
    int slot;
    Card card;
  };
  std::vector<Leaving> leaving_;  // cartas descartadas ainda em animação de saída
  GpuMesh highlight_quad_;
  GpuMesh frame_quad_;  // anel quadrado (4 tiras) de lado 1, em y = 0
  bool highlight_ = false;
  int hover_slot_ = -1;
  int hover_card_ = -1;
  float time_ = 0.0f;
};
