#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "scripts/button/button.h"

// Modal: while open the scene routes every event here and processes nothing else.
class ConfirmDialog {
 public:
  enum class Result { kNone, kNo, kYes };

  ConfirmDialog();

  // Takes translated labels; yes_danger gives Yes the red danger plate.
  void Open(const std::string& question, const std::string& no_label,
            const std::string& yes_label, int width, int height, bool yes_danger = false);
  void Close() { open_ = false; }

  Result HandleEvent(const SDL_Event& event);  // never closes by itself; the scene decides
  void Render(SDL_Renderer* r, int width, int height) const;

 private:
  void SetFocus(int index);

  std::string question_;
  Button no_;
  Button yes_;
  SDL_FRect panel_{};
  int focused_ = 0;  // 0 = No, 1 = Yes
  bool open_ = false;
};
