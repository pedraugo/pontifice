#include "scripts/hand/hand.h"

#include "scripts/aura/cardAura.h"

#include <algorithm>

#include <cmath>
#include <cstdlib>

#include "scripts/gpu/gpu.h"
#include "scripts/model/model.h"

namespace {

// Frame-rate independent exponential interpolation.
float Approach(float current, float target, float speed, float dt) {
  const float t = 1.0f - std::exp(-speed * dt);
  return math::Lerp(current, target, t);
}

}

void Hand::Add(SDL_GPUTexture* front, bool foil, const CardInfo* info) {
  Card card;
  card.front = front;
  card.foil = foil;
  card.info = info;
  card.enter_delay = pending_delay_;
  pending_delay_ += layout_.deal_stagger;
  cards_.push_back(card);
}

float Hand::CenterOffset(int index) const {
  const int n = static_cast<int>(cards_.size());
  float spacing = layout_.spacing;
  if (n > 1) spacing = std::min(spacing, layout_.max_width / static_cast<float>(n - 1));
  return (static_cast<float>(index) - (n - 1) / 2.0f) * spacing;
}

math::Mat4 Hand::CardMatrix(const Card& card, int index, bool with_tilt) const {
  const float center_offset = CenterOffset(index);
  const float e = card.enter * card.enter * (3.0f - 2.0f * card.enter);
  const float x = math::Lerp(layout_.deal_from_x, center_offset + card.shift, e);
  const float rest_y = layout_.base_y - layout_.arc * center_offset * center_offset;
  const float y = rest_y + layout_.hover_lift * card.hover -
                  layout_.hidden_drop * (1.0f - raise_) - layout_.away_drop * away_t_;
  const float z = layout_.base_z + layout_.depth_step * static_cast<float>(index) +
                  layout_.hover_forward * card.hover;
  const float tilt = -center_offset * layout_.tilt_per_unit * (1.0f - card.hover);
  const float scale = math::Lerp(layout_.scale, layout_.hover_scale, card.hover);

  math::Mat4 m = math::Mat4::Translate({x, y, z}) * math::Mat4::RotateZ(tilt);
  if (with_tilt) m = m * math::Mat4::RotateX(card.tilt_x) * math::Mat4::RotateY(card.tilt_y);
  return m * math::Mat4::Scale(scale);
}

math::Vec2 Hand::MouseOnCard(const Card& card, math::Vec2 mouse, const math::Mat4& view_proj,
                             int width, int height) const {
  const math::Vec3 mn = model_->BoundsMin();
  const math::Vec3 mx = model_->BoundsMax();
  const math::Vec3 center{(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, 0.0f};
  const math::Vec3 right{mx.x, center.y, 0.0f};
  const math::Vec3 top{center.x, mx.y, 0.0f};

  const math::Mat4 mvp = view_proj * card.pick;
  auto to_screen = [&](const math::Vec3& p) {
    const math::Vec4 clip = mvp * math::Vec4{p.x, p.y, p.z, 1.0f};
    const float w = clip.w > 1e-6f ? clip.w : 1e-6f;
    return math::Vec2{(clip.x / w * 0.5f + 0.5f) * width,
                      (1.0f - (clip.y / w * 0.5f + 0.5f)) * height};
  };
  const math::Vec2 c = to_screen(center);
  const math::Vec2 r = to_screen(right);
  const math::Vec2 t = to_screen(top);
  const math::Vec2 d{mouse.x - c.x, mouse.y - c.y};
  const math::Vec2 ax{r.x - c.x, r.y - c.y};
  const math::Vec2 ay{t.x - c.x, t.y - c.y};
  const float lx = ax.x * ax.x + ax.y * ax.y;
  const float ly = ay.x * ay.x + ay.y * ay.y;
  const float u = lx > 1e-6f ? (d.x * ax.x + d.y * ax.y) / lx : 0.0f;
  const float v = ly > 1e-6f ? (d.x * ay.x + d.y * ay.y) / ly : 0.0f;
  return {math::Clamp(u, -1.0f, 1.0f), math::Clamp(v, -1.0f, 1.0f)};
}

int Hand::PickCard(math::Vec2 mouse, const math::Mat4& view_proj, int width, int height) const {
  if (model_ == nullptr || mouse.x < 0 || mouse.y < 0) return -1;

  const math::Vec3 mn = model_->BoundsMin();
  const math::Vec3 mx = model_->BoundsMax();
  const math::Vec3 corners[4] = {
      {mn.x, mn.y, 0}, {mx.x, mn.y, 0}, {mx.x, mx.y, 0}, {mn.x, mx.y, 0}};

  int best = -1;
  float best_dist = 1e9f;
  for (size_t i = 0; i < cards_.size(); ++i) {
    const math::Mat4 mvp = view_proj * cards_[i].pick;
    math::Vec2 screen[4];
    float cx = 0.0f;
    bool behind = false;
    for (int c = 0; c < 4; ++c) {
      const math::Vec4 clip = mvp * math::Vec4{corners[c].x, corners[c].y, corners[c].z, 1.0f};
      if (clip.w <= 0.0f) {
        behind = true;
        break;
      }
      screen[c].x = (clip.x / clip.w * 0.5f + 0.5f) * width;
      screen[c].y = (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * height;
      cx += screen[c].x * 0.25f;
    }
    if (behind || !math::PointInConvexQuad(mouse, screen)) continue;
    const float dist = std::fabs(cx - mouse.x);
    if (dist < best_dist) {
      best_dist = dist;
      best = static_cast<int>(i);
    }
  }
  return best;
}

bool Hand::MouseOverHand(math::Vec2 mouse, const math::Mat4& view_proj, int height) const {
  if (model_ == nullptr || cards_.empty() || mouse.x < 0 || mouse.y < 0) return false;
  if (hovered_ >= 0) return true;
  // Use the raised hand's top line, otherwise the hand would bob up and down under the cursor.
  const float top_y = layout_.base_y + model_->BoundsMax().y * layout_.scale;
  const math::Vec4 clip = view_proj * math::Vec4{0.0f, top_y, layout_.base_z, 1.0f};
  if (clip.w <= 0.0f) return false;
  const float screen_y = (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * height;
  return mouse.y >= screen_y;
}

void Hand::MouseDown(math::Vec2 mouse, bool left, int clicks) {
  if (!left) {
    StopInspect();
    CancelDrag();
    return;
  }
  press_pos_ = mouse;
  press_consumed_ = false;
  if (Inspecting()) {
    rotating_ = true;
    press_card_ = -1;
    return;
  }
  press_card_ = PickCard(mouse, last_view_proj_, last_width_, last_height_);
  if (press_card_ >= 0 && clicks >= 2) {
    Inspect(static_cast<size_t>(press_card_));
    press_card_ = -1;
    press_consumed_ = true;
  }
}

void Hand::SetInspectOrigin(const SDL_FRect& screen_rect, int width, int height) {
  if (model_ == nullptr || width <= 0 || height <= 0) return;
  // Screen rectangle mapped onto the plane z = inspect_z seen by the hand camera.
  const float visible_h = 2.0f * (layout_.camera_z - layout_.inspect_z) *
                          std::tan(layout_.fov_y * 0.5f);
  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  const float cx = screen_rect.x + screen_rect.w / 2.0f;
  const float cy = screen_rect.y + screen_rect.h / 2.0f;
  origin_pos_ = {(cx / static_cast<float>(width) - 0.5f) * visible_h * aspect,
                 (0.5f - cy / static_cast<float>(height)) * visible_h, layout_.inspect_z};
  const float card_h = model_->BoundsMax().y - model_->BoundsMin().y;
  origin_scale_ = (screen_rect.h / static_cast<float>(height)) * visible_h /
                  std::max(card_h, 1e-3f);
  has_origin_ = true;
}

void Hand::Inspect(size_t index) {
  if (index >= cards_.size()) return;
  inspecting_ = static_cast<int>(index);
  inspect_card_ = inspecting_;
  inspect_yaw_ = 0.0f;
  inspect_pitch_ = 0.0f;
  yaw_target_ = 0.0f;
  zoom_ = zoom_shown_ = layout_.zoom_start;
}

void Hand::MouseUp(math::Vec2 mouse, int clicks) {
  if (rotating_) {
    rotating_ = false;
    const float moved = std::fabs(mouse.x - press_pos_.x) + std::fabs(mouse.y - press_pos_.y);
    if (moved < 6.0f && clicks <= 1) {  // click: on the card flips, elsewhere stops
      if (PickCard(mouse, last_view_proj_, last_width_, last_height_) == inspecting_) {
        Flip();
      } else {
        StopInspect();
      }
    } else {
      yaw_target_ = inspect_yaw_;
    }
  }
  CancelDrag();
  press_card_ = -1;
  press_consumed_ = false;
}

void Hand::CancelDrag() {
  dragging_ = -1;
  press_card_ = -1;
}

void Hand::Wheel(float dy) {
  if (!Inspecting()) return;
  zoom_ = math::Clamp(zoom_ + dy * layout_.zoom_step, layout_.zoom_min, layout_.zoom_max);
}

void Hand::Flip() {
  if (!Inspecting()) return;
  const float half = math::kPi;
  const float snapped = std::round(yaw_target_ / half) * half;
  yaw_target_ = snapped + half;
}

Hand::Card Hand::Remove(size_t index) {
  if (index >= cards_.size()) return Card{};
  std::vector<float> before(cards_.size());
  // Others keep their old offset in shift, which animates back to zero, so they slide.
  for (size_t i = 0; i < cards_.size(); ++i) before[i] = CenterOffset(static_cast<int>(i));
  const Card removed = cards_[index];
  cards_.erase(cards_.begin() + static_cast<long>(index));
  for (size_t i = 0; i < cards_.size(); ++i) {
    const size_t old = i < index ? i : i + 1;
    cards_[i].shift += before[old] - CenterOffset(static_cast<int>(i));
  }
  const int idx = static_cast<int>(index);
  auto fix = [idx](int& v) {
    if (v == idx) {
      v = -1;
    } else if (v > idx) {
      --v;
    }
  };
  fix(hovered_);
  fix(press_card_);
  fix(inspecting_);
  fix(inspect_card_);
  fix(dragging_);
  fix(drag_card_);
  return removed;
}

void Hand::Update(float dt, math::Vec2 mouse, const math::Mat4& view_proj, int width,
                  int height) {
  last_view_proj_ = view_proj;
  last_width_ = width;
  last_height_ = height;

  if (press_card_ >= 0 && press_card_ < static_cast<int>(cards_.size()) && !press_consumed_ &&
      mouse.x >= 0) {
    const float moved = std::fabs(mouse.x - press_pos_.x) + std::fabs(mouse.y - press_pos_.y);
    if (moved >= layout_.drag_threshold) {
      dragging_ = press_card_;
      drag_card_ = press_card_;
      press_consumed_ = true;
    }
  }
  if (dragging_ >= 0 && mouse.x >= 0) {
    // Mouse on the plane z = drag_z of the hand camera (at camera_z, looking at the origin).
    const float visible_h = 2.0f * (layout_.camera_z - layout_.drag_z) *
                            std::tan(layout_.fov_y * 0.5f);
    const float aspect = static_cast<float>(width) / static_cast<float>(std::max(height, 1));
    drag_pos_ = {(mouse.x / static_cast<float>(width) - 0.5f) * visible_h * aspect,
                 (0.5f - mouse.y / static_cast<float>(height)) * visible_h, layout_.drag_z};
  }
  drag_t_ = Approach(drag_t_, dragging_ >= 0 ? 1.0f : 0.0f, layout_.drag_speed, dt);
  if (dragging_ < 0 && drag_t_ < 0.01f) drag_card_ = -1;
  if (rotating_ && Inspecting() && mouse.x >= 0 && last_mouse_.x >= 0) {
    inspect_yaw_ += (mouse.x - last_mouse_.x) * layout_.rotate_speed;
    inspect_pitch_ = math::Clamp(inspect_pitch_ + (mouse.y - last_mouse_.y) * layout_.rotate_speed,
                                 -layout_.tilt_limit, layout_.tilt_limit);
    yaw_target_ = inspect_yaw_;
  } else if (Inspecting()) {
    inspect_yaw_ = Approach(inspect_yaw_, yaw_target_, layout_.flip_speed, dt);
    inspect_pitch_ = Approach(inspect_pitch_, 0.0f, layout_.flip_speed * 0.5f, dt);
  }
  last_mouse_ = mouse;
  inspect_t_ = Approach(inspect_t_, Inspecting() ? 1.0f : 0.0f, layout_.inspect_speed, dt);
  zoom_shown_ = Approach(zoom_shown_, zoom_, layout_.inspect_speed, dt);
  if (!Inspecting() && inspect_t_ < 0.01f) inspect_card_ = -1;

  away_t_ = Approach(away_t_, away_ ? 1.0f : 0.0f, layout_.raise_speed * 0.6f, dt);
  const bool interactive = !Inspecting() && !away_ && dragging_ < 0;
  hovered_ = interactive ? PickCard(mouse, view_proj, width, height) : -1;
  raised_ = interactive ? MouseOverHand(mouse, view_proj, height) : false;
  raise_ = Approach(raise_, raised_ ? 1.0f : 0.0f, layout_.raise_speed, dt);
  pending_delay_ = std::max(0.0f, pending_delay_ - dt);

  for (size_t i = 0; i < cards_.size(); ++i) {
    Card& card = cards_[i];
    if (card.enter_delay > 0.0f) {
      card.enter_delay -= dt;
    } else if (card.enter < 1.0f) {
      const float distance =
          std::max(1.0f, CenterOffset(static_cast<int>(i)) - layout_.deal_from_x);
      card.enter = std::min(1.0f, card.enter + dt * layout_.deal_speed / distance);
    }
    const float hover_target = static_cast<int>(i) == hovered_ ? 1.0f : 0.0f;

    float shift_target = 0.0f;
    if (hovered_ >= 0 && static_cast<int>(i) != hovered_) {
      const int d = static_cast<int>(i) - hovered_;
      const float falloff = 1.0f / static_cast<float>(std::abs(d));
      shift_target = (d > 0 ? 1.0f : -1.0f) * layout_.neighbor_push * falloff;
    }

    float tilt_x_target = 0.0f;
    float tilt_y_target = 0.0f;
    if (static_cast<int>(i) == hovered_) {
      const math::Vec2 rel = MouseOnCard(card, mouse, view_proj, width, height);
      tilt_x_target = -rel.y * layout_.tilt_max;
      tilt_y_target = rel.x * layout_.tilt_max;
    }

    card.hover = Approach(card.hover, hover_target, layout_.anim_speed, dt);
    card.shift = Approach(card.shift, shift_target, layout_.anim_speed, dt);
    card.tilt_x = Approach(card.tilt_x, tilt_x_target, layout_.tilt_speed, dt);
    card.tilt_y = Approach(card.tilt_y, tilt_y_target, layout_.tilt_speed, dt);
    card.pick = CardMatrix(card, static_cast<int>(i), false);
    card.world = CardMatrix(card, static_cast<int>(i), true);
    if (static_cast<int>(i) == inspect_card_ && inspect_t_ > 0.0f) {
      // Rotation scaled by progress so the card does not spin while still in the dock.
      const float t = inspect_t_ * inspect_t_ * (3.0f - 2.0f * inspect_t_);
      const math::Mat4& rest = card.pick;
      const math::Vec3 from = has_origin_ ? origin_pos_
                                          : math::Vec3{rest.m[3][0], rest.m[3][1], rest.m[3][2]};
      const math::Vec3 to{0.0f, layout_.inspect_y, layout_.inspect_z};
      const math::Vec3 pos = from + (to - from) * t;
      const float rest_scale =
          has_origin_ ? origin_scale_ : math::Lerp(layout_.scale, layout_.hover_scale, card.hover);
      const float visible_h = 2.0f * (layout_.camera_z - layout_.inspect_z) *
                              std::tan(layout_.fov_y * 0.5f);
      const float card_h = model_->BoundsMax().y - model_->BoundsMin().y;
      const float inspect_scale = zoom_shown_ * visible_h / std::max(card_h, 1e-3f);
      const float scale = math::Lerp(rest_scale, inspect_scale, t);
      card.world = math::Mat4::Translate(pos) * math::Mat4::RotateY(inspect_yaw_ * t) *
                   math::Mat4::RotateX(inspect_pitch_ * t) * math::Mat4::Scale(scale);
      card.pick = card.world;
    }
    if (static_cast<int>(i) == drag_card_ && drag_t_ > 0.0f) {
      const float t = drag_t_ * drag_t_ * (3.0f - 2.0f * drag_t_);
      const math::Mat4& rest = card.pick;
      const math::Vec3 from{rest.m[3][0], rest.m[3][1], rest.m[3][2]};
      const math::Vec3 pos = from + (drag_pos_ - from) * t;
      const float rest_scale = math::Lerp(layout_.scale, layout_.hover_scale, card.hover);
      const float scale = math::Lerp(rest_scale, layout_.drag_scale, t);
      card.world = math::Mat4::Translate(pos) * math::Mat4::Scale(scale);
      card.pick = card.world;
    }
  }
}

void Hand::CenterPose(float height_fraction, math::Vec3& pos, float& scale) const {
  pos = {0.0f, layout_.inspect_y, layout_.inspect_z};
  scale = 1.0f;
  if (model_ == nullptr) return;
  const float visible_h =
      2.0f * (layout_.camera_z - layout_.inspect_z) * std::tan(layout_.fov_y * 0.5f);
  const float card_h = model_->BoundsMax().y - model_->BoundsMin().y;
  scale = height_fraction * visible_h / std::max(card_h, 1e-3f);
}

math::Mat4 Hand::PoseMatrix(const math::Vec3& pos, float scale, float yaw, float pitch,
                            float roll) {
  return math::Mat4::Translate(pos) * math::Mat4::RotateZ(roll) * math::Mat4::RotateY(yaw) *
         math::Mat4::RotateX(pitch) * math::Mat4::Scale(scale);
}

void Hand::DrawLoose(Gpu& gpu, const math::Mat4& view_proj, const math::Mat4& world,
                     SDL_GPUTexture* front, bool foil, float alpha) const {
  if (model_ == nullptr || front == nullptr || alpha <= 0.0f) return;
  if (foil && aura_ != nullptr) aura_->Draw(gpu, world, view_proj, alpha);
  model_->Draw(gpu, world, view_proj, SDL_FColor{1, 1, 1, alpha},
               Model::Skin{front, back_, front_normal_, back_normal_,
                           foil ? Gpu::DrawMode::kFoil : Gpu::DrawMode::kMesh});
}

Model::Skin Hand::SkinFor(const Card& card) const {
  return Model::Skin{card.front, back_, front_normal_, back_normal_,
                     card.foil ? Gpu::DrawMode::kFoil : Gpu::DrawMode::kMesh};
}

void Hand::DrawCard(Gpu& gpu, const math::Mat4& view_proj, const Card& card) const {
  if (card.foil && aura_ != nullptr) aura_->Draw(gpu, card.world, view_proj);
  model_->Draw(gpu, card.world, view_proj, SDL_FColor{1, 1, 1, 1}, SkinFor(card));
}

void Hand::Render(Gpu& gpu, const math::Mat4& view_proj) const {
  if (model_ == nullptr) return;
  // Back to front by index, hovered, dragged then inspected card last; blending needs depth order.
  auto last = [&](int index) {
    return index == hovered_ || index == drag_card_ || index == inspect_card_;
  };
  for (size_t i = 0; i < cards_.size(); ++i) {
    if (last(static_cast<int>(i))) continue;
    DrawCard(gpu, view_proj, cards_[i]);
  }
  int drawn = -1;
  for (int index : {hovered_, drag_card_, inspect_card_}) {
    if (index < 0 || index >= static_cast<int>(cards_.size()) || index == drawn) continue;
    DrawCard(gpu, view_proj, cards_[index]);
    drawn = index;
  }
}
