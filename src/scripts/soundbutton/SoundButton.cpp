#include "scripts/soundbutton/SoundButton.h"

#include <cmath>
#include <vector>

#include "scripts/colors/Colors.h"
#include "scripts/frame/Frame.h"
#include "scripts/ui/Ui.h"

namespace {

SDL_FColor ToF(SDL_Color c) { return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f}; }

// Polígono convexo preenchido (leque de triângulos), em coordenadas de tela.
void FillPolygon(SDL_Renderer* r, const SDL_FPoint* pts, int n, SDL_Color color) {
  std::vector<SDL_Vertex> v(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = SDL_Vertex{pts[i], ToF(color), {0, 0}};
  std::vector<int> idx;
  for (int i = 1; i + 1 < n; ++i) idx.insert(idx.end(), {0, i, i + 1});
  SDL_RenderGeometry(r, nullptr, v.data(), n, idx.data(), static_cast<int>(idx.size()));
}

// Arco (polilinha) de centro c, raio rad, entre os ângulos a0 e a1 (rad), com 2 px de traço.
void Arc(SDL_Renderer* r, SDL_FPoint c, float rad, float a0, float a1, SDL_Color color) {
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
  const int steps = 10;
  for (int pass = 0; pass < 2; ++pass) {
    const float rr = rad + static_cast<float>(pass);
    SDL_FPoint prev{c.x + rr * std::cos(a0), c.y + rr * std::sin(a0)};
    for (int i = 1; i <= steps; ++i) {
      const float a = a0 + (a1 - a0) * static_cast<float>(i) / steps;
      const SDL_FPoint p{c.x + rr * std::cos(a), c.y + rr * std::sin(a)};
      SDL_RenderLine(r, prev.x, prev.y, p.x, p.y);
      prev = p;
    }
  }
}

}  // namespace

bool SoundButton::Contains(float x, float y) const {
  return x >= rect_.x && x < rect_.x + rect_.w && y >= rect_.y && y < rect_.y + rect_.h;
}

bool SoundButton::HandleEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
      hovered_ = Contains(event.motion.x, event.motion.y);
      return false;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      if (event.button.button == SDL_BUTTON_LEFT && Contains(event.button.x, event.button.y)) {
        pressed_ = true;
      }
      return false;
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      if (event.button.button != SDL_BUTTON_LEFT) return false;
      const bool clicked = pressed_ && Contains(event.button.x, event.button.y);
      pressed_ = false;
      return clicked;
    }
    default:
      return false;
  }
}

void SoundButton::Render(SDL_Renderer* r, bool muted) const {
  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &previous);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  // Fundo como os botões de texto: placa de pedra (ou cores "button.*" sem ela).
  if (frame::Ready()) {
    frame::Draw(r, rect_, frame::Kind::kButton, pressed_ ? 0.6f : hovered_ ? 1.0f : 0.8f);
  } else {
    SDL_Color fill = colors::Get(pressed_ ? "button.pressedFill" : hovered_ ? "button.hoverFill" : "button.fill");
    SDL_SetRenderDrawColor(r, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(r, &rect_);
    SDL_Color border = colors::Get(hovered_ ? "button.hoverBorder" : "button.border");
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, border.a);
    SDL_RenderRect(r, &rect_);
  }

  // Ícone em coordenadas 0..1 da caixa: corpo do alto-falante, cone, ondas (ou X).
  SDL_Color ink = colors::Get("button.text");
  if (muted) ink = colors::Get("text.muted");
  auto P = [&](float u, float v) {
    return SDL_FPoint{rect_.x + u * rect_.w, rect_.y + v * rect_.h};
  };
  const SDL_FPoint body[4] = {P(0.20f, 0.40f), P(0.34f, 0.40f), P(0.34f, 0.60f), P(0.20f, 0.60f)};
  FillPolygon(r, body, 4, ink);
  const SDL_FPoint cone[4] = {P(0.34f, 0.40f), P(0.52f, 0.24f), P(0.52f, 0.76f), P(0.34f, 0.60f)};
  FillPolygon(r, cone, 4, ink);
  if (!muted) {
    const SDL_FPoint c = P(0.52f, 0.50f);
    Arc(r, c, rect_.w * 0.12f, -0.9f, 0.9f, ink);
    Arc(r, c, rect_.w * 0.22f, -0.9f, 0.9f, ink);
  } else {
    // X ao lado do cone.
    const SDL_Color x = colors::Get("status.nerf");
    SDL_SetRenderDrawColor(r, x.r, x.g, x.b, x.a);
    const float t = 1.5f;
    for (int k = -1; k <= 1; ++k) {
      const float o = static_cast<float>(k) * t;
      SDL_RenderLine(r, P(0.60f, 0.38f).x + o, P(0.60f, 0.38f).y, P(0.80f, 0.62f).x + o,
                     P(0.80f, 0.62f).y);
      SDL_RenderLine(r, P(0.80f, 0.38f).x + o, P(0.80f, 0.38f).y, P(0.60f, 0.62f).x + o,
                     P(0.60f, 0.62f).y);
    }
  }
  SDL_SetRenderDrawBlendMode(r, previous);
}
