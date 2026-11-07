#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace opacity {

// Loads the flat {"name": 0..1} map; call again to reload.
bool Load(const std::string& path);

// By name ("overlay", "frameBorder"); unknown name is 1 (opaque), logged once.
float Get(const std::string& name);

// `color` with its alpha multiplied by opacity `name`.
SDL_Color Apply(SDL_Color color, const std::string& name);
SDL_FColor Apply(SDL_FColor color, const std::string& name);

}
