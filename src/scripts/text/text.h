#pragma once

#include <SDL3/SDL.h>

#include <initializer_list>
#include <string>

#include "scripts/image/image.h"

namespace text {

// kTitle: PirataOne (titles, buttons, labels). kBody: Newsreader (running text).
enum class Face { kTitle, kBody };

// Text variant: fields from assets/text.json merged by name ("large" + "bold" + "body").
struct Style {
  float size = 2.0f;  // debug-font units; line height = 8 px * size * ui::TextScale()
  Face face = Face::kTitle;
  bool bold = false;
  bool italic = false;
  float letter_spacing = 0.0f;  // em fraction added between glyphs
  float line_gap = 0.0f;  // fraction of the line height added to LineHeight
};

// Merges variants left to right; later ones override the fields they define.
Style Styles(std::initializer_list<const char*> names);
Style StyleOf(const std::string& names);  // "large+bold"
float Scale(const Style& style);  // size times ui::TextScale()
float LineHeight(const Style& style);

float Width(const char* str, const Style& style);
void Draw(SDL_Renderer* r, const char* str, float x, float y, const Style& style,
          SDL_Color color);
void DrawCentered(SDL_Renderer* r, const char* str, float center_x, float center_y,
                  const Style& style, SDL_Color color);

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
// `spacing` is extra px between glyphs.
float ImageWidth(const char* str, float px, Face face = Face::kTitle, float spacing = 0.0f);
void DrawOnImage(Image& img, const char* str, float x, float y, float px, SDL_Color color,
                 Face face = Face::kTitle, float spacing = 0.0f);

}
