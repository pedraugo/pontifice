#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Imagem RGBA8 em memória, decodificada de PNG/JPEG via stb_image.
struct Image {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba;

  bool Valid() const { return width > 0 && height > 0 && !rgba.empty(); }

  static Image LoadFile(const std::string& path);
  static Image LoadMemory(const uint8_t* data, size_t size);
};
