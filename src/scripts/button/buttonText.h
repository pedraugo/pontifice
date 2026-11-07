#pragma once

#include <functional>
#include <optional>
#include <string>

#include "scripts/button/button.h"
#include "scripts/flag/flag.h"

class ButtonText : public Button {
 public:
  ButtonText(std::string label, Action action);

  // Language item: flag and label left-aligned; the flag is grayscale until hover/focus.
  void SetFlag(flag::Country country) { flag_ = country; }
  float ContentWidth() const;  // flag + gap + label, or just the label

  void Render(SDL_Renderer* r) const override;

 protected:
  std::string VariantKey() const override;  // "buttonText.flag", ".danger" or ".regular"

 private:
  float FlagHeight() const { return Measure("flagHeight", 2.5f); }
  float FlagWidth() const { return FlagHeight() * 1.5f; }  // 3:2 aspect
  float FlagGap() const { return Measure("gap", 1.0f); }

  std::optional<flag::Country> flag_;
};
