#include "scripts/button/ButtonAction.h"

#include <utility>

ButtonAction::ButtonAction(std::string label, Action action)
    : Button(std::move(label)), action_(std::move(action)) {}

void ButtonAction::OnClick(SceneContext& ctx) {
  if (action_) action_(ctx);
}
