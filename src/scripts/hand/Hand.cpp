#include "scripts/hand/Hand.h"

#include <cmath>
#include <cstdlib>

#include "scripts/gpu/Gpu.h"
#include "scripts/model/Model.h"

namespace {

// Interpolação exponencial independente do frame rate.
float Approach(float current, float target, float speed, float dt) {
  const float t = 1.0f - std::exp(-speed * dt);
  return math::Lerp(current, target, t);
}

bool PointInConvexQuad(math::Vec2 p, const math::Vec2 q[4]) {
  float sign = 0.0f;
  for (int i = 0; i < 4; ++i) {
    const math::Vec2 a = q[i];
    const math::Vec2 b = q[(i + 1) % 4];
    const float cross = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
    if (std::fabs(cross) < 1e-6f) continue;
    if (sign == 0.0f) {
      sign = cross;
    } else if ((cross > 0) != (sign > 0)) {
      return false;
    }
  }
  return sign != 0.0f;
}

}  // namespace

void Hand::Add(SDL_GPUTexture* front) {
  Card card;
  card.front = front;
  cards_.push_back(card);
}

math::Mat4 Hand::CardMatrix(const Card& card, int index) const {
  const int n = static_cast<int>(cards_.size());
  float spacing = layout_.spacing;
  if (n > 1) spacing = std::min(spacing, layout_.max_width / static_cast<float>(n - 1));

  const float center_offset = (static_cast<float>(index) - (n - 1) / 2.0f) * spacing;
  const float x = center_offset + card.shift;
  const float rest_y = layout_.base_y - layout_.arc * center_offset * center_offset;
  const float y = rest_y + layout_.hover_lift * card.hover;
  // Profundidade crescente com o índice: cartas vizinhas se sobrepõem em X e, no mesmo Z,
  // o teste de profundidade empata e pisca. A carta destacada vai além de todas.
  const float z = layout_.base_z + layout_.depth_step * static_cast<float>(index) +
                  layout_.hover_forward * card.hover;
  const float tilt = -center_offset * layout_.tilt_per_unit * (1.0f - card.hover);
  const float scale = math::Lerp(layout_.scale, layout_.hover_scale, card.hover);

  return math::Mat4::Translate({x, y, z}) * math::Mat4::RotateZ(tilt) *
         math::Mat4::Scale(scale);
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
    const math::Mat4 mvp = view_proj * cards_[i].world;
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
    if (behind || !PointInConvexQuad(mouse, screen)) continue;
    // Entre cartas sobrepostas, vence a que tem o centro mais perto do cursor.
    const float dist = std::fabs(cx - mouse.x);
    if (dist < best_dist) {
      best_dist = dist;
      best = static_cast<int>(i);
    }
  }
  return best;
}

void Hand::Update(float dt, math::Vec2 mouse, const math::Mat4& view_proj, int width,
                  int height) {
  hovered_ = PickCard(mouse, view_proj, width, height);

  for (size_t i = 0; i < cards_.size(); ++i) {
    Card& card = cards_[i];
    const float hover_target = static_cast<int>(i) == hovered_ ? 1.0f : 0.0f;

    float shift_target = 0.0f;
    if (hovered_ >= 0 && static_cast<int>(i) != hovered_) {
      const int d = static_cast<int>(i) - hovered_;
      const float falloff = 1.0f / static_cast<float>(std::abs(d));  // 1, 1/2, 1/3...
      shift_target = (d > 0 ? 1.0f : -1.0f) * layout_.neighbor_push * falloff;
    }

    card.hover = Approach(card.hover, hover_target, layout_.anim_speed, dt);
    card.shift = Approach(card.shift, shift_target, layout_.anim_speed, dt);
    card.world = CardMatrix(card, static_cast<int>(i));
  }
}

void Hand::Render(Gpu& gpu, const math::Mat4& view_proj) const {
  if (model_ == nullptr) return;
  const SDL_FColor white{1, 1, 1, 1};
  // Ordem de trás para frente (índice crescente = mais perto da câmera) e a carta destacada
  // por último. Com blend ligado, desenhar na ordem da profundidade evita bordas erradas.
  for (size_t i = 0; i < cards_.size(); ++i) {
    if (static_cast<int>(i) == hovered_) continue;
    model_->Draw(gpu, cards_[i].world, view_proj, white, cards_[i].front, back_);
  }
  if (hovered_ >= 0 && hovered_ < static_cast<int>(cards_.size())) {
    model_->Draw(gpu, cards_[hovered_].world, view_proj, white, cards_[hovered_].front, back_);
  }
}
