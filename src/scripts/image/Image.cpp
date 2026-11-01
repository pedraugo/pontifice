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

Image Image::Downscaled(int factor) const {
  if (factor <= 1 || !Valid()) return *this;
  Image out;
  out.width = width / factor;
  out.height = height / factor;
  if (out.width <= 0 || out.height <= 0) return *this;
  out.rgba.resize(static_cast<size_t>(out.width) * out.height * 4);
  const unsigned samples = static_cast<unsigned>(factor * factor);
  for (int y = 0; y < out.height; ++y) {
    for (int x = 0; x < out.width; ++x) {
      unsigned sum[4] = {0, 0, 0, 0};
      for (int dy = 0; dy < factor; ++dy) {
        const uint8_t* row = &rgba[(static_cast<size_t>(y * factor + dy) * width + x * factor) * 4];
        for (int dx = 0; dx < factor; ++dx) {
          for (int c = 0; c < 4; ++c) sum[c] += row[dx * 4 + c];
        }
      }
      uint8_t* dst = &out.rgba[(static_cast<size_t>(y) * out.width + x) * 4];
      for (int c = 0; c < 4; ++c) dst[c] = static_cast<uint8_t>(sum[c] / samples);
    }
  }
  return out;
}
