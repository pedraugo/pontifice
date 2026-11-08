#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "scripts/button/button.h"

// Tutorial guide window. Modal: centered over a veil, closed by its button or Enter/Space.
// Otherwise docked at the left edge, without a button, while the player acts.
class TutorialPanel {
 public:
  TutorialPanel();

  // Takes translated texts; the body wraps to the panel width. A modal panel leaves
  // `spotlight` (if not empty) out of the veil and outlines it in pulsing gold.
  void Show(const std::string& counter, const std::string& title, const std::string& body,
            const std::string& button_label, bool modal, int width, int height,
            SDL_FRect spotlight = {});
  void SetSpotlight(SDL_FRect spotlight, int width, int height);
  void Hide() { open_ = false; }
  bool Open() const { return open_; }
  bool Modal() const { return open_ && modal_; }

  bool HandleEvent(const SDL_Event& event);  // true when a modal panel is confirmed
  void Render(SDL_Renderer* r, int width, int height) const;

 private:
  std::string counter_;
  std::string title_;
  std::vector<std::string> lines_;
  Button button_;
  SDL_FRect panel_{};
  SDL_FRect spotlight_{};
  bool modal_ = false;
  bool open_ = false;
};
