#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <vector>

#include "scripts/cards/Cards.h"
#include "scripts/math/Math.h"
#include "scripts/model/Model.h"

class CardAura;

class Gpu;

// Drag a card up to select it (summon candidate), drag down to inspect it.
class Hand {
 public:
  struct Card {
    SDL_GPUTexture* front = nullptr;
    bool foil = false;
    const CardInfo* info = nullptr;
    float hover = 0.0f;
    float shift = 0.0f;
    float tilt_x = 0.0f;
    float tilt_y = 0.0f;
    float enter = 0.0f;  // 0 = off screen left, 1 = in place
    float enter_delay = 0.0f;
    bool selected = false;
    float select = 0.0f;
    math::Mat4 world;
    math::Mat4 pick;  // world without tilt, so hover does not jitter
  };

  struct Layout {
    float base_y = -1.85f;
    float base_z = 0.0f;
    float depth_step = 0.03f;  // each card a bit closer, avoids z-fighting
    float spacing = 1.05f;
    float max_width = 9.5f;
    float arc = 0.045f;
    float tilt_per_unit = 0.06f;
    float scale = 0.5f;
    float hover_scale = 0.82f;
    float hover_lift = 0.85f;
    float hover_forward = 1.0f;
    float neighbor_push = 0.6f;
    float anim_speed = 14.0f;
    float tilt_max = 0.3f;
    float tilt_speed = 12.0f;
    float hidden_drop = 1.0f;
    float away_drop = 3.5f;
    float raise_speed = 10.0f;
    float select_lift = 0.2f;
    float select_forward = 0.15f;
    float drag_threshold = 36.0f;  // logical px (1920x1080) of vertical drag
    float inspect_z = 4.2f;
    float inspect_y = -0.05f;
    // Must match the hand camera in BattleScene: z = 10 looking at the origin, 35 degree fov.
    float camera_z = 10.0f;
    float fov_y = 0.6108652f;
    float zoom_min = 0.75f;
    float zoom_max = 1.0f;
    float zoom_start = 0.8f;
    float zoom_step = 0.05f;
    float inspect_speed = 9.0f;
    float rotate_speed = 0.010f;
    float tilt_limit = 0.35f;
    float flip_speed = 9.0f;
    float deal_from_x = -8.5f;
    float deal_speed = 13.0f;
    float deal_stagger = 0.12f;
  };

  void SetModel(const Model* model) { model_ = model; }
  void SetAura(const CardAura* aura) { aura_ = aura; }

  void CenterPose(float height_fraction, math::Vec3& pos, float& scale) const;
  static math::Mat4 PoseMatrix(const math::Vec3& pos, float scale, float yaw, float pitch,
                               float roll);
  void DrawLoose(Gpu& gpu, const math::Mat4& view_proj, const math::Mat4& world,
                 SDL_GPUTexture* front, bool foil, float alpha = 1.0f) const;
  void SetBack(SDL_GPUTexture* back) { back_ = back; }
  void SetNormalMaps(SDL_GPUTexture* front, SDL_GPUTexture* back) {
    front_normal_ = front;
    back_normal_ = back;
  }
  Layout& GetLayout() { return layout_; }

  void Add(SDL_GPUTexture* front, bool foil = false, const CardInfo* info = nullptr);
  void SetAway(bool away) { away_ = away; }
  void SetAwayNow(bool away) {
    away_ = away;
    away_t_ = away ? 1.0f : 0.0f;
  }
  void DelayDeal(float seconds) { pending_delay_ = std::max(pending_delay_, seconds); }
  void Clear() { cards_.clear(); }
  size_t Size() const { return cards_.size(); }

  void Update(float dt, math::Vec2 mouse, const math::Mat4& view_proj, int width, int height);
  void Render(Gpu& gpu, const math::Mat4& view_proj) const;

  void MouseDown(math::Vec2 mouse, bool left);
  void MouseUp(math::Vec2 mouse);

  int Hovered() const { return hovered_; }
  bool Inspecting() const { return inspecting_ >= 0; }
  void StopInspect() { inspecting_ = -1; }
  void Flip();
  void Wheel(float dy);
  const Card& CardAt(size_t i) const { return cards_[i]; }
  int Candidate() const;
  void Deselect();
  Card Remove(size_t index);

 private:
  int PickCard(math::Vec2 mouse, const math::Mat4& view_proj, int width, int height) const;
  bool MouseOverHand(math::Vec2 mouse, const math::Mat4& view_proj, int height) const;
  float CenterOffset(int index) const;
  Model::Skin SkinFor(const Card& card) const;
  math::Mat4 CardMatrix(const Card& card, int index, bool with_tilt) const;
  math::Vec2 MouseOnCard(const Card& card, math::Vec2 mouse, const math::Mat4& view_proj,
                         int width, int height) const;

  const Model* model_ = nullptr;
  const CardAura* aura_ = nullptr;
  void DrawCard(Gpu& gpu, const math::Mat4& view_proj, const Card& card) const;
  SDL_GPUTexture* back_ = nullptr;
  SDL_GPUTexture* front_normal_ = nullptr;
  SDL_GPUTexture* back_normal_ = nullptr;
  Layout layout_;
  std::vector<Card> cards_;
  int hovered_ = -1;
  bool raised_ = false;
  float raise_ = 0.0f;
  bool away_ = false;  // off screen (opponent turn)
  float away_t_ = 0.0f;
  float pending_delay_ = 0.0f;

  int press_card_ = -1;
  math::Vec2 press_pos_{0, 0};
  bool press_consumed_ = false;
  math::Vec2 last_mouse_{-1, -1};
  int inspecting_ = -1;
  int inspect_card_ = -1;  // still animating back; may differ from inspecting_
  float inspect_t_ = 0.0f;
  float inspect_yaw_ = 0.0f;
  float inspect_pitch_ = 0.0f;
  float yaw_target_ = 0.0f;
  float zoom_ = 0.8f;
  float zoom_shown_ = 0.8f;
  bool rotating_ = false;
  math::Mat4 last_view_proj_;  // last frame, for PickCard in MouseDown
  int last_width_ = 1;
  int last_height_ = 1;
};
