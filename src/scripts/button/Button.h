#pragma once

#include <SDL3/SDL.h>

#include <string>

struct SceneContext;

// Botão retangular com texto. Subclasses definem a ação em OnClick.
// Estados e cores seguem docs/menu.md.
class Button {
 public:
  explicit Button(std::string label);
  virtual ~Button() = default;

  void SetRect(const SDL_FRect& rect) { rect_ = rect; }
  void SetFocused(bool focused) { focused_ = focused; }

  // Retorna true quando um clique completo (pressionar e soltar dentro) acontece.
  bool HandleEvent(const SDL_Event& event);
  void Render(SDL_Renderer* r) const;

  virtual void OnClick(SceneContext& ctx) = 0;

 private:
  bool Contains(float x, float y) const;

  std::string label_;
  SDL_FRect rect_{};
  bool hovered_ = false;
  bool pressed_ = false;
  bool focused_ = false;
};
