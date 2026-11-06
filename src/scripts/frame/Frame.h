#pragma once

#include <SDL3/SDL.h>

namespace frame {

enum class Kind { kButton, kWindow, kButtonDanger };  // danger: dark red fill
enum class State { kNormal, kHighlight, kPressed };

// Inside `rect`: the outline takes the outer 3 px, the border the next 3 px.
constexpr float kOutline = 3.0f;
constexpr float kBorder = 3.0f;
constexpr float kInset = kOutline + kBorder;

SDL_FRect Inner(const SDL_FRect& rect);

// `alpha` (0..1) multiplies the opacity of everything.
void Draw(SDL_Renderer* r, const SDL_FRect& rect, Kind kind, State state = State::kNormal,
          float alpha = 1.0f);

}
