#include "scripts/vignette/vignette.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "scripts/ui/ui.h"

namespace vignette {

namespace {

SDL_Texture* g_texture = nullptr;

// Black with alpha rising from `inner` to `outer` (distance from the center, y squashed).
SDL_Texture* Create(SDL_Renderer* r, int width, int height) {
  const float strength = ui::Number("vignette.strength", 0.55f);
  const float inner = ui::Number("vignette.inner", 0.3f);
  const float outer = std::max(ui::Number("vignette.outer", 0.8f), inner + 0.01f);
  const float squash = ui::Number("vignette.squash", 0.85f);
  const int w = std::max(width / 4, 1);
  const int h = std::max(height / 4, 1);
  std::vector<uint8_t> rgba(static_cast<size_t>(w) * h * 4, 0);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float u = (x + 0.5f) / w - 0.5f;
      const float v = ((y + 0.5f) / h - 0.5f) * squash;
      const float t = std::clamp((std::sqrt(u * u + v * v) - inner) / (outer - inner), 0.0f, 1.0f);
      const float alpha = std::clamp(strength * t * t * (3.0f - 2.0f * t), 0.0f, 1.0f);
      rgba[(static_cast<size_t>(y) * w + x) * 4 + 3] = static_cast<uint8_t>(alpha * 255.0f + 0.5f);
    }
  }
  SDL_Texture* texture =
      SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
  if (texture == nullptr) return nullptr;
  SDL_UpdateTexture(texture, nullptr, rgba.data(), w * 4);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
  return texture;
}

}

void Draw(SDL_Renderer* r, int width, int height) {
  if (g_texture == nullptr) g_texture = Create(r, width, height);
  if (g_texture == nullptr) return;
  const SDL_FRect full{0, 0, static_cast<float>(width), static_cast<float>(height)};
  SDL_RenderTexture(r, g_texture, nullptr, &full);
}

void Shutdown() {
  if (g_texture != nullptr) SDL_DestroyTexture(g_texture);
  g_texture = nullptr;
}

}
