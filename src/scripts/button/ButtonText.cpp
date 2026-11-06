#include "scripts/button/ButtonText.h"

#include <utility>

#include "scripts/colors/Colors.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

constexpr float kFlagHeight = ui::Grid(2.5f);     // 40 px
constexpr float kFlagWidth = kFlagHeight * 1.5f;  // 3:2
constexpr float kFlagGap = ui::Grid(1);

}  // namespace

ButtonText::ButtonText(std::string label, Action action)
    : Button(std::move(label), std::move(action)) {}

float ButtonText::ContentWidth() const {
  return (flag_ ? kFlagWidth + kFlagGap : 0.0f) + LabelWidth();
}

void ButtonText::Render(SDL_Renderer* r) const {
  const SDL_FRect& rect = Rect();
  const float cy = rect.y + rect.h / 2.0f;
  const bool highlight = Highlighted();
  const SDL_Color ink = colors::Get(highlight ? "buttonText.highlight" : "buttonText.text");
  if (!flag_) {
    text::DrawCentered(r, Label().c_str(), rect.x + rect.w / 2.0f, cy, LabelScale(), ink);
    return;
  }
  // Bandeira em preto e branco; ganha cor com hover/foco, junto com o texto.
  flag::Draw(r, *flag_, SDL_FRect{rect.x, cy - kFlagHeight / 2.0f, kFlagWidth, kFlagHeight},
             highlight ? 1.0f : 0.0f);
  text::DrawCentered(r, Label().c_str(), rect.x + kFlagWidth + kFlagGap + LabelWidth() / 2.0f,
                     cy, LabelScale(), ink);
}
