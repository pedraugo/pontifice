#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>

#include "scenes/Scene.h"
#include "scenes/SceneManager.h"

namespace {

constexpr int kLogicalWidth = 1280;
constexpr int kLogicalHeight = 720;

struct App {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
};

bool Init(App& app) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return false;
  }

  app.window = SDL_CreateWindow("PONTIFICE", kLogicalWidth, kLogicalHeight,
                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (app.window == nullptr) {
    std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
    return false;
  }

  // Renderer 2D sobre o SDL_GPU (Metal no macOS, Vulkan/D3D12 em outros sistemas).
  // O device fica acessível via SDL_GetGPURendererDevice para o render 3D do campo.
  app.renderer = SDL_CreateGPURenderer(nullptr, app.window);
  if (app.renderer == nullptr) {
    std::fprintf(stderr, "SDL_CreateGPURenderer falhou (%s), usando renderer padrao\n",
                 SDL_GetError());
    app.renderer = SDL_CreateRenderer(app.window, nullptr);
  }
  if (app.renderer == nullptr) {
    std::fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError());
    return false;
  }

  SDL_SetRenderVSync(app.renderer, 1);
  SDL_SetRenderLogicalPresentation(app.renderer, kLogicalWidth, kLogicalHeight,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  std::printf("Renderer: %s\n", SDL_GetRendererName(app.renderer));
  return true;
}

void Shutdown(App& app) {
  if (app.renderer != nullptr) SDL_DestroyRenderer(app.renderer);
  if (app.window != nullptr) SDL_DestroyWindow(app.window);
  SDL_Quit();
}

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
  App app;
  if (!Init(app)) {
    Shutdown(app);
    return 1;
  }

  SceneContext ctx;
  ctx.renderer = app.renderer;
  ctx.logical_width = kLogicalWidth;
  ctx.logical_height = kLogicalHeight;

  SceneManager scenes(ctx);
  scenes.SwitchTo(SceneId::kMenu);

  Uint64 last = SDL_GetTicksNS();
  while (!scenes.ShouldQuit()) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        Shutdown(app);
        return 0;
      }
      // Converte coordenadas de mouse da janela para a resolução lógica.
      SDL_ConvertEventToRenderCoordinates(app.renderer, &event);
      scenes.HandleEvent(event);
    }

    const Uint64 now = SDL_GetTicksNS();
    const float dt = static_cast<float>(now - last) / 1e9f;
    last = now;

    scenes.Update(dt);
    scenes.Render();
    SDL_RenderPresent(app.renderer);
  }

  Shutdown(app);
  return 0;
}
