#pragma once

#include "scripts/button/Button.h"

// Square plated button; the icon is drawn with renderer primitives, no sprite.
class ButtonIcon : public Button {
 public:
  enum class Icon { kSound, kHint };

  explicit ButtonIcon(Icon icon);

  // Inactive: dimmed icon with a red strike (X beside the speaker, slash over the "?").
  void SetActive(bool active) { active_ = active; }

 protected:
  void RenderContent(SDL_Renderer* r) const override;

 private:
  void RenderSound(SDL_Renderer* r, SDL_Color ink) const;
  void RenderHint(SDL_Renderer* r, SDL_Color ink) const;

  Icon icon_;
  bool active_ = true;
};
