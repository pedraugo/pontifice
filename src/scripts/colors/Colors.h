#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace colors {

// Loads "roles.*" (flattened, no prefix) and "palette.*"; call again to reload.
bool Load(const std::string& path);

// By role ("panel.fill", "text.primary"), never by hue; unknown role is magenta, logged once.
SDL_Color Get(const std::string& role);
SDL_FColor GetF(const std::string& role);

// "#rrggbb" or "#rrggbbaa".
bool ParseHex(const std::string& hex, SDL_Color& out);

}
