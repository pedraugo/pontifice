#include "scripts/button/ButtonText.h"

#include <utility>

#include "scripts/colors/Colors.h"
#include "scripts/text/Text.h"

ButtonText::ButtonText(std::string label, Action action)
    : Button(std::move(label)), action_(std::move(action)) {}

void ButtonText::OnClick(SceneContext& ctx) {
  if (action_) action_(ctx);
}

void ButtonText::Render(SDL_Renderer* r) const {
  const SDL_FRect& rect = Rect();
  text::DrawCentered(r, Label().c_str(), rect.x + rect.w / 2.0f, rect.y + rect.h / 2.0f,
                     LabelScale(),
                     colors::Get(Highlighted() ? "buttonText.highlight" : "buttonText.text"));
}
