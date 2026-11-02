#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "scripts/ui/Ui.h"

struct SceneContext;

// Botão retangular com texto. Subclasses definem a ação em OnClick.
// Estados e cores seguem docs/menu.md.
class Button {
 public:
  explicit Button(std::string label);
  virtual ~Button() = default;

  void SetRect(const SDL_FRect& rect) { rect_ = rect; }
  void SetLabel(std::string label) { label_ = std::move(label); }
  void SetFocused(bool focused) { focused_ = focused; }

  // Retorna true quando um clique completo (pressionar e soltar dentro) acontece.
  bool HandleEvent(const SDL_Event& event);
  void Render(SDL_Renderer* r) const;

  virtual void OnClick(SceneContext& ctx) = 0;

  bool Contains(float x, float y) const;
  const SDL_FRect& Rect() const { return rect_; }

 protected:
  // Conteúdo sobre a placa: por padrão o rótulo centralizado. Subclasses podem desenhar
  // algo a mais (ex.: bandeira ao lado do texto, scripts/button/ButtonLanguage).
  virtual void RenderContent(SDL_Renderer* r) const;
  const std::string& Label() const { return label_; }
  static constexpr float LabelScale() { return 3.0f * ui::kScale; }

 private:
  std::string label_;
  SDL_FRect rect_{};
  bool hovered_ = false;
  bool pressed_ = false;
  bool focused_ = false;
};
