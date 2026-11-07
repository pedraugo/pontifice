#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace colors {

// Loads the flat {"name": "#rrggbb"} map; call again to reload.
bool Load(const std::string& path);

// By name ("silver", "stoneBlack"); unknown name is magenta, logged once.
SDL_Color Get(const std::string& name);
SDL_FColor GetF(const std::string& name);

// "#rrggbb" or "#rrggbbaa".
bool ParseHex(const std::string& hex, SDL_Color& out);

}
