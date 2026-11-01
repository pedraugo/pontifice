#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#include "scenes/Scene.h"
#include "scenes/SceneManager.h"
#include "scripts/colors/Colors.h"
#include "scripts/image/Image.h"
#include "scripts/music/Music.h"
#include "scripts/text/Text.h"

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
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return false;
  }

  // Janela de tamanho fixo: sem redimensionar, maximizar ou entrar em tela cheia. Sem
  // SDL_WINDOW_RESIZABLE o sistema desabilita o zoom/maximizar e o SDL não oferece tela cheia.
  app.window = SDL_CreateWindow("PONTIFICE", kLogicalWidth, kLogicalHeight,
                                SDL_WINDOW_HIGH_PIXEL_DENSITY);
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

  const char* base_path = SDL_GetBasePath();
  const std::string base = base_path != nullptr ? base_path : "./";

  // Paleta (assets/colors.json) antes da primeira cena: menu e HUD pedem cores por papel.
  {
    const std::string palette = base + "assets/colors.json";
    if (!colors::Load(palette)) {
      std::fprintf(stderr, "Paleta nao carregada (%s); cores saem em magenta\n", palette.c_str());
    }
  }
  // Fonte do jogo (atlas de glifos no renderer); sem ela o texto usa a fonte de depuração.
  text::Init(app.renderer, base + "assets/PirataOne.ttf");

  SceneContext ctx;
  ctx.renderer = app.renderer;
  ctx.gpu_device = SDL_GetGPURendererDevice(app.renderer);  // nulo no renderer de fallback
  ctx.logical_width = kLogicalWidth;
  ctx.logical_height = kLogicalHeight;

  // Bloco próprio: as cenas (e seus recursos de GPU) precisam ser destruídas antes de
  // Shutdown(), que destrói o renderer, o device e encerra o SDL.
  {
  // Trilha sonora (assets/ost.json) toca em loop durante todo o jogo, em qualquer cena.
  Music music;
  music.Init(base);
  SceneManager scenes(std::move(ctx));

  // Depuração: PONTIFICE_SCENE=battle abre direto na batalha e =loading na tela de loading
  // (que segue para a batalha); PONTIFICE_MOUSE=x,y injeta um
  // movimento de mouse em coordenadas lógicas; PONTIFICE_SCREENSHOT=arquivo.bmp salva o
  // quadro PONTIFICE_SCREENSHOT_FRAME (padrão 60), ou o primeiro quadro depois de
  // PONTIFICE_SCREENSHOT_TIME segundos, e encerra.
  const char* env_scene = std::getenv("PONTIFICE_SCENE");
  const char* env_mouse = std::getenv("PONTIFICE_MOUSE");
  const char* env_shot = std::getenv("PONTIFICE_SCREENSHOT");
  const char* env_shot_frame = std::getenv("PONTIFICE_SCREENSHOT_FRAME");
  const char* env_shot_time = std::getenv("PONTIFICE_SCREENSHOT_TIME");
  const int shot_frame = env_shot_frame != nullptr ? std::atoi(env_shot_frame) : 60;
  const double shot_time = env_shot_time != nullptr ? std::atof(env_shot_time) : -1.0;
  SceneId first = SceneId::kMenu;
  if (env_scene != nullptr && std::strcmp(env_scene, "battle") == 0) first = SceneId::kBattle;
  if (env_scene != nullptr && std::strcmp(env_scene, "loading") == 0) first = SceneId::kLoading;
  scenes.SwitchTo(first);
  int frame = 0;

  const Uint64 start = SDL_GetTicksNS();  // depois da primeira cena carregar
  Uint64 last = start;
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

    ++frame;
    const double elapsed = static_cast<double>(now - start) / 1e9;
    const bool shot_now = shot_time >= 0.0 ? elapsed >= shot_time : frame == shot_frame;
    if (env_shot != nullptr && shot_now) {
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

  text::Shutdown();  // textura da fonte antes de destruir o renderer
  Shutdown(app);
  return 0;
}
