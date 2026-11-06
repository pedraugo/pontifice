#pragma once

#include <SDL3/SDL.h>

#include <functional>
#include <string>

struct SceneContext;

// Measures from assets/button.json, in 16 px grid cells (Cells) or plain numbers (Number).
namespace button {
float Cells(const std::string& path, float fallback_cells);
float Number(const std::string& path, float fallback);
}

class Button {
 public:
  using Action = std::function<void(SceneContext&)>;
  explicit Button(std::string label, Action action = nullptr);
  virtual ~Button() = default;

  void SetRect(const SDL_FRect& rect) { rect_ = rect; }
  void SetLabel(std::string label) { label_ = std::move(label); }
  void SetFocused(bool focused) { focused_ = focused; }
  void SetDanger(bool danger) { danger_ = danger; }  // red plate for destructive actions
  float LabelWidth() const;

  bool HandleEvent(const SDL_Event& event);  // true on press and release inside
  virtual void Render(SDL_Renderer* r) const;

  virtual void OnClick(SceneContext& ctx);  // scene calls it after a click or by keyboard

  bool Contains(float x, float y) const;
  const SDL_FRect& Rect() const { return rect_; }

 protected:
  virtual void RenderContent(SDL_Renderer* r) const;
  const std::string& Label() const { return label_; }
  bool Highlighted() const { return hovered_ || focused_ || pressed_; }
  static float LabelScale();

 private:
  std::string label_;
  Action action_;
  SDL_FRect rect_{};
  bool hovered_ = false;
  bool pressed_ = false;
  bool focused_ = false;
  bool danger_ = false;
};
