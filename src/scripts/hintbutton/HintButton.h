#pragma once

#include <SDL3/SDL.h>

// Botão quadrado com um "?" que liga e desliga as dicas do topo da batalha. Com as dicas
// desligadas o "?" aparece esmaecido e riscado. Mesmos estados visuais do botão de som
// (hover, pressionado).
class HintButton {
 public:
  void SetRect(const SDL_FRect& rect) { rect_ = rect; }
  const SDL_FRect& Rect() const { return rect_; }
  bool Contains(float x, float y) const;

  // true num clique completo (pressionar e soltar dentro).
  bool HandleEvent(const SDL_Event& event);
  void Render(SDL_Renderer* r, bool enabled) const;

 private:
  SDL_FRect rect_{};
  bool hovered_ = false;
  bool pressed_ = false;
};
