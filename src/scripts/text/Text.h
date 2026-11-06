#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "scripts/image/Image.h"

namespace text {

// kTitle: PirataOne (titles, buttons, labels). kBody: Newsreader (running text).
enum class Face { kTitle, kBody };

// Missing body font falls back to title; missing title falls back to SDL debug text.
bool Init(SDL_Renderer* r, const std::string& ttf_path, Face face = Face::kTitle);
void Shutdown();

// `scale` keeps the debug-font convention: line height = 8 px * scale.
float Width(const char* str, float scale, Face face = Face::kTitle);
void Draw(SDL_Renderer* r, const char* str, float x, float y, float scale, SDL_Color color,
          Face face = Face::kTitle);
void DrawCentered(SDL_Renderer* r, const char* str, float center_x, float center_y, float scale,
                  SDL_Color color, Face face = Face::kTitle);

// CPU raster into an Image: `px` is the em height, `y` the em top; thread-safe after Init.
float ImageWidth(const char* str, float px, Face face = Face::kTitle);
void DrawOnImage(Image& img, const char* str, float x, float y, float px, SDL_Color color,
                 Face face = Face::kTitle);

}
