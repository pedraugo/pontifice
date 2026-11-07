#pragma once

#include <SDL3/SDL.h>

#include <functional>
#include <string>

#include "scripts/text/text.h"

struct SceneContext;

// Measures from assets/button.json ("<category>.<variant>.<name>"), in 16 px grid cells
// (Cells) or plain numbers (Number).
namespace button {
float Cells(const std::string& path, float fallback_cells);
float Number(const std::string& path, float fallback);
}

class Button {
 public:
  using Action = std::function<void(SceneContext&)>;
  enum class Size { kSmall, kRegular, kLarge };
  explicit Button(std::string label, Action action = nullptr);
  virtual ~Button() = default;

  void SetRect(const SDL_FRect& rect) { rect_ = rect; }
  void SetLabel(std::string label) { label_ = std::move(label); }
  void SetFocused(bool focused) { focused_ = focused; }
  void SetDanger(bool danger) { danger_ = danger; }  // red plate for destructive actions
  bool Danger() const { return danger_; }
  void SetAlpha(float alpha) { alpha_ = alpha; }  // 0..1 opacity of plate and label (disabled look)
  void SetSize(Size size) { size_ = size; }
  Size GetSize() const { return size_; }
  float Alpha() const { return alpha_; }
  float LabelWidth() const;
  // Measures of this button's variant in button.json, in grid cells (px) or plain number.
  float Measure(const std::string& name, float fallback_cells) const;
  float MeasureNumber(const std::string& name, float fallback) const;
  float Height() const { return Measure("height", 3.5f); }
  float PreferredWidth() const;  // label plus padding, at least minWidth, snapped to the grid

  bool HandleEvent(const SDL_Event& event);  // true on press and release inside
  virtual void Render(SDL_Renderer* r) const;

  virtual void OnClick(SceneContext& ctx);  // scene calls it after a click or by keyboard

  bool Contains(float x, float y) const;
  const SDL_FRect& Rect() const { return rect_; }

 protected:
  virtual void RenderContent(SDL_Renderer* r) const;
  const std::string& Label() const { return label_; }
  bool Highlighted() const { return hovered_ || focused_ || pressed_; }
  text::Style LabelStyle() const;  // text.json variant named by the button variant's "label"
  const char* SizeName() const;  // "small", "regular", "large"
  virtual std::string VariantKey() const;  // "button.small", "button.dangerRegular"...

 private:
  std::string label_;
  Action action_;
  SDL_FRect rect_{};
  bool hovered_ = false;
  bool pressed_ = false;
  bool focused_ = false;
  bool danger_ = false;
  Size size_ = Size::kRegular;
  float alpha_ = 1.0f;
};
