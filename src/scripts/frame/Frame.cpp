#include "scripts/frame/Frame.h"

#include <algorithm>
#include <cmath>

#include "scripts/image/Image.h"

namespace frame {

namespace {

struct Sheet {
  SDL_Texture* texture = nullptr;
  float tile_src = 0.0f;  // lado de cada um dos 9 tiles na textura (um terço)
};
Sheet sheets[3];  // na ordem de Kind: kButton, kWindow, kButtonDanger

bool Load(SDL_Renderer* r, const std::string& path, Sheet& sheet) {
  Image img = Image::LoadFile(path);
  if (!img.Valid() || img.width != img.height) {
    SDL_Log("Moldura da interface nao carregada: %s", path.c_str());
    return false;
  }
  sheet.texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                    img.width, img.height);
  if (sheet.texture == nullptr) return false;
  SDL_UpdateTexture(sheet.texture, nullptr, img.rgba.data(), img.width * 4);
  SDL_SetTextureBlendMode(sheet.texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(sheet.texture, SDL_SCALEMODE_LINEAR);
  sheet.tile_src = static_cast<float>(img.width) / 3.0f;
  return true;
}

}  // namespace

bool Init(SDL_Renderer* r, const std::string& dir) {
  Shutdown();
  const bool button = Load(r, dir + "UiFrameButton.png", sheets[0]);
  const bool window = Load(r, dir + "UiFrameWindow.png", sheets[1]);
  if (!button || !window) {
    Shutdown();
    return false;
  }
  Load(r, dir + "UiFrameButtonDanger.png", sheets[2]);  // opcional: cai na de botão
  return true;
}

void Shutdown() {
  for (Sheet& sheet : sheets) {
    if (sheet.texture != nullptr) SDL_DestroyTexture(sheet.texture);
    sheet = Sheet{};
  }
}

bool Ready() { return sheets[0].texture != nullptr && sheets[1].texture != nullptr; }

void Draw(SDL_Renderer* r, const SDL_FRect& rect, Kind kind, float brightness,
          float ideal_tile, float alpha) {
  const Sheet* chosen = &sheets[static_cast<int>(kind)];
  if (kind == Kind::kButtonDanger && chosen->texture == nullptr) chosen = &sheets[0];
  const Sheet& sheet = *chosen;
  SDL_Texture* texture = sheet.texture;
  if (texture == nullptr || rect.w <= 0.0f || rect.h <= 0.0f) return;
  if (ideal_tile <= 0.0f) ideal_tile = kIdealTile;
  // Linhas: pelo menos 2 (cima e baixo); painéis altos ganham linhas do meio. Tile quadrado
  // com lado = altura / linhas; colunas do meio em número inteiro, levemente esticadas para
  // fechar a largura exata.
  const int rows = std::max(2, static_cast<int>(std::floor(rect.h / ideal_tile + 0.5f)));
  const float tile = rect.h / static_cast<float>(rows);
  const float inner_w = std::max(0.0f, rect.w - 2.0f * tile);
  const int cols = std::max(1, static_cast<int>(std::floor(inner_w / tile + 0.5f)));
  const float mid_w = inner_w / static_cast<float>(cols);

  const Uint8 mod = static_cast<Uint8>(std::clamp(brightness, 0.0f, 1.0f) * 255.0f);
  SDL_SetTextureColorMod(texture, mod, mod, mod);
  SDL_SetTextureAlphaMod(texture, static_cast<Uint8>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));

  const float ts = sheet.tile_src;
  auto src = [&](int col, int row) { return SDL_FRect{col * ts, row * ts, ts, ts}; };
  for (int row = 0; row < rows; ++row) {
    const int srow = row == 0 ? 0 : (row == rows - 1 ? 2 : 1);
    const float y = rect.y + row * tile;
    SDL_FRect dst{rect.x, y, tile, tile};
    SDL_FRect s = src(0, srow);
    SDL_RenderTexture(r, texture, &s, &dst);
    s = src(1, srow);
    for (int col = 0; col < cols; ++col) {
      dst = SDL_FRect{rect.x + tile + col * mid_w, y, mid_w, tile};
      SDL_RenderTexture(r, texture, &s, &dst);
    }
    s = src(2, srow);
    dst = SDL_FRect{rect.x + rect.w - tile, y, tile, tile};
    SDL_RenderTexture(r, texture, &s, &dst);
  }
  SDL_SetTextureColorMod(texture, 255, 255, 255);
  SDL_SetTextureAlphaMod(texture, 255);
}

}  // namespace frame
