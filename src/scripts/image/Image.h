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
  // Cópia reduzida por um fator inteiro (média de blocos factor x factor). Para texturas que
  // só aparecem pequenas na tela, poupa memória e tempo de envio.
  Image Downscaled(int factor) const;
};
