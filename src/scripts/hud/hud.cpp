#include "scripts/hud/hud.h"

#include <algorithm>
#include <cmath>

#include "scripts/colors/colors.h"
#include "scripts/frame/frame.h"
#include "scripts/i18n/i18n.h"
#include "scripts/text/text.h"
#include "scripts/ui/ui.h"

namespace {

constexpr float kMargin = ui::Grid(1);
constexpr float kPadding = ui::Grid(1);
constexpr float kPanelWidth = ui::Grid(34);
constexpr float kPanelHeight = ui::Grid(6);
constexpr float kFieldPanelWidth = ui::Grid(24);
constexpr float kFieldPanelHeight = ui::Grid(6);
constexpr float kCounterHeight = ui::Grid(3);
constexpr float kLineScale = 2.0f * ui::kScale;
constexpr float kDetailScale = 1.5f * ui::kScale;
constexpr float kResultScale = 6.0f * ui::kScale;
constexpr float kLineHeight = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * kLineScale;
constexpr float kDetailHeight = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * kDetailScale;
constexpr float kLineGap = ui::Px(8.0f);
// Card front sprites are 1600x2230; the art window is 668x448, zoomed at native size.
constexpr float kCardAspect = 1600.0f / 2230.0f;
constexpr float kArtAspect = 668.0f / 448.0f;
constexpr float kZoomWidth = 668.0f;
constexpr float kZoomHeight = 448.0f;
constexpr float kZoomSeconds = 0.15f;
constexpr float kLifeShakeSeconds = 0.4f;
constexpr float kLifeShakeAmplitude = 9.0f;
constexpr float kPopupSeconds = 1.2f;
constexpr float kPopupFade = 0.4f;
// Damage number: initial upward speed (px/s), gravity (px/s^2), lateral drift (px/s).
constexpr float kPopupJump = 230.0f;
constexpr float kPopupGravity = 900.0f;
constexpr float kPopupDrift = 110.0f;
constexpr float kPopupScale = 3.5f * ui::kScale;

void FillRect(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(r, &rect);
}

void StrokeRect(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderRect(r, &rect);
}

void Panel(SDL_Renderer* r, const SDL_FRect& rect) { frame::Draw(r, rect, frame::Kind::kWindow); }

std::string CategoryName(Category c) {
  return i18n::Get(c == Category::kGood ? "category.good.plural" : "category.evil.plural");
}
Category Other(Category c) { return c == Category::kGood ? Category::kEvil : Category::kGood; }

void DrawRight(SDL_Renderer* r, const char* str, float right_x, float y, float scale,
               SDL_Color color) {
  text::Draw(r, str, right_x - text::Width(str, scale), y, scale, color);
}

std::string LifeText(int life, int max) {
  return std::to_string(std::max(life, 0)) + " / " + std::to_string(max);
}

}

void Hud::SetLife(int opponent, int player) {
  if (opponent < opponent_life_) OnDamage(false, opponent_life_ - opponent);
  if (player < player_life_) OnDamage(true, player_life_ - player);
  opponent_life_ = std::clamp(opponent, 0, kMaxLife);
  player_life_ = std::clamp(player, 0, kMaxLife);
}

void Hud::OnDamage(bool player, int amount) {
  if (amount <= 0) return;
  life_shake_ = kLifeShakeSeconds;
  life_flash_[player ? 1 : 0] = kLifeShakeSeconds * 1.5f;
  DamagePopup popup;
  popup.player = player;
  popup.amount = amount;
  // Consecutive hits drift to alternating sides so the numbers do not overlap.
  popup.drift = (popups_.size() % 2 == 0) ? -1.0f : -0.35f;
  popups_.push_back(popup);
}

void Hud::SetFieldCard(const FieldCardInfo& card) {
  field_ = card;
  has_field_ = true;
}

void Hud::ClearFieldCard() {
  field_ = FieldCardInfo{};
  has_field_ = false;
}

void Hud::SetDeck(int used, int remaining) {
  deck_used_ = std::clamp(used, 0, kDeckSize);
  deck_remaining_ = std::clamp(remaining, 0, kDeckSize);
}

void Hud::SetGraveyard(int count) { graveyard_ = std::max(count, 0); }

void Hud::Update(float dt, float mouse_x, float mouse_y) {
  const FieldLayout layout = LayoutField();
  auto inside = [mouse_x, mouse_y](const SDL_FRect& rect) {
    return mouse_x >= rect.x && mouse_x < rect.x + rect.w && mouse_y >= rect.y &&
           mouse_y < rect.y + rect.h;
  };
  zoom_hover_ = has_field_ && layout.art &&
                (inside(layout.thumb) || (zoom_t_ > 0.5f && inside(ZoomRect())));
  const float step = dt / kZoomSeconds;
  zoom_t_ = std::clamp(zoom_t_ + (zoom_hover_ ? step : -step), 0.0f, 1.0f);

  if (life_shake_ > 0.0f) {
    life_shake_ = std::max(0.0f, life_shake_ - dt);
    life_shake_time_ += dt;
  }
  for (float& flash : life_flash_) flash = std::max(0.0f, flash - dt);
  for (DamagePopup& popup : popups_) popup.time += dt;
  popups_.erase(std::remove_if(popups_.begin(), popups_.end(),
                               [](const DamagePopup& p) { return p.time >= kPopupSeconds; }),
                popups_.end());
}

void Hud::Render(SDL_Renderer* r, int width, int height) const {
  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &previous);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  RenderFieldPanel(r);
  RenderLifePanel(r, width);
  RenderPilesPanel(r, width, height);
  RenderFieldZoom(r);
  RenderDamagePopups(r, width);
  if (Over()) RenderResult(r, width, height);

  SDL_SetRenderDrawBlendMode(r, previous);
}

Hud::FieldLayout Hud::LayoutField() const {
  FieldLayout layout;
  const float thumb_h = kFieldPanelHeight - 2 * kPadding;
  layout.art = has_field_ && field_.art != nullptr;
  const float thumb_w = thumb_h * (layout.art ? kArtAspect : kCardAspect);
  float width = kFieldPanelWidth;
  if (has_field_) {
    layout.buff =
        std::string("+") + std::to_string(field_.buff) + " " + CategoryName(field_.favored);
    layout.nerf = std::string("-") + std::to_string(field_.nerf) + " " +
                  CategoryName(Other(field_.favored));
    const float text_w = std::max({text::Width(field_.name.c_str(), kLineScale),
                                   text::Width(layout.buff.c_str(), kDetailScale),
                                   text::Width(layout.nerf.c_str(), kDetailScale)});
    width = std::max(width, ui::SnapUp(3 * kPadding + thumb_w + text_w));
  }
  layout.panel = SDL_FRect{kMargin, kMargin, width, kFieldPanelHeight};
  layout.thumb = SDL_FRect{kMargin + kPadding, kMargin + kPadding, thumb_w, thumb_h};
  return layout;
}

SDL_FRect Hud::ZoomRect() const {
  return SDL_FRect{kMargin + kPadding, kMargin + kFieldPanelHeight + ui::Grid(1) + kPadding,
                   kZoomWidth, kZoomHeight};
}

void Hud::RenderFieldZoom(SDL_Renderer* r) const {
  if (zoom_t_ <= 0.0f || !has_field_ || field_.art == nullptr) return;
  const float t = zoom_t_ * zoom_t_ * (3.0f - 2.0f * zoom_t_);  // smoothstep easing
  const SDL_FRect from = LayoutField().thumb;
  const SDL_FRect to = ZoomRect();
  const SDL_FRect rect{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
                       from.w + (to.w - from.w) * t, from.h + (to.h - from.h) * t};
  const float pad = kPadding * t;
  const SDL_FRect plate{rect.x - pad, rect.y - pad, rect.w + 2 * pad, rect.h + 2 * pad};
  frame::Draw(r, plate, frame::Kind::kWindow, frame::State::kNormal, t);
  SDL_Texture* texture = field_.art_large != nullptr ? field_.art_large : field_.art;
  SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(255.0f * std::min(1.0f, 0.4f + t)));
  SDL_RenderTexture(r, texture, nullptr, &rect);
  SDL_SetTextureAlphaMod(texture, 255);
  StrokeRect(r, rect, colors::Get("panel.border"));
}

void Hud::RenderFieldPanel(SDL_Renderer* r) const {
  const FieldLayout layout = LayoutField();
  const SDL_FRect& panel = layout.panel;
  const std::string& buff = layout.buff;
  const std::string& nerf = layout.nerf;
  Panel(r, panel);

  if (!has_field_) {
    text::DrawCentered(r, i18n::Get("hud.noField").c_str(), panel.x + panel.w / 2.0f, panel.y + panel.h / 2.0f,
                       kLineScale, colors::Get("text.muted"));
    return;
  }

  const SDL_FRect card = layout.thumb;
  SDL_Texture* thumb = layout.art ? field_.art : field_.image;
  if (thumb != nullptr) {
    SDL_RenderTexture(r, thumb, nullptr, &card);
  } else {
    FillRect(r, card, colors::Get("border.subtle"));
  }
  StrokeRect(r, card, colors::Get("panel.border"));

  const float text_x = card.x + card.w + kPadding;
  const float small_gap = ui::Grid(0.25f);
  float y = card.y;
  text::Draw(r, field_.name.c_str(), text_x, y, kLineScale, colors::Get("hud.fieldName"));
  y += kLineHeight + small_gap;
  text::Draw(r, buff.c_str(), text_x, y, kDetailScale, colors::Get("status.buff"));
  y += kDetailHeight + small_gap;
  text::Draw(r, nerf.c_str(), text_x, y, kDetailScale, colors::Get("status.nerf"));
}

void Hud::RenderLifePanel(SDL_Renderer* r, int width) const {
  float dx = 0.0f, dy = 0.0f;
  if (life_shake_ > 0.0f) {
    const float k = life_shake_ / kLifeShakeSeconds;
    const float a = kLifeShakeAmplitude * k * k;
    dx = a * std::sin(life_shake_time_ * 61.0f);
    dy = a * 0.5f * std::sin(life_shake_time_ * 47.0f + 1.1f);
  }
  const SDL_FRect panel{width - kMargin - kPanelWidth + dx, kMargin + dy, kPanelWidth,
                        kPanelHeight};
  Panel(r, panel);

  const float left = panel.x + kPadding;
  const float right = panel.x + panel.w - kPadding;
  float y = panel.y + (panel.h - (2 * kLineHeight + kLineGap)) / 2.0f;
  const SDL_Color label = colors::Get("hud.lifeLabel");
  const SDL_Color value = colors::Get("hud.lifeValue");
  const SDL_Color zero = colors::Get("hud.lifeZero");
  const SDL_Color hit = colors::Get("status.nerf");
  auto value_color = [&](int life, float flash) {
    if (life <= 0) return zero;
    // Blink: 80 ms red, 80 ms normal while the flash lasts.
    return flash > 0.0f && std::fmod(flash, 0.16f) > 0.08f ? hit : value;
  };
  text::Draw(r, i18n::Get("hud.opponent").c_str(), left, y, kLineScale, label);
  DrawRight(r, LifeText(opponent_life_, kMaxLife).c_str(), right, y, kLineScale,
            value_color(opponent_life_, life_flash_[0]));
  y += kLineHeight + kLineGap;
  text::Draw(r, i18n::Get("hud.player").c_str(), left, y, kLineScale, label);
  DrawRight(r, LifeText(player_life_, kMaxLife).c_str(), right, y, kLineScale,
            value_color(player_life_, life_flash_[1]));
}

void Hud::RenderDamagePopups(SDL_Renderer* r, int width) const {
  if (popups_.empty()) return;
  const float panel_x = width - kMargin - kPanelWidth;
  const float right = panel_x + kPanelWidth - kPadding;
  const float top = kMargin + (kPanelHeight - (2 * kLineHeight + kLineGap)) / 2.0f;
  const SDL_Color red = colors::Get("status.nerf");
  const SDL_Color shadow = colors::Get("text.shadow");
  for (const DamagePopup& popup : popups_) {
    const float t = popup.time;
    const float row_y = top + (popup.player ? kLineHeight + kLineGap : 0.0f);
    const float x = right - ui::Grid(4) + popup.drift * kPopupDrift * t;
    const float y = row_y - kPopupJump * t + 0.5f * kPopupGravity * t * t;
    const float pop = t < 0.15f ? 1.0f + 0.6f * std::sin(t / 0.15f * 3.14159f) : 1.0f;
    const float alpha = 1.0f - std::clamp((t - (kPopupSeconds - kPopupFade)) / kPopupFade, 0.0f, 1.0f);
    const std::string label = "-" + std::to_string(popup.amount);
    const float scale = kPopupScale * pop;
    const float w = text::Width(label.c_str(), scale);
    SDL_Color s = shadow, c = red;
    s.a = static_cast<Uint8>(200.0f * alpha);
    c.a = static_cast<Uint8>(255.0f * alpha);
    const float off = ui::Px(2.0f);
    text::Draw(r, label.c_str(), x - w / 2.0f + off, y + off, scale, s);
    text::Draw(r, label.c_str(), x - w / 2.0f, y, scale, c);
  }
}

namespace {
constexpr float kCounterGap = ui::Grid(1);
constexpr float kPilesGap = ui::Grid(3);
}

void Hud::RenderPilesPanel(SDL_Renderer* r, int width, int height) const {
  const std::string deck_title = i18n::Get("hud.deck");
  const std::string deck_value =
      std::to_string(deck_remaining_) + "/" + std::to_string(kDeckSize);
  const std::string grave_title = i18n::Get("hud.graveyard");
  const std::string grave_value = std::to_string(graveyard_);
  // Width measured with the widest values so the panel does not resize on every draw.
  const std::string widest_deck = std::to_string(kDeckSize) + "/" + std::to_string(kDeckSize);
  const std::string widest_grave = graveyard_ > 99 ? grave_value : std::string("99");
  const float deck_title_w = text::Width(deck_title.c_str(), kLineScale);
  const float panel_w =
      ui::SnapUp(2 * kPadding + deck_title_w + kCounterGap +
                 text::Width(widest_deck.c_str(), kLineScale) + kPilesGap +
                 text::Width(grave_title.c_str(), kLineScale) + kCounterGap +
                 text::Width(widest_grave.c_str(), kLineScale));
  const SDL_FRect panel{width - kMargin - panel_w, height - kMargin - kCounterHeight, panel_w,
                        kCounterHeight};
  Panel(r, panel);
  const float y = panel.y + (panel.h - kLineHeight) / 2.0f;
  const float left = panel.x + kPadding;
  const float right = panel.x + panel.w - kPadding;
  text::Draw(r, deck_title.c_str(), left, y, kLineScale, colors::Get("panel.title"));
  text::Draw(r, deck_value.c_str(), left + deck_title_w + kCounterGap, y, kLineScale,
             deck_remaining_ > 0 ? colors::Get("hud.counterValue")
                                 : colors::Get("hud.deckEmpty"));
  DrawRight(r, grave_value.c_str(), right, y, kLineScale, colors::Get("hud.counterValue"));
  DrawRight(r, grave_title.c_str(),
            right - text::Width(grave_value.c_str(), kLineScale) - kCounterGap, y, kLineScale,
            colors::Get("panel.title"));
}

void Hud::RenderResult(SDL_Renderer* r, int width, int height) const {
  FillRect(r, SDL_FRect{0, 0, static_cast<float>(width), static_cast<float>(height)},
           colors::Get("background.overlay"));
  const float cx = width / 2.0f;
  const float cy = height / 2.0f;
  // Both at 0 cannot happen by the rules; if it does, it counts as a defeat.
  const bool won = opponent_life_ <= 0 && player_life_ > 0;
  text::DrawCentered(r, i18n::Get(won ? "hud.victory" : "hud.defeat").c_str(), cx,
                     cy - ui::Grid(2), kResultScale,
                     colors::Get(won ? "status.victory" : "status.defeat"));
  text::DrawCentered(r, i18n::Get("hud.escMenu").c_str(), cx, cy + ui::Grid(4), kLineScale,
                     colors::Get("text.primary"));
}
