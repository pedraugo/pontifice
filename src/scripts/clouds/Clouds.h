#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

// Nuvens e névoa em movimento para fundos 2D (menu). Texturas de ruído geradas na CPU ao
// iniciar, repetíveis nas duas direções, desenhadas em camadas que rolam em velocidades
// diferentes: camadas de trás (densas, escuras, lentas) ficam atrás do texto; camadas da
// frente (névoa rala, nuvens soltas e fiapos rápidos, claras e translúcidas) passam por cima
// do texto e dos botões.
class Clouds {
 public:
  ~Clouds();
  Clouds() = default;
  Clouds(const Clouds&) = delete;
  Clouds& operator=(const Clouds&) = delete;

  bool Init(SDL_Renderer* renderer, int width, int height);
  void Update(float dt_seconds);
  void RenderBack(SDL_Renderer* renderer) const;
  void RenderFront(SDL_Renderer* renderer) const;

 private:
  struct Layer {
    SDL_Texture* texture = nullptr;
    std::string color_role;            // papel em colors.json
    float scale = 1.0f;                // tamanho do ladrilho em relação à textura
    float speed_x = 0.0f;              // px/s na tela (positivo = para a direita)
    float speed_y = 0.0f;
    float alpha = 1.0f;
    float offset_x = 0.0f;             // deslocamento atual (px), já reduzido ao ladrilho
    float offset_y = 0.0f;
  };

  void RenderLayer(SDL_Renderer* renderer, const Layer& layer) const;
  SDL_Texture* MakeNoiseTexture(SDL_Renderer* renderer, unsigned seed, float threshold,
                                float softness);

  int width_ = 0;
  int height_ = 0;
  std::vector<SDL_Texture*> textures_;
  std::vector<Layer> back_;
  std::vector<Layer> front_;
};
