#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "scripts/ui/Ui.h"

struct SceneContext;

// Botão retangular com placa (sprites/UiFrameButton*.png; sem elas, retângulo de cores) e
// texto. Subclasses definem a ação em OnClick. Para botão só de texto, sem placa, ver
// ButtonText. Estados e cores seguem docs/menu.md.
class Button {
 public:
  explicit Button(std::string label);
  virtual ~Button() = default;

  void SetRect(const SDL_FRect& rect) { rect_ = rect; }
  void SetLabel(std::string label) { label_ = std::move(label); }
  void SetFocused(bool focused) { focused_ = focused; }
  // Ação destrutiva (Sair, confirmar abandono): placa vermelha (frame::Kind::kButtonDanger;
  // sem as placas, cores "button.danger*" de colors.json).
  void SetDanger(bool danger) { danger_ = danger; }
  // Largura do rótulo na escala do botão (para dimensionar botões só de texto).
  float LabelWidth() const;

  // Retorna true quando um clique completo (pressionar e soltar dentro) acontece.
  bool HandleEvent(const SDL_Event& event);
  // Placa (ou retângulo de cores) e, por cima, RenderContent. Subclasses sem placa
  // (ButtonText) sobrescrevem e desenham só o conteúdo.
  virtual void Render(SDL_Renderer* r) const;

  virtual void OnClick(SceneContext& ctx) = 0;

  bool Contains(float x, float y) const;
  const SDL_FRect& Rect() const { return rect_; }

 protected:
  // Conteúdo sobre a placa: por padrão o rótulo centralizado. Subclasses podem desenhar
  // algo a mais.
  virtual void RenderContent(SDL_Renderer* r) const;
  const std::string& Label() const { return label_; }
  bool Hovered() const { return hovered_; }
  bool Pressed() const { return pressed_; }
  bool Focused() const { return focused_; }
  bool Highlighted() const { return hovered_ || focused_ || pressed_; }
  static constexpr float LabelScale() { return 3.0f * ui::kScale; }

 private:
  std::string label_;
  SDL_FRect rect_{};
  bool hovered_ = false;
  bool pressed_ = false;
  bool focused_ = false;
  bool danger_ = false;
};
