#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace flag {

// Simplified to read at ~48 px: no coat of arms, no motto band, stars as a dot grid.
enum class Country { kUnitedStates, kSpain, kFrance, kBrazil };

Country FromCode(const std::string& code);  // translate.json codes; unknown -> Brazil

// Ideal aspect 3:2. saturation 1 = real colors, 0 = black and white.
void Draw(SDL_Renderer* r, Country country, const SDL_FRect& rect, float saturation = 1.0f);

}
