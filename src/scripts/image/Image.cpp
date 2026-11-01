#include "scripts/image/Image.h"

#include <SDL3/SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

namespace {

Image FromStb(unsigned char* pixels, int w, int h, const char* what) {
  Image img;
  if (pixels == nullptr) {
    SDL_Log("stb_image falhou em %s: %s", what, stbi_failure_reason());
    return img;
  }
  img.width = w;
  img.height = h;
  img.rgba.assign(pixels, pixels + static_cast<size_t>(w) * h * 4);
  stbi_image_free(pixels);
  return img;
}

}  // namespace

Image Image::LoadFile(const std::string& path) {
  int w = 0, h = 0, n = 0;
  unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &n, 4);
  return FromStb(pixels, w, h, path.c_str());
}

Image Image::LoadMemory(const uint8_t* data, size_t size) {
  int w = 0, h = 0, n = 0;
  unsigned char* pixels = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &n, 4);
  return FromStb(pixels, w, h, "memoria");
}
