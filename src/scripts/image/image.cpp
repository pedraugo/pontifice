#include "scripts/image/image.h"

#include <SDL3/SDL.h>
#include <webp/decode.h>

#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>
#include "scripts/log/log.h"

namespace {

Image FromStb(unsigned char* pixels, int w, int h, const char* what) {
  Image img;
  if (pixels == nullptr) {
    LOG("stb_image failed on %s: %s", what, stbi_failure_reason());
    return img;
  }
  img.width = w;
  img.height = h;
  img.rgba.assign(pixels, pixels + static_cast<size_t>(w) * h * 4);
  stbi_image_free(pixels);
  return img;
}

// RIFF container with "WEBP" type: bytes 0-3 "RIFF", 8-11 "WEBP"
bool IsWebP(const uint8_t* data, size_t size) {
  return size >= 12 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WEBP", 4) == 0;
}

Image Decode(const uint8_t* data, size_t size, const char* what) {
  if (IsWebP(data, size)) {
    Image img;
    int w = 0, h = 0;
    uint8_t* pixels = WebPDecodeRGBA(data, size, &w, &h);
    if (pixels == nullptr) {
      LOG("libwebp failed on %s", what);
      return img;
    }
    img.width = w;
    img.height = h;
    img.rgba.assign(pixels, pixels + static_cast<size_t>(w) * h * 4);
    WebPFree(pixels);
    return img;
  }
  int w = 0, h = 0, n = 0;
  unsigned char* pixels = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &n, 4);
  return FromStb(pixels, w, h, what);
}

}

Image Image::LoadFile(const std::string& path) {
  size_t size = 0;
  void* data = SDL_LoadFile(path.c_str(), &size);
  if (data == nullptr) {
    LOG("Failed to read %s: %s", path.c_str(), SDL_GetError());
    return Image();
  }
  Image img = Decode(static_cast<const uint8_t*>(data), size, path.c_str());
  SDL_free(data);
  return img;
}

Image Image::LoadMemory(const uint8_t* data, size_t size) {
  return Decode(data, size, "memoria");
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
