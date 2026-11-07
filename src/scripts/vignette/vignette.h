#pragma once

#include <SDL3/SDL.h>

// Screen-edge darkening from assets/ui.json ("vignette"), between a scene's game layer and HUD.
namespace vignette {

void Draw(SDL_Renderer* r, int width, int height);
void Shutdown();  // frees the texture before the renderer goes away

}
