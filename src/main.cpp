#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "scenes/Scene.h"
#include "scenes/SceneManager.h"
#include "scripts/colors/Colors.h"
#include "scripts/image/Image.h"

namespace {

constexpr int kLogicalWidth = 1280;
constexpr int kLogicalHeight = 720;

struct App {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
};

// Ícone da janela a partir de assets/icon.png: título e barra de tarefas no Windows, janela e
// dock no Linux, Dock no macOS (SDL chama setApplicationIconImage). O ícone do executável em
// si vem de src/icons (recurso .rc no Windows).
void SetWindowIcon(SDL_Window* window) {
  const char* base = SDL_GetBasePath();
  const std::string path = std::string(base != nullptr ? base : "./") + "assets/icon.png";
  const Image image = Image::LoadFile(path);
  if (!image.Valid()) {
    std::fprintf(stderr, "Icone nao carregado: %s\n", path.c_str());
    return;
  }
  SDL_Surface* icon = SDL_CreateSurfaceFrom(image.width, image.height, SDL_PIXELFORMAT_RGBA32,
                                            const_cast<uint8_t*>(image.rgba.data()),
                                            image.width * 4);
  if (icon == nullptr) return;
#ifndef SDL_PLATFORM_MACOS
  // Fora do macOS os sistemas não precisam de 1024 px; X11 copia o ícone inteiro para uma
  // propriedade da janela, então reduz para 256.
  constexpr int kMaxSize = 256;
  if (image.width > kMaxSize || image.height > kMaxSize) {
    SDL_Surface* small = SDL_ScaleSurface(icon, kMaxSize, kMaxSize, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(icon);
    icon = small;
    if (icon == nullptr) return;
  }
#endif
  if (!SDL_SetWindowIcon(window, icon)) {
    std::fprintf(stderr, "SDL_SetWindowIcon: %s\n", SDL_GetError());
  }
  SDL_DestroySurface(icon);
}

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
  SetWindowIcon(app.window);

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

  // Paleta (assets/colors.json) antes da primeira cena: menu e HUD pedem cores por papel.
  {
    const char* base = SDL_GetBasePath();
    const std::string palette = std::string(base != nullptr ? base : "./") + "assets/colors.json";
    if (!colors::Load(palette)) {
      std::fprintf(stderr, "Paleta nao carregada (%s); cores saem em magenta\n", palette.c_str());
    }
  }

  SceneContext ctx;
  ctx.renderer = app.renderer;
  ctx.logical_width = kLogicalWidth;
  ctx.logical_height = kLogicalHeight;

  // Bloco próprio: as cenas (e seus recursos de GPU) precisam ser destruídas antes de
  // Shutdown(), que destrói o renderer, o device e encerra o SDL.
  {
  SceneManager scenes(ctx);

  // Depuração: PONTIFICE_SCENE=battle abre direto na batalha; PONTIFICE_MOUSE=x,y injeta um
  // movimento de mouse em coordenadas lógicas; PONTIFICE_SCREENSHOT=arquivo.bmp salva o
  // quadro 60 e encerra.
  const char* env_scene = std::getenv("PONTIFICE_SCENE");
  const char* env_mouse = std::getenv("PONTIFICE_MOUSE");
  const char* env_shot = std::getenv("PONTIFICE_SCREENSHOT");
  scenes.SwitchTo(env_scene != nullptr && std::strcmp(env_scene, "battle") == 0 ? SceneId::kBattle
                                                                                : SceneId::kMenu);
  int frame = 0;

  Uint64 last = SDL_GetTicksNS();
  while (!scenes.ShouldQuit()) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        scenes.RequestQuit();
        break;
      }
      // Converte coordenadas de mouse da janela para a resolução lógica.
      SDL_ConvertEventToRenderCoordinates(app.renderer, &event);
      scenes.HandleEvent(event);
    }

    if (env_mouse != nullptr) {
      SDL_Event fake{};
      fake.type = SDL_EVENT_MOUSE_MOTION;
      std::sscanf(env_mouse, "%f,%f", &fake.motion.x, &fake.motion.y);
      scenes.HandleEvent(fake);
    }

    const Uint64 now = SDL_GetTicksNS();
    const float dt = static_cast<float>(now - last) / 1e9f;
    last = now;

    scenes.Update(dt);
    scenes.Render();

    if (env_shot != nullptr && ++frame == 60) {
      SDL_Surface* shot = SDL_RenderReadPixels(app.renderer, nullptr);
      if (shot != nullptr) {
        SDL_SaveBMP(shot, env_shot);
        SDL_DestroySurface(shot);
        std::printf("Screenshot salvo em %s\n", env_shot);
      } else {
        std::fprintf(stderr, "SDL_RenderReadPixels: %s\n", SDL_GetError());
      }
      SDL_RenderPresent(app.renderer);
      break;
    }

    SDL_RenderPresent(app.renderer);
  }
  }  // cenas destruídas aqui

  Shutdown(app);
  return 0;
}
