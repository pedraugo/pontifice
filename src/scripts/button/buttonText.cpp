#include "scripts/button/buttonText.h"

#include <utility>

#include "scripts/colors/colors.h"
#include "scripts/text/text.h"

namespace {

struct FlagLayout {
  float height = 0.0f;
  float width = 0.0f;  // 3:2 aspect
  float gap = 0.0f;
};

const FlagLayout& Flag() {
  static const FlagLayout layout = [] {
    FlagLayout f;
    f.height = button::Cells("flag.height", 2.5f);
    f.width = f.height * 1.5f;
    f.gap = button::Cells("flag.gap", 1.0f);
    return f;
  }();
  return layout;
}

}

ButtonText::ButtonText(std::string label, Action action)
    : Button(std::move(label), std::move(action)) {}

float ButtonText::ContentWidth() const {
  return (flag_ ? Flag().width + Flag().gap : 0.0f) + LabelWidth();
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
  const FlagLayout& flag = Flag();
  flag::Draw(r, *flag_, SDL_FRect{rect.x, cy - flag.height / 2.0f, flag.width, flag.height},
             highlight ? 1.0f : 0.0f);
  text::DrawCentered(r, Label().c_str(), rect.x + flag.width + flag.gap + LabelWidth() / 2.0f,
                     cy, LabelScale(), ink);
}
