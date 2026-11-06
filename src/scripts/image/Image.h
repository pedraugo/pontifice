#pragma once

#include <cstdint>
#include <string>
#include <vector>

// RGBA8 in memory, decoded from WebP (libwebp) or PNG/JPEG (stb_image)
struct Image {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba;

  bool Valid() const { return width > 0 && height > 0 && !rgba.empty(); }

  static Image LoadFile(const std::string& path);
  static Image LoadMemory(const uint8_t* data, size_t size);
  Image Downscaled(int factor) const;  // box average of factor x factor blocks
};
