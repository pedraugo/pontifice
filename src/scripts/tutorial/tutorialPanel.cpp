#include "scripts/tutorial/tutorialPanel.h"

#include <algorithm>
#include <cmath>

#include "scripts/colors/colors.h"
#include "scripts/frame/frame.h"
#include "scripts/opacity/opacity.h"
#include "scripts/text/text.h"
#include "scripts/ui/ui.h"

namespace {

// Sizes from ui.json ("tutorial"), text variants from text.json.
struct Layout {
  float w = ui::Cells("tutorial.width", 40.0f);
  float padding = ui::Cells("tutorial.padding", 2.0f);
  float margin = ui::Cells("tutorial.margin", 2.0f);
  float counter_gap = ui::Cells("tutorial.counterGap", 0.5f);
  float title_gap = ui::Cells("tutorial.titleGap", 1.0f);
  float button_gap = ui::Cells("tutorial.buttonGap", 1.5f);
  float spotlight_margin = ui::Cells("tutorial.spotlightMargin", 0.5f);
  float spotlight_line = ui::Number("tutorial.spotlightLinePx", 3.0f);
  float spotlight_pulse = ui::Number("tutorial.spotlightPulseSpeed", 4.0f);
  text::Style counter_style = text::Styles({"xsmall", "body"});
  text::Style title_style = text::Styles({"large"});
  text::Style body_style = text::Styles({"description"});
};

const Layout& L() {
  static const Layout layout;
  return layout;
}

std::vector<std::string> Wrap(const std::string& body, const text::Style& style, float max_w) {
  std::vector<std::string> lines;
  std::string line, word;
  auto flush = [&] {
    if (word.empty()) return;
    const std::string candidate = line.empty() ? word : line + " " + word;
    if (line.empty() || text::Width(candidate.c_str(), style) <= max_w) {
      line = candidate;
    } else {
      lines.push_back(line);
      line = word;
    }
    word.clear();
  };
  for (const char c : body) {
    if (c == ' ') {
      flush();
    } else {
      word.push_back(c);
    }
  }
  flush();
  if (!line.empty()) lines.push_back(line);
  return lines;
}

}

TutorialPanel::TutorialPanel() : button_("") {}

void TutorialPanel::Show(const std::string& counter, const std::string& title,
                     const std::string& body, const std::string& button_label, bool modal,
                     int width, int height, SDL_FRect spotlight) {
  counter_ = counter;
  title_ = title;
  modal_ = modal;
  lines_ = Wrap(body, L().body_style, L().w - 2 * L().padding);
  button_.SetLabel(button_label);
  button_.SetFocused(true);

  float h = 2 * L().padding + text::LineHeight(L().counter_style) + L().counter_gap +
            text::LineHeight(L().title_style) + L().title_gap +
            static_cast<float>(lines_.size()) * text::LineHeight(L().body_style);
  if (modal_) h += L().button_gap + button_.Height();
  h = ui::SnapUp(h);
  const float x = modal_ ? (width - L().w) / 2.0f : L().margin;
  panel_ = SDL_FRect{x, (height - h) / 2.0f, L().w, h};
  SetSpotlight(spotlight, width, height);

  const float button_w = button_.PreferredWidth();
  button_.SetRect(SDL_FRect{panel_.x + panel_.w - L().padding - button_w,
                            panel_.y + panel_.h - L().padding - button_.Height(), button_w,
                            button_.Height()});
  open_ = true;
}

void TutorialPanel::SetSpotlight(SDL_FRect spotlight, int width, int height) {
  spotlight_ = SDL_FRect{};
  if (spotlight.w <= 0.0f || spotlight.h <= 0.0f) return;
  const float m = L().spotlight_margin;
  const float left = std::max(0.0f, spotlight.x - m);
  const float top = std::max(0.0f, spotlight.y - m);
  const float right = std::min(static_cast<float>(width), spotlight.x + spotlight.w + m);
  const float bottom = std::min(static_cast<float>(height), spotlight.y + spotlight.h + m);
  spotlight_ = SDL_FRect{left, top, right - left, bottom - top};
}

bool TutorialPanel::HandleEvent(const SDL_Event& event) {
  if (!Modal()) return false;
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
      case SDLK_SPACE:
        return true;
      default:
        return false;
    }
  }
  return button_.HandleEvent(event);
}

void TutorialPanel::Render(SDL_Renderer* r, int width, int height) const {
  if (!open_) return;
  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &previous);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  if (modal_) {
    const SDL_Color veil = opacity::Apply(colors::Get("stoneBlack"), "overlay");
    SDL_SetRenderDrawColor(r, veil.r, veil.g, veil.b, veil.a);
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    const SDL_FRect& s = spotlight_;
    if (s.w <= 0.0f) {
      const SDL_FRect full{0, 0, w, h};
      SDL_RenderFillRect(r, &full);
    } else {
      // Veil in four bands around the spotlight hole.
      const SDL_FRect bands[4] = {{0, 0, w, s.y},
                                  {0, s.y + s.h, w, h - s.y - s.h},
                                  {0, s.y, s.x, s.h},
                                  {s.x + s.w, s.y, w - s.x - s.w, s.h}};
      SDL_RenderFillRects(r, bands, 4);
      SDL_Color gold = colors::Get("gold");
      const float t = static_cast<float>(SDL_GetTicks()) / 1000.0f;
      gold.a = static_cast<Uint8>(gold.a * (0.75f + 0.25f * std::sin(t * L().spotlight_pulse)));
      SDL_SetRenderDrawColor(r, gold.r, gold.g, gold.b, gold.a);
      const float line = L().spotlight_line;
      const SDL_FRect outline[4] = {{s.x, s.y, s.w, line},
                                    {s.x, s.y + s.h - line, s.w, line},
                                    {s.x, s.y + line, line, s.h - 2 * line},
                                    {s.x + s.w - line, s.y + line, line, s.h - 2 * line}};
      SDL_RenderFillRects(r, outline, 4);
    }
  }
  frame::Draw(r, panel_, frame::Kind::kWindow);

  const float x = panel_.x + L().padding;
  float y = panel_.y + L().padding;
  text::Draw(r, counter_.c_str(), x, y, L().counter_style, colors::Get("steelLight"));
  y += text::LineHeight(L().counter_style) + L().counter_gap;
  text::Draw(r, title_.c_str(), x, y, L().title_style, colors::Get("silverLight"));
  y += text::LineHeight(L().title_style) + L().title_gap;
  for (const std::string& line : lines_) {
    text::Draw(r, line.c_str(), x, y, L().body_style, colors::Get("silver"));
    y += text::LineHeight(L().body_style);
  }
  if (modal_) button_.Render(r);

  SDL_SetRenderDrawBlendMode(r, previous);
}
