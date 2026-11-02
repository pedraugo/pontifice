#pragma once

#include <SDL3/SDL.h>

// Botão quadrado com ícone de alto-falante (desenhado com primitivas do renderer, sem
// sprite) que liga e desliga a música. Com a música desligada o alto-falante aparece sem
// ondas, com um X e esmaecido. Mesmos estados visuais dos outros botões (hover, pressionado).
class SoundButton {
 public:
  void SetRect(const SDL_FRect& rect) { rect_ = rect; }
  const SDL_FRect& Rect() const { return rect_; }
  bool Contains(float x, float y) const;

  // true num clique completo (pressionar e soltar dentro).
  bool HandleEvent(const SDL_Event& event);
  void Render(SDL_Renderer* r, bool muted) const;

 private:
  SDL_FRect rect_{};
  bool hovered_ = false;
  bool pressed_ = false;
};
