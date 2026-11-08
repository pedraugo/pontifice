#include "scripts/hud/hud.h"

#include <algorithm>
#include <cmath>

#include "scripts/button/button.h"
#include "scripts/cardface/cardFace.h"
#include "scripts/colors/colors.h"
#include "scripts/opacity/opacity.h"
#include "scripts/frame/frame.h"
#include "scripts/i18n/i18n.h"
#include "scripts/text/text.h"
#include "scripts/layout/layout.h"
#include "scripts/ui/ui.h"

namespace {

// Sizes from ui.json, animations from layout.json, text from text.json ("hud"); read once.
struct Layout {
  float margin = ui::Cells("hud.margin", 1.0f);
  float padding = ui::Cells("hud.padding", 1.0f);
  float panel_w = ui::Cells("hud.life.width", 16.0f);
  float panel_h = ui::Cells("hud.life.height", 6.0f);
  float field_panel_w = ui::Cells("hud.field.width", 24.0f);
  float field_panel_h = ui::Cells("hud.field.height", 6.0f);
  float counter_h = ui::Cells("hud.piles.height", 3.5f);
  float piles_padding = ui::Cells("hud.piles.padding", 1.5f);
  float counter_gap = ui::Cells("hud.piles.counterGap", 1.0f);
  float piles_gap = ui::Cells("hud.piles.gap", 3.0f);
  float result_w = ui::Cells("hud.result.width", 44.0f);
  float result_padding = ui::Cells("hud.result.padding", 2.0f);
  float result_title_gap = ui::Cells("hud.result.titleGap", 1.0f);
  float result_card_gap = ui::Cells("hud.result.cardGap", 2.0f);
  float result_button_h = button::Cells("button.regular.height", 3.5f);
  float result_row_gap = ui::Cells("hud.result.rowGap", 0.5f);
  float result_column_w = ui::Cells("hud.result.columnWidth", 9.0f);
  float line_gap = ui::Cells("hud.life.lineGap", 0.75f);
  float shake_seconds = layout::Number("hud.life.shake.seconds", 0.4f);
  float shake_amplitude = layout::Number("hud.life.shake.amplitudePx", 9.0f);
  float shake_frequency_x = layout::Number("hud.life.shake.frequencyX", 61.0f);
  float shake_frequency_y = layout::Number("hud.life.shake.frequencyY", 47.0f);
  float shake_ratio_y = layout::Number("hud.life.shake.ratioY", 0.5f);
  float shake_phase_y = layout::Number("hud.life.shake.phaseY", 1.1f);
  float flash_seconds = layout::Number("hud.life.flash.seconds", 0.6f);
  float flash_blink = layout::Number("hud.life.flash.blinkSeconds", 0.16f);
  float life_warning = layout::Number("hud.life.tiers.warning", 2500.0f);
  float life_danger = layout::Number("hud.life.tiers.danger", 1000.0f);
  float life_critical = layout::Number("hud.life.tiers.critical", 500.0f);
  float field_text_gap = ui::Cells("hud.field.textGap", 0.25f);
  float zoom_gap = ui::Cells("hud.field.zoom.gap", 1.0f);
  float zoom_w = ui::Number("hud.field.zoom.widthPx", 668.0f);
  float zoom_h = ui::Number("hud.field.zoom.heightPx", 448.0f);
  float zoom_seconds = layout::Number("hud.field.zoom.seconds", 0.15f);
  float popup_seconds = layout::Number("hud.popup.seconds", 1.2f);
  float popup_fade = layout::Number("hud.popup.fadeSeconds", 0.4f);
  float popup_jump = layout::Number("hud.popup.jumpSpeedPx", 230.0f);
  float popup_gravity = layout::Number("hud.popup.gravityPx", 900.0f);
  float popup_drift = layout::Number("hud.popup.driftSpeedPx", 110.0f);
  float popup_drift_even = layout::Number("hud.popup.driftEven", -1.0f);
  float popup_drift_odd = layout::Number("hud.popup.driftOdd", -0.35f);
  float popup_offset_x = ui::Cells("hud.popup.offsetX", 4.0f);
  float popup_pop_seconds = layout::Number("hud.popup.popSeconds", 0.15f);
  float popup_pop_grow = layout::Number("hud.popup.popGrow", 0.6f);
  float popup_shadow_offset = ui::Number("hud.popup.shadowOffsetPx", 3.0f);
  text::Style line_style = text::Styles({"regular"});
  text::Style body_style = text::Styles({"description"});
  text::Style detail_style = text::Styles({"xsmall"});
  text::Style result_style = text::Styles({"heading"});
  text::Style popup_style = text::Styles({"subtitle"});
  float line_h = text::LineHeight(line_style);
  float detail_h = text::LineHeight(detail_style);
};

const Layout& L() {
  static const Layout layout;
  return layout;
}

constexpr float kCardAspect = static_cast<float>(cardface::kWidth) / cardface::kHeight;
// Art window of the card front sprite (668x448 px).
constexpr float kArtAspect = 668.0f / 448.0f;

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
  return i18n::Get(c == Category::kGood ? "category.plural.good" : "category.plural.evil");
}
Category Other(Category c) { return c == Category::kGood ? Category::kEvil : Category::kGood; }

void DrawRight(SDL_Renderer* r, const char* str, float right_x, float y, const text::Style& style,
               SDL_Color color) {
  text::Draw(r, str, right_x - text::Width(str, style), y, style, color);
}

std::string LifeText(int life) { return std::to_string(std::max(life, 0)); }

std::string ClockText(float seconds) {
  const int total = std::max(0, static_cast<int>(seconds));
  char buf[16];
  SDL_snprintf(buf, sizeof(buf), "%02d:%02d", total / 60, total % 60);
  return buf;
}

}

void Hud::SetLife(int opponent, int player) {
  if (opponent < opponent_life_) OnDamage(false, opponent_life_ - opponent);
  if (player < player_life_) OnDamage(true, player_life_ - player);
  opponent_life_ = std::clamp(opponent, 0, rules::Get().max_life);
  player_life_ = std::clamp(player, 0, rules::Get().max_life);
}

void Hud::OnDamage(bool player, int amount) {
  if (amount <= 0) return;
  life_shake_ = L().shake_seconds;
  life_flash_[player ? 1 : 0] = L().flash_seconds;
  DamagePopup popup;
  popup.player = player;
  popup.amount = amount;
  // Consecutive hits drift to alternating sides so the numbers do not overlap.
  popup.drift = (popups_.size() % 2 == 0) ? L().popup_drift_even : L().popup_drift_odd;
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
  deck_used_ = std::max(used, 0);
  deck_remaining_ = std::max(remaining, 0);
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
  const float step = dt / L().zoom_seconds;
  zoom_t_ = std::clamp(zoom_t_ + (zoom_hover_ ? step : -step), 0.0f, 1.0f);

  if (life_shake_ > 0.0f) {
    life_shake_ = std::max(0.0f, life_shake_ - dt);
    life_shake_time_ += dt;
  }
  for (float& flash : life_flash_) flash = std::max(0.0f, flash - dt);
  for (DamagePopup& popup : popups_) popup.time += dt;
  popups_.erase(std::remove_if(popups_.begin(), popups_.end(),
                               [](const DamagePopup& p) { return p.time >= L().popup_seconds; }),
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
  const float thumb_h = L().field_panel_h - 2 * L().padding;
  layout.art = has_field_ && field_.art != nullptr;
  const float thumb_w = thumb_h * (layout.art ? kArtAspect : kCardAspect);
  float width = L().field_panel_w;
  if (has_field_) {
    layout.buff =
        std::string("+") + std::to_string(field_.buff) + " " + CategoryName(field_.favored);
    layout.nerf = std::string("-") + std::to_string(field_.nerf) + " " +
                  CategoryName(Other(field_.favored));
    const float text_w = std::max({text::Width(field_.name.c_str(), L().line_style),
                                   text::Width(layout.buff.c_str(), L().detail_style),
                                   text::Width(layout.nerf.c_str(), L().detail_style)});
    width = std::max(width, ui::SnapUp(3 * L().padding + thumb_w + text_w));
  }
  layout.panel = SDL_FRect{L().margin, L().margin, width, L().field_panel_h};
  layout.thumb = SDL_FRect{L().margin + L().padding, L().margin + L().padding, thumb_w, thumb_h};
  return layout;
}

SDL_FRect Hud::ZoomRect() const {
  const Layout& l = L();
  return SDL_FRect{l.margin + l.padding, l.margin + l.field_panel_h + l.zoom_gap + l.padding,
                   l.zoom_w, l.zoom_h};
}

void Hud::RenderFieldZoom(SDL_Renderer* r) const {
  if (zoom_t_ <= 0.0f || !has_field_ || field_.art == nullptr) return;
  const float t = zoom_t_ * zoom_t_ * (3.0f - 2.0f * zoom_t_);  // smoothstep easing
  const SDL_FRect from = LayoutField().thumb;
  const SDL_FRect to = ZoomRect();
  const SDL_FRect rect{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t,
                       from.w + (to.w - from.w) * t, from.h + (to.h - from.h) * t};
  const float pad = L().padding * t;
  const SDL_FRect plate{rect.x - pad, rect.y - pad, rect.w + 2 * pad, rect.h + 2 * pad};
  frame::Draw(r, plate, frame::Kind::kWindow, frame::State::kNormal, t);
  SDL_Texture* texture = field_.art_large != nullptr ? field_.art_large : field_.art;
  const float art_alpha = std::min(1.0f, opacity::Get("fieldZoomArtStart") + t);
  SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(255.0f * art_alpha));
  SDL_RenderTexture(r, texture, nullptr, &rect);
  SDL_SetTextureAlphaMod(texture, 255);
  StrokeRect(r, rect, colors::Get("steel"));
}

void Hud::RenderFieldPanel(SDL_Renderer* r) const {
  const FieldLayout layout = LayoutField();
  const SDL_FRect& panel = layout.panel;
  const std::string& buff = layout.buff;
  const std::string& nerf = layout.nerf;
  Panel(r, panel);

  if (!has_field_) {
    text::DrawCentered(r, i18n::Get("hud.noField").c_str(), panel.x + panel.w / 2.0f, panel.y + panel.h / 2.0f,
                       L().line_style, colors::Get("slate"));
    return;
  }

  const SDL_FRect card = layout.thumb;
  SDL_Texture* thumb = layout.art ? field_.art : field_.image;
  if (thumb != nullptr) {
    SDL_RenderTexture(r, thumb, nullptr, &card);
  } else {
    FillRect(r, card, colors::Get("stone"));
  }
  StrokeRect(r, card, colors::Get("steel"));

  const float text_x = card.x + card.w + L().padding;
  const float small_gap = L().field_text_gap;
  float y = card.y;
  text::Draw(r, field_.name.c_str(), text_x, y, L().line_style, colors::Get("silverLight"));
  y += L().line_h + small_gap;
  text::Draw(r, buff.c_str(), text_x, y, L().detail_style, colors::Get("green"));
  y += L().detail_h + small_gap;
  text::Draw(r, nerf.c_str(), text_x, y, L().detail_style, colors::Get("red"));
}

SDL_FRect Hud::LifePanelRect(int width) const {
  return SDL_FRect{width - L().margin - L().panel_w, L().margin, L().panel_w, L().panel_h};
}

void Hud::RenderLifePanel(SDL_Renderer* r, int width) const {
  float dx = 0.0f, dy = 0.0f;
  if (life_shake_ > 0.0f) {
    const Layout& l = L();
    const float k = life_shake_ / l.shake_seconds;
    const float a = l.shake_amplitude * k * k;
    dx = a * std::sin(life_shake_time_ * l.shake_frequency_x);
    dy = a * l.shake_ratio_y * std::sin(life_shake_time_ * l.shake_frequency_y + l.shake_phase_y);
  }
  const SDL_FRect panel{width - L().margin - L().panel_w + dx, L().margin + dy, L().panel_w,
                        L().panel_h};
  Panel(r, panel);

  const float left = panel.x + L().padding;
  const float right = panel.x + panel.w - L().padding;
  float y = panel.y + (panel.h - (2 * L().line_h + L().line_gap)) / 2.0f;
  const SDL_Color label = colors::Get("silver");
  const SDL_Color value = colors::Get("silverLight");
  const SDL_Color zero = colors::Get("red");
  const SDL_Color hit = colors::Get("red");
  auto blink_color = [&](float flash, SDL_Color base) {
    // Blink: half of each period red, half normal, while the flash lasts.
    const float blink = L().flash_blink;
    return flash > 0.0f && std::fmod(flash, blink) > blink / 2.0f ? hit : base;
  };
  auto player_color = [&](int life) {
    if (life <= L().life_critical) return colors::Get("crimson");
    if (life <= L().life_danger) return colors::Get("red");
    if (life <= L().life_warning) return colors::Get("yellow");
    return colors::Get("green");
  };
  text::Draw(r, i18n::Get("general.opponent").c_str(), left, y, L().line_style, label);
  DrawRight(r, LifeText(opponent_life_).c_str(), right, y, L().line_style,
            opponent_life_ <= 0 ? zero : blink_color(life_flash_[0], value));
  y += L().line_h + L().line_gap;
  text::Draw(r, i18n::Get("general.player").c_str(), left, y, L().line_style, label);
  DrawRight(r, LifeText(player_life_).c_str(), right, y, L().line_style,
            blink_color(life_flash_[1], player_color(player_life_)));
}

void Hud::RenderDamagePopups(SDL_Renderer* r, int width) const {
  if (popups_.empty()) return;
  const float panel_x = width - L().margin - L().panel_w;
  const float right = panel_x + L().panel_w - L().padding;
  const float top = L().margin + (L().panel_h - (2 * L().line_h + L().line_gap)) / 2.0f;
  const SDL_Color red = colors::Get("red");
  const SDL_Color shadow = colors::Get("stoneBlack");
  const float shadow_alpha = opacity::Get("damagePopupShadow");
  for (const DamagePopup& popup : popups_) {
    const float t = popup.time;
    const Layout& l = L();
    const float row_y = top + (popup.player ? l.line_h + l.line_gap : 0.0f);
    const float x = right - l.popup_offset_x + popup.drift * l.popup_drift * t;
    const float y = row_y - l.popup_jump * t + 0.5f * l.popup_gravity * t * t;
    const float pop = t < l.popup_pop_seconds
                          ? 1.0f + l.popup_pop_grow * std::sin(t / l.popup_pop_seconds * 3.14159f)
                          : 1.0f;
    const float fade = (t - (l.popup_seconds - l.popup_fade)) / l.popup_fade;
    const float alpha = 1.0f - std::clamp(fade, 0.0f, 1.0f);
    const std::string label = "-" + std::to_string(popup.amount);
    text::Style style = L().popup_style;
    style.size *= pop;
    const float w = text::Width(label.c_str(), style);
    SDL_Color s = shadow, c = red;
    s.a = static_cast<Uint8>(255.0f * shadow_alpha * alpha);
    c.a = static_cast<Uint8>(255.0f * alpha);
    const float off = l.popup_shadow_offset;
    text::Draw(r, label.c_str(), x - w / 2.0f + off, y + off, style, s);
    text::Draw(r, label.c_str(), x - w / 2.0f, y, style, c);
  }
}


void Hud::RenderPilesPanel(SDL_Renderer* r, int width, int height) const {
  const std::string deck_title = i18n::Get("hud.deck");
  const std::string deck_value =
      std::to_string(deck_remaining_) + "/" + std::to_string(deck_used_ + deck_remaining_);
  const std::string grave_title = i18n::Get("hud.graveyard");
  const std::string grave_value = std::to_string(graveyard_);
  // Width measured with the widest values so the panel does not resize on every draw.
  const std::string deck_size = std::to_string(deck_used_ + deck_remaining_);
  const std::string widest_deck = deck_size + "/" + deck_size;
  const std::string widest_grave = graveyard_ > 99 ? grave_value : std::string("99");
  const float deck_title_w = text::Width(deck_title.c_str(), L().line_style);
  const float panel_w =
      ui::SnapUp(2 * L().piles_padding + deck_title_w + L().counter_gap +
                 text::Width(widest_deck.c_str(), L().line_style) + L().piles_gap +
                 text::Width(grave_title.c_str(), L().line_style) + L().counter_gap +
                 text::Width(widest_grave.c_str(), L().line_style));
  const SDL_FRect panel{width - L().margin - panel_w, height - L().margin - L().counter_h, panel_w,
                        L().counter_h};
  Panel(r, panel);
  const float y = panel.y + (panel.h - L().line_h) / 2.0f;
  const float left = panel.x + L().piles_padding;
  const float right = panel.x + panel.w - L().piles_padding;
  text::Draw(r, deck_title.c_str(), left, y, L().line_style, colors::Get("silverLight"));
  text::Draw(r, deck_value.c_str(), left + deck_title_w + L().counter_gap, y, L().line_style,
             deck_remaining_ > 0 ? colors::Get("silver")
                                 : colors::Get("red"));
  DrawRight(r, grave_value.c_str(), right, y, L().line_style, colors::Get("silver"));
  DrawRight(r, grave_title.c_str(),
            right - text::Width(grave_value.c_str(), L().line_style) - L().counter_gap, y,
            L().line_style, colors::Get("silverLight"));
}

// Window with the result title, time and rounds, then one row per stat: label on the left,
// your number and the opponent's in two right-aligned columns.
// Stat rows listed in RenderResult.
constexpr int kResultRows = 8;

// Title, time + rounds line, column header, stat rows, Menu button; after a win the window and
// the card won (label above it) are centered together.
SDL_FRect Hud::ResultPanel(int width, int height) const {
  const Layout& l = L();
  const float row_h = l.line_h + l.result_row_gap;
  const float h = 2 * l.result_padding + text::LineHeight(l.result_style) + l.result_title_gap +
                  row_h * (2 + kResultRows) + l.result_title_gap + l.result_button_h;
  const float card_w =
      reward_card_ != nullptr ? (h - l.line_h - l.result_title_gap) * kCardAspect : 0.0f;
  const float group_w = l.result_w + (card_w > 0.0f ? l.result_card_gap + card_w : 0.0f);
  return SDL_FRect{(width - group_w) / 2.0f, (height - h) / 2.0f, l.result_w, h};
}

SDL_FRect Hud::ResultButtonArea(int width, int height) const {
  const SDL_FRect panel = ResultPanel(width, height);
  const float pad = L().result_padding;
  return SDL_FRect{panel.x + pad, panel.y + panel.h - pad - L().result_button_h,
                   panel.w - 2 * pad, L().result_button_h};
}

void Hud::RenderResult(SDL_Renderer* r, int width, int height) const {
  FillRect(r, SDL_FRect{0, 0, static_cast<float>(width), static_cast<float>(height)},
           opacity::Apply(colors::Get("stoneBlack"), "overlay"));
  const Layout& l = L();
  const MatchStats& s = stats_;
  struct Row {
    std::string label;
    std::string you;
    std::string opponent;
  };
  auto pair = [](const int v[2]) {
    return std::make_pair(std::to_string(v[0]), std::to_string(v[1]));
  };
  std::vector<Row> rows;
  for (const auto& [key, v] : {std::pair{"hud.stats.drawn", s.drawn},
                               std::pair{"hud.stats.played", s.played},
                               std::pair{"hud.stats.graveyard", s.graveyard},
                               std::pair{"hud.stats.attacks", s.attacks},
                               std::pair{"hud.stats.direct", s.direct_attacks},
                               std::pair{"hud.stats.destroyed", s.destroyed},
                               std::pair{"hud.stats.damage", s.damage},
                               std::pair{"hud.stats.biggest", s.biggest_hit}}) {
    const auto [you, opponent] = pair(v);
    rows.push_back({i18n::Get(key), you, opponent});
  }
  const float title_h = text::LineHeight(l.result_style);
  const float row_h = l.line_h + l.result_row_gap;
  const SDL_FRect panel = ResultPanel(width, height);
  const float card_h = panel.h - l.line_h - l.result_title_gap;
  const float card_w = card_h * kCardAspect;
  frame::Draw(r, panel, frame::Kind::kWindow);

  const float cx = panel.x + panel.w / 2.0f;
  const float left = panel.x + l.result_padding;
  const float right = panel.x + panel.w - l.result_padding;
  const float you_right = right - l.result_column_w;
  // Both at 0 cannot happen by the rules; if it does, it counts as a defeat.
  const bool won = opponent_life_ <= 0 && player_life_ > 0;
  float y = panel.y + l.result_padding;
  text::DrawCentered(r, i18n::Get(won ? "hud.victory" : "hud.defeat").c_str(), cx,
                     y + title_h / 2.0f, l.result_style,
                     colors::Get(won ? "green" : "red"));
  y += title_h + l.result_title_gap;

  const SDL_Color label = colors::Get("steelLight");
  const SDL_Color value = colors::Get("silver");
  const SDL_Color header = colors::Get("silverLight");
  const std::string summary = i18n::Get("hud.stats.time") + " " + ClockText(s.seconds) + "   " +
                              i18n::Get("hud.stats.rounds") + " " + std::to_string(s.rounds);
  text::DrawCentered(r, summary.c_str(), cx, y + l.line_h / 2.0f, l.body_style, label);
  y += row_h;
  DrawRight(r, i18n::Get("general.you").c_str(), you_right, y, l.line_style, header);
  DrawRight(r, i18n::Get("general.opponent").c_str(), right, y, l.line_style, header);
  y += row_h;
  for (const Row& row : rows) {
    text::Draw(r, row.label.c_str(), left, y, l.body_style, label);
    DrawRight(r, row.you.c_str(), you_right, y, l.line_style, value);
    DrawRight(r, row.opponent.c_str(), right, y, l.line_style, value);
    y += row_h;
  }
  if (reward_card_ == nullptr) return;
  const float card_x = panel.x + panel.w + l.result_card_gap;
  text::DrawCentered(r, reward_label_.c_str(), card_x + card_w / 2.0f, panel.y + l.line_h / 2.0f,
                     l.line_style, colors::Get("silverLight"));
  const SDL_FRect card{card_x, panel.y + l.line_h + l.result_title_gap, card_w, card_h};
  SDL_SetTextureAlphaMod(reward_card_, 255);
  SDL_RenderTexture(r, reward_card_, nullptr, &card);
}
