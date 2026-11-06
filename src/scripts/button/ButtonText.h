#pragma once

#include <functional>
#include <optional>
#include <string>

#include "scripts/button/Button.h"
#include "scripts/flag/Flag.h"

class ButtonText : public Button {
 public:
  ButtonText(std::string label, Action action);

  // Language item: flag and label left-aligned; the flag is grayscale until hover/focus.
  void SetFlag(flag::Country country) { flag_ = country; }
  float ContentWidth() const;  // flag + gap + label, or just the label

  void Render(SDL_Renderer* r) const override;

 private:
  std::optional<flag::Country> flag_;
};
