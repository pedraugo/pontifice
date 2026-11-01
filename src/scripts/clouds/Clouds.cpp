#include "scripts/clouds/Clouds.h"

#include <cmath>
#include <cstdint>

#include "scripts/colors/Colors.h"
#include "scripts/ui/Ui.h"

namespace {

constexpr int kTexW = 512;
constexpr int kTexH = 256;
constexpr int kOctaves = 5;

// Hash inteiro determinístico (lattice periódico: índices já reduzidos ao período).
float Hash(int x, int y, unsigned seed) {
  uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u +
               seed * 2246822519u;
  h = (h ^ (h >> 13)) * 1274126177u;
  h ^= h >> 16;
  return static_cast<float>(h & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
}

float Smooth(float t) { return t * t * (3.0f - 2.0f * t); }

// Ruído de valor com período `period` células em cada eixo; u, v em [0, 1).
float ValueNoise(float u, float v, int period_x, int period_y, unsigned seed) {
  const float fx = u * period_x;
  const float fy = v * period_y;
  const int ix = static_cast<int>(std::floor(fx));
  const int iy = static_cast<int>(std::floor(fy));
  const float tx = Smooth(fx - ix);
  const float ty = Smooth(fy - iy);
  auto at = [&](int dx, int dy) {
    return Hash((ix + dx) % period_x, (iy + dy) % period_y, seed);
  };
  const float a = at(0, 0), b = at(1, 0), c = at(0, 1), d = at(1, 1);
  return (a + (b - a) * tx) * (1 - ty) + (c + (d - c) * tx) * ty;
}

}  // namespace

Clouds::~Clouds() {
  for (SDL_Texture* t : textures_) SDL_DestroyTexture(t);
}

SDL_Texture* Clouds::MakeNoiseTexture(SDL_Renderer* renderer, unsigned seed, float threshold,
                                      float softness) {
  std::vector<uint8_t> pixels(static_cast<size_t>(kTexW) * kTexH * 4);
  for (int y = 0; y < kTexH; ++y) {
    for (int x = 0; x < kTexW; ++x) {
      const float u = static_cast<float>(x) / kTexW;
      const float v = static_cast<float>(y) / kTexH;
      // fBm: oitavas com o dobro da frequência e metade da amplitude.
      float sum = 0.0f, amp = 0.5f, total = 0.0f;
      int px = 4, py = 2;  // proporção 2:1 da textura mantém as células quadradas
      for (int o = 0; o < kOctaves; ++o) {
        sum += ValueNoise(u, v, px, py, seed + o * 101u) * amp;
        total += amp;
        amp *= 0.5f;
        px *= 2;
        py *= 2;
      }
      const float n = sum / total;  // ~[0, 1], concentrado em torno de 0,5
      // Limiar suave: abaixo de threshold não há nuvem; acima, densidade cresce até 1.
      const float t = std::fmin(std::fmax((n - threshold) / softness, 0.0f), 1.0f);
      const float alpha = Smooth(t);
      uint8_t* p = &pixels[(static_cast<size_t>(y) * kTexW + x) * 4];
      p[0] = p[1] = p[2] = 255;
      p[3] = static_cast<uint8_t>(alpha * 255.0f);
    }
  }
  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STATIC, kTexW, kTexH);
  if (texture == nullptr) return nullptr;
  SDL_UpdateTexture(texture, nullptr, pixels.data(), kTexW * 4);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
  textures_.push_back(texture);
  return texture;
}

bool Clouds::Init(SDL_Renderer* renderer, int width, int height) {
  width_ = width;
  height_ = height;
  SDL_Texture* far = MakeNoiseTexture(renderer, 3u, 0.36f, 0.34f);    // manto contínuo
  SDL_Texture* dense = MakeNoiseTexture(renderer, 7u, 0.42f, 0.30f);  // massas grandes
  SDL_Texture* wispy = MakeNoiseTexture(renderer, 31u, 0.55f, 0.18f); // fiapos ralos
  SDL_Texture* puffs = MakeNoiseTexture(renderer, 53u, 0.60f, 0.10f); // nuvens soltas
  if (far == nullptr || dense == nullptr || wispy == nullptr || puffs == nullptr) return false;

  // Atrás do texto, do fundo para a frente: cada camada mais clara, menor e mais rápida que
  // a anterior; sentidos alternados reforçam a paralaxe.
  back_ = {
      {far, "menu.cloudFar", 4.0f, 4.0f, 0.5f, 0.90f, 0.0f, 0.0f},
      {dense, "menu.cloudBack", 3.0f, 9.0f, 1.5f, 0.85f, 900.0f, 200.0f},
      {dense, "menu.cloudMid", 2.2f, -16.0f, -2.0f, 0.55f, 600.0f, 90.0f},
      {wispy, "menu.cloudNear", 1.8f, 21.0f, 2.5f, 0.45f, 150.0f, 300.0f},
  };
  // Na frente do texto e dos botões: névoa rala contínua, nuvens soltas que passam por cima
  // de vez em quando e fiapos rápidos bem transparentes; tudo translúcido para não esconder.
  front_ = {
      {wispy, "menu.cloudFront", 2.6f, 28.0f, 3.0f, 0.35f, 300.0f, 40.0f},
      {puffs, "menu.cloudPuff", 2.8f, 38.0f, -4.0f, 0.70f, 1100.0f, 500.0f},
      {wispy, "menu.cloudFront", 1.4f, 56.0f, 5.0f, 0.22f, 500.0f, 120.0f},
  };
  // Medidas acima em px de 1280x720: tamanho do ladrilho, velocidade e deslocamento escalam
  // para a resolução lógica, mantendo o mesmo visual.
  for (std::vector<Layer>* group : {&back_, &front_}) {
    for (Layer& layer : *group) {
      layer.scale *= ui::kScale;
      layer.speed_x *= ui::kScale;
      layer.speed_y *= ui::kScale;
      layer.offset_x *= ui::kScale;
      layer.offset_y *= ui::kScale;
    }
  }
  return true;
}

void Clouds::Update(float dt) {
  auto advance = [dt](Layer& layer) {
    const float tile_w = kTexW * layer.scale;
    const float tile_h = kTexH * layer.scale;
    layer.offset_x = std::fmod(layer.offset_x + layer.speed_x * dt + tile_w, tile_w);
    layer.offset_y = std::fmod(layer.offset_y + layer.speed_y * dt + tile_h, tile_h);
  };
  for (Layer& layer : back_) advance(layer);
  for (Layer& layer : front_) advance(layer);
}

void Clouds::RenderLayer(SDL_Renderer* renderer, const Layer& layer) const {
  const SDL_Color c = colors::Get(layer.color_role);
  SDL_SetTextureColorMod(layer.texture, c.r, c.g, c.b);
  SDL_SetTextureAlphaMod(layer.texture, static_cast<Uint8>(layer.alpha * c.a));
  const float tile_w = kTexW * layer.scale;
  const float tile_h = kTexH * layer.scale;
  // Ladrilhos cobrindo a tela; o primeiro começa antes da borda pelo deslocamento atual.
  for (float y = -layer.offset_y; y < height_; y += tile_h) {
    for (float x = -layer.offset_x; x < width_; x += tile_w) {
      const SDL_FRect dst{x, y, tile_w, tile_h};
      SDL_RenderTexture(renderer, layer.texture, nullptr, &dst);
    }
  }
}

void Clouds::RenderBack(SDL_Renderer* renderer) const {
  for (const Layer& layer : back_) RenderLayer(renderer, layer);
}

void Clouds::RenderFront(SDL_Renderer* renderer) const {
  for (const Layer& layer : front_) RenderLayer(renderer, layer);
}
