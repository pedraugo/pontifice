#pragma once

#include <string>

#include "scripts/button/button.h"

// Square plated button; the icon is drawn with renderer primitives, no sprite.
class ButtonIcon : public Button {
 public:
  enum class Icon { kSound, kHint, kExit };

  explicit ButtonIcon(Icon icon);

  // Inactive: dimmed icon with a red strike (X beside the speaker, slash over the "?").
  void SetActive(bool active) { active_ = active; }
  float Side() const { return Measure("size", 3.0f); }
  float Margin() const { return Measure("margin", 1.0f); }

 protected:
  void RenderContent(SDL_Renderer* r) const override;
  std::string VariantKey() const override;  // "buttonIcon.danger" or "buttonIcon.<size>"

 private:
  void RenderSound(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const;
  void RenderHint(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const;
  void RenderExit(SDL_Renderer* r, const SDL_FRect& rect, SDL_Color ink) const;

  Icon icon_;
  bool active_ = true;
};
