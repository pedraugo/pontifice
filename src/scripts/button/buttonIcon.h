#pragma once

#include <string>
#include <utility>

#include "scripts/button/button.h"
#include "scripts/flag/flag.h"

// Square plated button; the icon is drawn with renderer primitives, no sprite.
class ButtonIcon : public Button {
 public:
  enum class Icon { kSound, kHint, kExit, kFlag, kLevel };

  explicit ButtonIcon(Icon icon);

  // Inactive: dimmed icon with a red strike (X beside the speaker, slash over the "?").
  void SetActive(bool active) { active_ = active; }
  void SetFlag(flag::Country country) { flag_ = country; }  // kFlag: the flag fills the square
  void SetLevel(int level) { level_ = level; }  // kLevel: 0..3, rising bars filled up to it
  // Label beside the button while hovered; RenderTooltip goes after the rest of the HUD.
  void SetTooltip(std::string tooltip) { tooltip_ = std::move(tooltip); }
  void RenderTooltip(SDL_Renderer* r) const;
  float Side() const { return Measure("size", 3.0f); }
  float Margin() const { return Measure("margin", 1.0f); }

 protected:
  void RenderContent(SDL_Renderer* r) const override;
  std::string VariantKey() const override;  // "buttonIcon.danger" or "buttonIcon.<size>"

 private:
  void RenderSound(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const;
  void RenderHint(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const;
  void RenderExit(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const;
  void RenderLevel(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const;

  Icon icon_;
  bool active_ = true;
  flag::Country flag_ = flag::Country::kBrazil;
  int level_ = 0;
  std::string tooltip_;
};
