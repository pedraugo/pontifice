#include "scripts/hand/Hand.h"

#include <algorithm>

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
  // Entrada: desliza de fora da tela (esquerda) até o lugar, com suavização nas pontas.
  const float e = card.enter * card.enter * (3.0f - 2.0f * card.enter);
  const float x = math::Lerp(layout_.deal_from_x, center_offset + card.shift, e);
  const float rest_y = layout_.base_y - layout_.arc * center_offset * center_offset;
  const float y = rest_y + layout_.hover_lift * card.hover -
                  layout_.hidden_drop * (1.0f - raise_);
  // Profundidade crescente com o índice: cartas vizinhas se sobrepõem em X e, no mesmo Z,
  // o teste de profundidade empata e pisca. A carta destacada vai além de todas.
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
  // Projeta (mouse - centro) nos eixos da carta em tela (aproximação afim, boa para a mão).
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

bool Hand::MouseOverHand(math::Vec2 mouse, const math::Mat4& view_proj, int height) const {
  if (model_ == nullptr || cards_.empty() || mouse.x < 0 || mouse.y < 0) return false;
  if (hovered_ >= 0) return true;
  // Linha do topo da mão já levantada (carta central, em repouso). Abaixo dela o mouse
  // "pertence" à mão: usar a posição levantada, e não a atual, evita que a mão suba, saia
  // de baixo do cursor e volte a descer em loop.
  const float top_y = layout_.base_y + model_->BoundsMax().y * layout_.scale;
  const math::Vec4 clip = view_proj * math::Vec4{0.0f, top_y, layout_.base_z, 1.0f};
  if (clip.w <= 0.0f) return false;
  const float screen_y = (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * height;
  return mouse.y >= screen_y;
}

void Hand::Update(float dt, math::Vec2 mouse, const math::Mat4& view_proj, int width,
                  int height) {
  hovered_ = PickCard(mouse, view_proj, width, height);
  raised_ = MouseOverHand(mouse, view_proj, height);
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
      const float falloff = 1.0f / static_cast<float>(std::abs(d));  // 1, 1/2, 1/3...
      shift_target = (d > 0 ? 1.0f : -1.0f) * layout_.neighbor_push * falloff;
    }

    // Inclinação: a quina sob o mouse afunda (quinas de cima inclinam para trás, de baixo
    // para a frente; esquerda/direita idem). Fora do hover volta a zero.
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
