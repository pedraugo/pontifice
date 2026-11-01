#include "scripts/hud/Hud.h"

#include <algorithm>

#include "scripts/colors/Colors.h"
#include "scripts/text/Text.h"

// Cores vêm de assets/colors.json, pelos papéis "panel.*", "hud.*", "status.*" e "text.*".
namespace {

constexpr float kMargin = 16.0f;
constexpr float kPadding = 10.0f;
constexpr float kPanelWidth = 360.0f;
constexpr float kFieldPanelHeight = 140.0f;
constexpr float kCounterPanelWidth = 200.0f;  // painéis inferiores, fora da mão em repouso
constexpr float kLineScale = 2.0f;      // 16 px
constexpr float kDetailScale = 1.5f;    // 12 px
constexpr float kResultScale = 6.0f;
constexpr float kLineHeight = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * kLineScale;
constexpr float kDetailHeight = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * kDetailScale;
constexpr float kLineGap = 8.0f;
// Proporção da frente da carta (sprites 1600x2230).
constexpr float kCardAspect = 1600.0f / 2230.0f;

void FillRect(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderFillRect(r, &rect);
}

void StrokeRect(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color c) {
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderRect(r, &rect);
}

void Panel(SDL_Renderer* r, const SDL_FRect& rect) {
  FillRect(r, rect, colors::Get("panel.fill"));
  StrokeRect(r, rect, colors::Get("panel.border"));
}

const char* CategoryName(Category c) { return c == Category::kGood ? "Bondosas" : "Malignas"; }
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
  const SDL_FRect panel{kMargin, kMargin, kPanelWidth, kFieldPanelHeight};
  Panel(r, panel);

  if (!has_field_) {
    text::DrawCentered(r, "Sem campo", panel.x + panel.w / 2.0f, panel.y + panel.h / 2.0f,
                       kLineScale, colors::Get("text.muted"));
    return;
  }

  // Carta virada para cima, à esquerda; texto à direita dela.
  const float card_h = panel.h - 2 * kPadding;
  const SDL_FRect card{panel.x + kPadding, panel.y + kPadding, card_h * kCardAspect, card_h};
  if (field_.image != nullptr) {
    SDL_RenderTexture(r, field_.image, nullptr, &card);
  } else {
    FillRect(r, card, colors::Get("border.subtle"));
  }
  StrokeRect(r, card, colors::Get("panel.border"));

  const float text_x = card.x + card.w + kPadding;
  float y = card.y;
  text::Draw(r, field_.name.c_str(), text_x, y, kLineScale, colors::Get("hud.fieldName"));
  y += kLineHeight + kLineGap;
  text::Draw(r, "Carta de campo", text_x, y, kDetailScale, colors::Get("hud.fieldLabel"));
  y += kDetailHeight + kLineGap * 2;

  const std::string buff =
      std::string("Buff  ") + CategoryName(field_.favored) + " +" + std::to_string(field_.buff);
  const std::string nerf = std::string("Nerf  ") + CategoryName(Other(field_.favored)) + " -" +
                           std::to_string(field_.nerf);
  text::Draw(r, buff.c_str(), text_x, y, kDetailScale, colors::Get("status.buff"));
  y += kDetailHeight + kLineGap;
  text::Draw(r, nerf.c_str(), text_x, y, kDetailScale, colors::Get("status.nerf"));
}

void Hud::RenderLifePanel(SDL_Renderer* r, int width) const {
  const float panel_h = 2 * kPadding + 2 * kLineHeight + kLineGap;
  const SDL_FRect panel{width - kMargin - kPanelWidth, kMargin, kPanelWidth, panel_h};
  Panel(r, panel);

  const float left = panel.x + kPadding;
  const float right = panel.x + panel.w - kPadding;
  float y = panel.y + kPadding;
  const SDL_Color label = colors::Get("hud.lifeLabel");
  const SDL_Color value = colors::Get("hud.lifeValue");
  const SDL_Color zero = colors::Get("hud.lifeZero");
  text::Draw(r, "OPONENTE", left, y, kLineScale, label);
  DrawRight(r, LifeText(opponent_life_, kMaxLife).c_str(), right, y, kLineScale,
            opponent_life_ > 0 ? value : zero);
  y += kLineHeight + kLineGap;
  text::Draw(r, "VOCÊ", left, y, kLineScale, label);
  DrawRight(r, LifeText(player_life_, kMaxLife).c_str(), right, y, kLineScale,
            player_life_ > 0 ? value : zero);
}

// Painel de contadores: título e linhas "rótulo ... valor".
namespace {

struct CounterLine {
  const char* label;
  int value;
  SDL_Color color;
};

float CounterPanelHeight(int lines) {
  return 2 * kPadding + kLineHeight + lines * (kDetailHeight + kLineGap);
}

void CounterPanel(SDL_Renderer* r, const SDL_FRect& panel, const char* title,
                  const CounterLine* lines, int count) {
  Panel(r, panel);
  const float left = panel.x + kPadding;
  const float right = panel.x + panel.w - kPadding;
  float y = panel.y + kPadding;
  text::Draw(r, title, left, y, kLineScale, colors::Get("panel.title"));
  y += kLineHeight + kLineGap;
  const SDL_Color label = colors::Get("hud.counterLabel");
  for (int i = 0; i < count; ++i) {
    text::Draw(r, lines[i].label, left, y, kDetailScale, label);
    DrawRight(r, std::to_string(lines[i].value).c_str(), right, y, kDetailScale,
              lines[i].color);
    y += kDetailHeight + kLineGap;
  }
}

}  // namespace

void Hud::RenderDeckPanel(SDL_Renderer* r, int height) const {
  const float panel_h = CounterPanelHeight(2);
  const SDL_FRect panel{kMargin, height - kMargin - panel_h, kCounterPanelWidth, panel_h};
  // Monte vazio: destaca "restantes", o jogador não compra mais.
  const SDL_Color value = colors::Get("hud.counterValue");
  const CounterLine lines[] = {
      {"usadas", deck_used_, value},
      {"restantes", deck_remaining_, deck_remaining_ > 0 ? value : colors::Get("hud.deckEmpty")},
  };
  CounterPanel(r, panel, "MONTE", lines, 2);
}

void Hud::RenderGraveyardPanel(SDL_Renderer* r, int width, int height) const {
  const float panel_h = CounterPanelHeight(1);
  const SDL_FRect panel{width - kMargin - kCounterPanelWidth, height - kMargin - panel_h,
                        kCounterPanelWidth, panel_h};
  const CounterLine lines[] = {{"cartas", graveyard_, colors::Get("hud.counterValue")}};
  CounterPanel(r, panel, "CEMITÉRIO", lines, 1);
}

void Hud::RenderResult(SDL_Renderer* r, int width, int height) const {
  FillRect(r, SDL_FRect{0, 0, static_cast<float>(width), static_cast<float>(height)},
           colors::Get("background.overlay"));
  const float cx = width / 2.0f;
  const float cy = height / 2.0f;
  // Os dois em 0 ao mesmo tempo não acontece pelas regras; se acontecer, conta como derrota.
  const bool won = opponent_life_ <= 0 && player_life_ > 0;
  text::DrawCentered(r, won ? "VITÓRIA" : "DERROTA", cx, cy - 20.0f, kResultScale,
                     colors::Get(won ? "status.victory" : "status.defeat"));
  text::DrawCentered(r, "Esc volta ao menu", cx, cy + 40.0f, kLineScale,
                     colors::Get("text.primary"));
}
