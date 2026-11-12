#pragma once

#include <SDL3/SDL.h>

#include <utility>
#include <vector>

#include "scripts/cards/cards.h"
#include "scripts/gpu/gpu.h"
#include "scripts/math/math.h"
#include "scripts/layout/layout.h"
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
    float arena_scale = layout::Number("battle.arena.scale", 4.2f);
    float tile = layout::Number("battle.board.tile", 0.356f);
    float channel_half = layout::Number("battle.board.channelHalf", 0.178f);
    float top_y = layout::Number("battle.board.topY", 0.30f);
    // card.glb is 3.5 tall; fits the tile
    float card_scale = layout::Number("battle.board.cardScale", 0.36f);
    float highlight_inset = layout::Number("battle.board.highlightInset", 0.88f);
    float frame_thickness = layout::Number("battle.board.frameThickness", 0.08f);
    // above the tile top, avoids depth fighting
    float highlight_lift = layout::Number("battle.board.highlightLift", 0.012f);
    float place_seconds = layout::Number("battle.board.placeSeconds", 0.45f);
    math::Vec3 place_from = layout::Vec3("battle.board.placeFrom", {0.0f, 2.4f, 1.4f});
    float place_tilt = layout::Number("battle.board.placeTilt", -0.9f);
    float place_scale = layout::Number("battle.board.placeScale", 1.5f);
    float discard_seconds = layout::Number("battle.board.discardSeconds", 0.35f);
    float discard_rise = layout::Number("battle.board.discardRise", 1.6f);
    float position_speed = layout::Number("battle.board.positionSpeed", 9.0f);
    float hover_scale = layout::Number("battle.board.hoverScale", 1.45f);
    float hover_lift = layout::Number("battle.board.hoverLift", 0.35f);
    float hover_speed = layout::Number("battle.board.hoverSpeed", 12.0f);
    float lunge_seconds = layout::Number("battle.board.lungeSeconds", 0.5f);
    float lunge_reach = layout::Number("battle.board.lungeReach", 0.7f);
    float lunge_height = layout::Number("battle.board.lungeHeight", 0.6f);
    float flash_speed = layout::Number("battle.board.flashSpeed", 5.0f);
    float spent_scale = layout::Number("battle.board.spentScale", 0.82f);
    float spent_desaturate = layout::Number("battle.board.spentDesaturate", 0.5f);
    float spent_speed = layout::Number("battle.board.spentSpeed", 8.0f);
    float pulse_speed = layout::Number("battle.board.pulseSpeed", 3.0f);
  };

  struct Card {
    SDL_GPUTexture* front = nullptr;
    bool foil = false;
    const CardInfo* info = nullptr;
    int summoned_turn = 0;  // cannot change position on this turn
    bool attacked = false;
    bool defense = false;
    int attack_mod = 0;  // instant field cards; added to the effective value
    int defense_mod = 0;
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
  void SetHoverColor(const char* color) { hover_color_ = color; }
  void SetHoverCard(int slot) { hover_card_ = slot; }
  // Slots marked with a pulsing gold frame (tutorial emphasis).
  void SetFocus(std::vector<int> slots) { focus_ = std::move(slots); }

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
  const char* hover_color_ = "yellow";
  int hover_card_ = -1;
  std::vector<int> focus_;
  float time_ = 0.0f;
};
