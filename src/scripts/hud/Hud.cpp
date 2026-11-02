#include "scripts/hud/Hud.h"

#include <algorithm>

#include "scripts/colors/Colors.h"
#include "scripts/frame/Frame.h"
#include "scripts/i18n/I18n.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

// Cores vêm de assets/colors.json, pelos papéis "panel.*", "hud.*", "status.*" e "text.*".
namespace {

// Layout na grade de 16 px (ui::Grid): margens de 1 célula, painéis com tamanhos inteiros.
constexpr float kMargin = ui::Grid(1);
constexpr float kPadding = ui::Grid(1);
constexpr float kPanelWidth = ui::Grid(34);         // painel de vida (544 px)
constexpr float kPanelHeight = ui::Grid(6);         // 96 px: 2 linhas de 24 px + folgas
constexpr float kFieldPanelWidth = ui::Grid(24);    // carta de campo (384 px, mínimo)
constexpr float kFieldPanelHeight = ui::Grid(6);
constexpr float kCounterHeight = ui::Grid(3);       // Monte / Cemitério: 48 px
constexpr float kLineScale = 2.0f * ui::kScale;      // 16 px em 720p
constexpr float kDetailScale = 1.5f * ui::kScale;    // 12 px em 720p
constexpr float kResultScale = 6.0f * ui::kScale;
constexpr float kLineHeight = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * kLineScale;
constexpr float kDetailHeight = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * kDetailScale;
constexpr float kLineGap = ui::Px(8.0f);
// Proporção da frente da carta (sprites 1600x2230) e da ilustração (janela 668x448).
constexpr float kCardAspect = 1600.0f / 2230.0f;
constexpr float kArtAspect = 668.0f / 448.0f;

void FillRect(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(r, &rect);
}

void StrokeRect(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderRect(r, &rect);
}

void Panel(SDL_Renderer* r, const SDL_FRect& rect) {
  if (frame::Ready()) {
    frame::Draw(r, rect, frame::Kind::kWindow);
    return;
  }
  FillRect(r, rect, colors::Get("panel.fill"));
  StrokeRect(r, rect, colors::Get("panel.border"));
}

std::string CategoryName(Category c) {
  return i18n::Get(c == Category::kGood ? "category.good.plural" : "category.evil.plural");
}
Category Other(Category c) { return c == Category::kGood ? Category::kEvil : Category::kGood; }

// Texto alinhado à direita: a borda direita fica em `right_x`.
void DrawRight(SDL_Renderer* r, const char* str, float right_x, float y, float scale,
               SDL_Color color) {
  text::Draw(r, str, right_x - text::Width(str, scale), y, scale, color);
}

std::string LifeText(int life, int max) {
  return std::to_string(std::max(life, 0)) + " / " + std::to_string(max);
}

}  // namespace

void Hud::SetLife(int opponent, int player) {
  opponent_life_ = std::clamp(opponent, 0, kMaxLife);
  player_life_ = std::clamp(player, 0, kMaxLife);
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

void Hud::Render(SDL_Renderer* r, int width, int height) const {
  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &previous);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  RenderFieldPanel(r);
  RenderLifePanel(r, width);
  RenderDeckPanel(r, height);
  RenderGraveyardPanel(r, width, height);
  if (Over()) RenderResult(r, width, height);

  SDL_SetRenderDrawBlendMode(r, previous);
}

void Hud::RenderFieldPanel(SDL_Renderer* r) const {
  // Compacto: miniatura à esquerda (só a ilustração da carta; sem ela, a carta inteira);
  // nome e, em uma linha cada, buff e nerf à direita. A largura cresce com o texto.
  const float thumb_h = kFieldPanelHeight - 2 * kPadding;
  const bool art = has_field_ && field_.art != nullptr;
  const float thumb_w = thumb_h * (art ? kArtAspect : kCardAspect);
  std::string buff, nerf;
  float width = kFieldPanelWidth;
  if (has_field_) {
    buff = std::string("+") + std::to_string(field_.buff) + " " + CategoryName(field_.favored);
    nerf = std::string("-") + std::to_string(field_.nerf) + " " +
           CategoryName(Other(field_.favored));
    const float text_w = std::max({text::Width(field_.name.c_str(), kLineScale),
                                   text::Width(buff.c_str(), kDetailScale),
                                   text::Width(nerf.c_str(), kDetailScale)});
    width = std::max(width, ui::SnapUp(3 * kPadding + thumb_w + text_w));
  }
  const SDL_FRect panel{kMargin, kMargin, width, kFieldPanelHeight};
  Panel(r, panel);

  if (!has_field_) {
    text::DrawCentered(r, i18n::Get("hud.noField").c_str(), panel.x + panel.w / 2.0f, panel.y + panel.h / 2.0f,
                       kLineScale, colors::Get("text.muted"));
    return;
  }

  const SDL_FRect card{panel.x + kPadding, panel.y + kPadding, thumb_w, thumb_h};
  SDL_Texture* thumb = art ? field_.art : field_.image;
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
  const SDL_FRect panel{width - kMargin - kPanelWidth, kMargin, kPanelWidth, kPanelHeight};
  Panel(r, panel);

  const float left = panel.x + kPadding;
  const float right = panel.x + panel.w - kPadding;
  // Duas linhas centralizadas na altura do painel.
  float y = panel.y + (panel.h - (2 * kLineHeight + kLineGap)) / 2.0f;
  const SDL_Color label = colors::Get("hud.lifeLabel");
  const SDL_Color value = colors::Get("hud.lifeValue");
  const SDL_Color zero = colors::Get("hud.lifeZero");
  text::Draw(r, i18n::Get("hud.opponent").c_str(), left, y, kLineScale, label);
  DrawRight(r, LifeText(opponent_life_, kMaxLife).c_str(), right, y, kLineScale,
            opponent_life_ > 0 ? value : zero);
  y += kLineHeight + kLineGap;
  text::Draw(r, i18n::Get("hud.player").c_str(), left, y, kLineScale, label);
  DrawRight(r, LifeText(player_life_, kMaxLife).c_str(), right, y, kLineScale,
            player_life_ > 0 ? value : zero);
}

// Painel de contador em uma linha: "[Título valor]", largura pelo texto arredondada à grade.
namespace {

constexpr float kCounterGap = ui::Grid(1);  // entre título e valor

float CounterPanelWidth(const char* title, const std::string& value) {
  return ui::SnapUp(2 * kPadding + text::Width(title, kLineScale) + kCounterGap +
                    text::Width(value.c_str(), kLineScale));
}

void CounterPanel(SDL_Renderer* r, const SDL_FRect& panel, const char* title,
                  const std::string& value, SDL_Color value_color) {
  Panel(r, panel);
  const float y = panel.y + (panel.h - kLineHeight) / 2.0f;
  text::Draw(r, title, panel.x + kPadding, y, kLineScale, colors::Get("panel.title"));
  DrawRight(r, value.c_str(), panel.x + panel.w - kPadding, y, kLineScale, value_color);
}

}  // namespace

void Hud::RenderDeckPanel(SDL_Renderer* r, int height) const {
  // "Monte restantes/total"; monte vazio em destaque: o jogador não compra mais.
  const std::string value = std::to_string(deck_remaining_) + "/" + std::to_string(kDeckSize);
  const float panel_h = kCounterHeight;
  const float left = deck_panel_left_ >= 0.0f ? deck_panel_left_ : kMargin;
  const std::string title = i18n::Get("hud.deck");
  const SDL_FRect panel{left, height - kMargin - panel_h,
                        CounterPanelWidth(title.c_str(), value), panel_h};
  CounterPanel(r, panel, title.c_str(), value,
               deck_remaining_ > 0 ? colors::Get("hud.counterValue")
                                   : colors::Get("hud.deckEmpty"));
}

void Hud::RenderGraveyardPanel(SDL_Renderer* r, int width, int height) const {
  const std::string value = std::to_string(graveyard_);
  const float panel_h = kCounterHeight;
  const std::string title = i18n::Get("hud.graveyard");
  const float panel_w = CounterPanelWidth(title.c_str(), value);
  const SDL_FRect panel{width - kMargin - panel_w, height - kMargin - panel_h, panel_w, panel_h};
  CounterPanel(r, panel, title.c_str(), value, colors::Get("hud.counterValue"));
}

void Hud::RenderResult(SDL_Renderer* r, int width, int height) const {
  FillRect(r, SDL_FRect{0, 0, static_cast<float>(width), static_cast<float>(height)},
           colors::Get("background.overlay"));
  const float cx = width / 2.0f;
  const float cy = height / 2.0f;
  // Os dois em 0 ao mesmo tempo não acontece pelas regras; se acontecer, conta como derrota.
  const bool won = opponent_life_ <= 0 && player_life_ > 0;
  text::DrawCentered(r, i18n::Get(won ? "hud.victory" : "hud.defeat").c_str(), cx,
                     cy - ui::Grid(2), kResultScale,
                     colors::Get(won ? "status.victory" : "status.defeat"));
  text::DrawCentered(r, i18n::Get("hud.escMenu").c_str(), cx, cy + ui::Grid(4), kLineScale,
                     colors::Get("text.primary"));
}
