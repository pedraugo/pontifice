#include "scripts/input/textInput.h"

#include <cmath>

#include "scripts/colors/colors.h"
#include "scripts/frame/frame.h"

namespace {

constexpr float kCaretGap = 3.0f;

// The fonts cover ASCII and Latin-1 only.
bool Drawable(Uint32 ch) { return (ch >= 0x20 && ch < 0x7F) || (ch >= 0xA0 && ch <= 0xFF); }

}

void TextInput::Clear() {
  text_.clear();
  blink_ = 0.0f;
}

int TextInput::Length() const {
  int count = 0;
  const char* str = text_.c_str();
  while (SDL_StepUTF8(&str, nullptr) != 0) ++count;
  return count;
}

bool TextInput::HandleEvent(const SDL_Event& event) {
  if (event.type == SDL_EVENT_TEXT_INPUT) {
    bool changed = false;
    const char* str = event.text.text;
    int length = Length();
    while (true) {
      const char* start = str;
      const Uint32 ch = SDL_StepUTF8(&str, nullptr);
      if (ch == 0 || length >= max_length_) break;
      if (!(filter_ != nullptr ? filter_(ch) : Drawable(ch)) || (ch == ' ' && text_.empty())) {
        continue;
      }
      text_.append(start, static_cast<size_t>(str - start));
      ++length;
      changed = true;
    }
    if (changed) blink_ = 0.0f;
    return changed;
  }
  if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_BACKSPACE && !text_.empty()) {
    size_t end = text_.size() - 1;
    while (end > 0 && (static_cast<unsigned char>(text_[end]) & 0xC0) == 0x80) --end;
    text_.erase(end);
    blink_ = 0.0f;
    return true;
  }
  return false;
}

void TextInput::Update(float dt_seconds) { blink_ = std::fmod(blink_ + dt_seconds, 1.0f); }

void TextInput::Render(SDL_Renderer* r) const {
  frame::Draw(r, rect_, frame::Kind::kWindow, frame::State::kHighlight);
  const float x = rect_.x + padding_;
  const float cy = rect_.y + rect_.h / 2.0f;
  const float line = text::LineHeight(style_);
  const float top = cy - line / 2.0f;
  if (text_.empty()) {
    text::Draw(r, placeholder_.c_str(), x + kCaretGap * 2.0f, top, style_,
               colors::Get("steelLight"));
  } else {
    text::Draw(r, text_.c_str(), x, top, style_, colors::Get("silverLight"));
  }
  if (blink_ >= 0.5f) return;
  const SDL_Color caret = colors::Get("silverLight");
  const float text_w = text_.empty() ? 0.0f : text::Width(text_.c_str(), style_) + kCaretGap;
  const float caret_x = x + text_w;
  const float caret_h = line * 0.8f;
  const SDL_FRect bar{std::round(caret_x), std::round(cy - caret_h / 2.0f), 2.0f, caret_h};
  SDL_SetRenderDrawColor(r, caret.r, caret.g, caret.b, caret.a);
  SDL_RenderFillRect(r, &bar);
}
