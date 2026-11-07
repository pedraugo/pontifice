#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <utility>

#include "scripts/text/text.h"

// Single-line text field on a window plate; the caret always sits at the end of the text.
class TextInput {
 public:
  void SetRect(const SDL_FRect& rect) { rect_ = rect; }
  const SDL_FRect& Rect() const { return rect_; }
  void SetPlaceholder(std::string placeholder) { placeholder_ = std::move(placeholder); }
  void SetMaxLength(int codepoints) { max_length_ = codepoints; }
  void SetStyle(const text::Style& style) { style_ = style; }
  void SetPadding(float padding) { padding_ = padding; }
  // Codepoints the field accepts; by default whatever the fonts draw (ASCII and Latin-1).
  using Filter = bool (*)(Uint32 codepoint);
  void SetFilter(Filter filter) { filter_ = filter; }

  const std::string& Text() const { return text_; }
  void Clear();

  // Typed text (SDL_EVENT_TEXT_INPUT) and Backspace; true when the text changed.
  bool HandleEvent(const SDL_Event& event);
  void Update(float dt_seconds);  // caret blink
  void Render(SDL_Renderer* r) const;

 private:
  int Length() const;

  SDL_FRect rect_{};
  std::string text_;
  std::string placeholder_;
  text::Style style_;
  float padding_ = 16.0f;
  int max_length_ = 16;
  float blink_ = 0.0f;
  Filter filter_ = nullptr;
};
