#include "scripts/button/buttonText.h"

#include <utility>

#include "scripts/colors/colors.h"
#include "scripts/text/text.h"

ButtonText::ButtonText(std::string label, Action action)
    : Button(std::move(label), std::move(action)) {}

std::string ButtonText::VariantKey() const {
  if (flag_) return "buttonText.flag";
  return Danger() ? "buttonText.danger" : "buttonText.regular";
}

float ButtonText::ContentWidth() const {
  return (flag_ ? FlagWidth() + FlagGap() : 0.0f) + LabelWidth();
}

void ButtonText::Render(SDL_Renderer* r) const {
  const SDL_FRect& rect = Rect();
  const float cy = rect.y + rect.h / 2.0f;
  const bool highlight = Highlighted();
  const SDL_Color ink = colors::Get(Danger() ? (highlight ? "redLight"
                                                         : "red")
                                             : (highlight ? "silverLight"
                                                          : "steelLight"));
  if (!flag_) {
    text::DrawCentered(r, Label().c_str(), rect.x + rect.w / 2.0f, cy, LabelStyle(), ink);
    return;
  }
  const float flag_h = FlagHeight();
  const float flag_w = FlagWidth();
  flag::Draw(r, *flag_, SDL_FRect{rect.x, cy - flag_h / 2.0f, flag_w, flag_h},
             highlight ? 1.0f : 0.0f);
  text::DrawCentered(r, Label().c_str(), rect.x + flag_w + FlagGap() + LabelWidth() / 2.0f, cy,
                     LabelStyle(), ink);
}
