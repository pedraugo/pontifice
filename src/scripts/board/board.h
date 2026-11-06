#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "scripts/cards/cards.h"
#include "scripts/gpu/gpu.h"
#include "scripts/math/math.h"
#include "scripts/model/model.h"

class CardAura;

class Board {
 public:
  // Row A (slots 0-4) sits next to the center channel, row B (5-9) next to the owner.
  static constexpr int kCols = 5;
  static constexpr int kRows = 2;
  static constexpr int kSlots = kCols * kRows;

  // Arena measures in model units (tools/textureArena.py ARENA_*), scaled by arena_scale.
  struct Layout {
    float arena_scale = 4.2f;
    float tile = 0.356f;
    float channel_half = 0.178f;
    float top_y = 0.30f;
    float card_scale = 0.36f;  // card.glb is 3.5 tall; fits the tile
    float highlight_inset = 0.88f;
    float frame_thickness = 0.08f;
    float highlight_lift = 0.012f;  // above the tile top, avoids depth fighting
    float place_seconds = 0.45f;
    math::Vec3 place_from{0.0f, 2.4f, 1.4f};
    float place_tilt = -0.9f;
    float place_scale = 1.5f;
    float discard_seconds = 0.35f;
    float discard_rise = 1.6f;
    float position_speed = 9.0f;
    float hover_scale = 1.45f;
    float hover_lift = 0.35f;
    float hover_speed = 12.0f;
    float lunge_seconds = 0.5f;
    float lunge_reach = 0.7f;
    float lunge_height = 0.6f;
    float flash_speed = 5.0f;
    float spent_scale = 0.82f;
    float spent_alpha = 0.3f;
    float spent_speed = 8.0f;
    float pulse_speed = 3.0f;
  };

  struct Card {
    SDL_GPUTexture* front = nullptr;
    bool foil = false;
    const CardInfo* info = nullptr;
    int summoned_turn = 0;  // cannot change position on this turn
    bool attacked = false;
    bool defense = false;
    float defense_t = 0.0f;  // animated turn: 0 = attack, 1 = defense
    float place = 1.0f;  // 0 = in the air, 1 = resting on the tile
    float discard = 0.0f;
    float hover = 0.0f;
    float flash = 0.0f;
    float spent = 0.0f;
    float lunge = 1.0f;  // 0 = starts, 1 = finished (idle)
    math::Vec3 lunge_target;
    math::Mat4 world;
  };

  void SetModel(const Model* model) { model_ = model; }
  void SetAura(const CardAura* aura) { aura_ = aura; }
  void SetBack(SDL_GPUTexture* back) { back_ = back; }
  void SetNormalMaps(SDL_GPUTexture* front, SDL_GPUTexture* back) {
    front_normal_ = front;
    back_normal_ = back;
  }
  Layout& GetLayout() { return layout_; }
  // +1 = front platform (player, z > 0); -1 = back platform (opponent), cards facing it.
  void SetSide(float side) { side_ = side < 0 ? -1.0f : 1.0f; }
  float Side() const { return side_; }

  bool Init(Gpu& gpu);
  void Destroy(Gpu& gpu);

  math::Vec3 SlotPosition(int slot) const;
  bool Empty(int slot) const { return slot >= 0 && slot < kSlots && !cards_[slot].occupied; }
  int Count() const;
  const Card* At(int slot) const {
    return Empty(slot) || slot < 0 || slot >= kSlots ? nullptr : &cards_[slot].card;
  }
  Card* At(int slot) {
    return Empty(slot) || slot < 0 || slot >= kSlots ? nullptr : &cards_[slot].card;
  }
  std::vector<int> Occupied() const;
  std::vector<int> Free() const;
  math::Vec3 CardPosition(int slot) const;
  void ResetAttacks();

  int PickSlot(math::Vec2 mouse, const math::Mat4& view_proj, int width, int height) const;
  bool Place(int slot, SDL_GPUTexture* front, bool foil, bool defense = false,
             const CardInfo* info = nullptr, int turn = 0);
  void Lunge(int slot, const math::Vec3& target);
  void Flash(int slot);
  bool Animating() const;
  bool Discard(int slot);
  bool TogglePosition(int slot);

  void SetHighlight(bool on) { highlight_ = on; }
  void SetHoverSlot(int slot) { hover_slot_ = slot; }
  // `role` must outlive the Board (string literal).
  void SetHoverRole(const char* role) { hover_role_ = role; }
  void SetHoverCard(int slot) { hover_card_ = slot; }

  void Update(float dt);
  void Render(Gpu& gpu, const math::Mat4& view_proj) const;
  void RenderDepth(Gpu& gpu) const;

 private:
  struct Slot {
    bool occupied = false;
    Card card;
  };
  math::Mat4 CardMatrix(int slot, const Card& card) const;
  void DrawCard(Gpu& gpu, const math::Mat4& view_proj, const Card& card) const;
  Model::Skin SkinFor(const Card& card) const;

  const Model* model_ = nullptr;
  const CardAura* aura_ = nullptr;
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
  std::vector<Leaving> leaving_;  // discarded cards still animating out
  GpuMesh highlight_quad_;
  GpuMesh frame_quad_;
  bool highlight_ = false;
  int hover_slot_ = -1;
  const char* hover_role_ = "board.slotHover";
  int hover_card_ = -1;
  float time_ = 0.0f;
};
