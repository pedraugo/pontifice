#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "scenes/scene.h"
#include "scenes/sceneManager.h"
#include "scripts/colors/colors.h"
#include "scripts/config/config.h"
#include "scripts/image/image.h"
#include "scripts/i18n/i18n.h"
#include "scripts/audios/music.h"
#include "scripts/text/text.h"
#include "scripts/log/log.h"

namespace {

constexpr int kLogicalWidth = 1920;
constexpr int kLogicalHeight = 1080;

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
    LOG("Icon not loaded: %s", path.c_str());
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
    LOG("SDL_SetWindowIcon: %s", SDL_GetError());
  }
  SDL_DestroySurface(icon);
}

bool Init(App& app) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    LOG("SDL_Init: %s", SDL_GetError());
    return false;
  }

  // Tela cheia (sem borda, na resolução do monitor) por padrão; a resolução lógica 1920x1080
  // vai com letterbox. Sem SDL_WINDOW_RESIZABLE o usuário não redimensiona nem sai da tela
  // cheia pelo sistema. PONTIFICE_WINDOWED=1 (depuração) abre em janela de 1280x720;
  // PONTIFICE_WINDOWED=LxA (ex.: 1920x1080) abre nesse tamanho, para screenshots em resolução
  // cheia sem tomar a tela.
  const char* env_windowed = std::getenv("PONTIFICE_WINDOWED");
  const bool windowed = env_windowed != nullptr;
  int window_w = 1280, window_h = 720;
  if (windowed) {
    int w = 0, h = 0;
    if (std::sscanf(env_windowed, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
      window_w = w;
      window_h = h;
    }
  }
  SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (!windowed) flags |= SDL_WINDOW_FULLSCREEN;
  app.window = SDL_CreateWindow("PONTIFICE", windowed ? window_w : kLogicalWidth,
                                windowed ? window_h : kLogicalHeight, flags);
  if (app.window == nullptr) {
    LOG("SDL_CreateWindow: %s", SDL_GetError());
    return false;
  }
  SetWindowIcon(app.window);
  if (!windowed) SDL_SetWindowFullscreenMode(app.window, nullptr);  // sem borda, resolução do monitor

  // Renderer 2D sobre o SDL_GPU (Metal no macOS, Vulkan/D3D12 em outros sistemas).
  // O device fica acessível via SDL_GetGPURendererDevice para o render 3D do campo.
  app.renderer = SDL_CreateGPURenderer(nullptr, app.window);
  if (app.renderer == nullptr) {
    LOG("SDL_CreateGPURenderer failed (%s), using the default renderer",
                 SDL_GetError());
    app.renderer = SDL_CreateRenderer(app.window, nullptr);
  }
  if (app.renderer == nullptr) {
    LOG("SDL_CreateRenderer: %s", SDL_GetError());
    return false;
  }

  SDL_SetRenderVSync(app.renderer, 1);
  SDL_SetRenderLogicalPresentation(app.renderer, kLogicalWidth, kLogicalHeight,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  LOG("Renderer: %s", SDL_GetRendererName(app.renderer));
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
      LOG("Palette not loaded (%s); colors fall back to magenta", palette.c_str());
    }
  }
  // Ajustes de cena (assets/light.json, fog.json, shadow.json): iluminação, névoa/nuvens e
  // sombras. Valor ausente cai no padrão do código, com aviso no log.
  for (const auto& [name, file] : {std::pair{"light", "light.json"}, std::pair{"fog", "fog.json"},
                                   std::pair{"shadow", "shadow.json"}}) {
    const std::string path = base + "assets/" + file;
    if (!config::Load(name, path)) {
      LOG("Settings not loaded (%s); using defaults", path.c_str());
    }
  }
  // Textos (assets/translate.json): pt, en e es. O idioma é escolhido na primeira cena;
  // PONTIFICE_LANG=en|es|pt (depuração) já define o idioma e pula essa cena.
  {
    const std::string translate = base + "assets/translate.json";
    if (!i18n::Load(translate)) {
      LOG("Translations not loaded (%s); texts fall back to keys",
                   translate.c_str());
    }
  }
  const char* env_lang = std::getenv("PONTIFICE_LANG");
  if (env_lang != nullptr) i18n::SetLanguage(env_lang);
  // Fontes do jogo (atlas de glifos no renderer): PirataOne nos títulos e rótulos, Newsreader
  // no texto corrido. Sem a de corpo, usa a de título; sem a de título, a fonte de depuração.
  text::Init(app.renderer, base + "assets/pirataOne.ttf", text::Face::kTitle);
  text::Init(app.renderer, base + "assets/newsreader.ttf", text::Face::kBody);

  SceneContext ctx;
  ctx.renderer = app.renderer;
  ctx.gpu_device = SDL_GetGPURendererDevice(app.renderer);  // nulo no renderer de fallback
  ctx.logical_width = kLogicalWidth;
  ctx.logical_height = kLogicalHeight;

  // Bloco próprio: as cenas (e seus recursos de GPU) precisam ser destruídas antes de
  // Shutdown(), que destrói o renderer, o device e encerra o SDL.
  {
  // Trilha sonora (assets/music.json) toca em loop durante todo o jogo, em qualquer cena.
  Music music;
  music.Init(base);
  ctx.music = &music;  // vive até o fim deste bloco, depois das cenas
  SceneManager scenes(std::move(ctx));

  // Depuração: PONTIFICE_SCENE=menu abre direto no menu (pulando a escolha de idioma),
  // =battle na batalha e =loading na tela de loading (que segue para a batalha);
  // PONTIFICE_MOUSE=x,y injeta um
  // movimento de mouse em coordenadas lógicas; PONTIFICE_SCREENSHOT=arquivo.bmp salva o
  // quadro PONTIFICE_SCREENSHOT_FRAME (padrão 60), ou o primeiro quadro depois de
  // PONTIFICE_SCREENSHOT_TIME segundos, e encerra.
  const char* env_scene = std::getenv("PONTIFICE_SCENE");
  const char* env_mouse = std::getenv("PONTIFICE_MOUSE");
  // PONTIFICE_DRAG: sequência de gestos separados por ';'. "x0,y0,x1,y1,t" = arrasto do botão
  // esquerdo de (x0,y0) a (x1,y1) começando em t s (pressiona, move em 0,3 s, solta em 0,6 s;
  // x0=x1,y0=y1 vira clique); "w,dy,t" = roda do mouse dy em t s; "d,x,y,t" = duplo clique.
  struct Gesture {
    bool wheel = false;
    bool double_click = false;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, t = 2.0f, dy = 0;
    int stage = 0;  // 0 nada, 1 pressionado, 2 movido, 3 feito
  };
  std::vector<Gesture> gestures;
  if (const char* env_drag = std::getenv("PONTIFICE_DRAG")) {
    std::string spec = env_drag;
    size_t pos = 0;
    while (pos <= spec.size()) {
      size_t end = spec.find(';', pos);
      if (end == std::string::npos) end = spec.size();
      const std::string item = spec.substr(pos, end - pos);
      Gesture g;
      if (item.rfind("w,", 0) == 0) {
        g.wheel = true;
        std::sscanf(item.c_str(), "w,%f,%f", &g.dy, &g.t);
      } else if (item.rfind("d,", 0) == 0) {
        g.double_click = true;
        std::sscanf(item.c_str(), "d,%f,%f,%f", &g.x0, &g.y0, &g.t);
        g.x1 = g.x0;
        g.y1 = g.y0;
      } else {
        std::sscanf(item.c_str(), "%f,%f,%f,%f,%f", &g.x0, &g.y0, &g.x1, &g.y1, &g.t);
      }
      if (!item.empty()) gestures.push_back(g);
      pos = end + 1;
    }
  }
  // PONTIFICE_KEYS: teclas separadas por ';', "nome,t" = aperta a tecla (nome do SDL, ex.:
  // Escape, Return, Left) em t s.
  struct KeyPress {
    SDL_Keycode key = SDLK_UNKNOWN;
    float t = 0.0f;
    bool done = false;
  };
  std::vector<KeyPress> key_presses;
  if (const char* env_keys = std::getenv("PONTIFICE_KEYS")) {
    std::string spec = env_keys;
    size_t pos = 0;
    while (pos < spec.size()) {
      size_t end = spec.find(';', pos);
      if (end == std::string::npos) end = spec.size();
      const std::string item = spec.substr(pos, end - pos);
      const size_t comma = item.find(',');
      if (comma != std::string::npos) {
        KeyPress k;
        k.key = SDL_GetKeyFromName(item.substr(0, comma).c_str());
        k.t = static_cast<float>(std::atof(item.c_str() + comma + 1));
        if (k.key != SDLK_UNKNOWN) key_presses.push_back(k);
      }
      pos = end + 1;
    }
  }
  const char* env_shot = std::getenv("PONTIFICE_SCREENSHOT");
  const char* env_shot_frame = std::getenv("PONTIFICE_SCREENSHOT_FRAME");
  const char* env_shot_time = std::getenv("PONTIFICE_SCREENSHOT_TIME");
  const int shot_frame = env_shot_frame != nullptr ? std::atoi(env_shot_frame) : 60;
  const double shot_time = env_shot_time != nullptr ? std::atof(env_shot_time) : -1.0;
  // Primeira cena: escolha de idioma (docs/idioma.md); com PONTIFICE_LANG, direto no menu.
  SceneId first = env_lang != nullptr ? SceneId::kMenu : SceneId::kLanguage;
  if (env_scene != nullptr && std::strcmp(env_scene, "language") == 0) first = SceneId::kLanguage;
  if (env_scene != nullptr && std::strcmp(env_scene, "menu") == 0) first = SceneId::kMenu;
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
    for (KeyPress& k : key_presses) {
      const double t = static_cast<double>(SDL_GetTicksNS() - start) / 1e9;
      if (k.done || t < k.t) continue;
      SDL_Event fake{};
      fake.type = SDL_EVENT_KEY_DOWN;
      fake.key.key = k.key;
      scenes.HandleEvent(fake);
      k.done = true;
    }
    for (Gesture& g : gestures) {
      const double t = static_cast<double>(SDL_GetTicksNS() - start) / 1e9;
      SDL_Event fake{};
      if (g.wheel) {
        if (g.stage == 0 && t >= g.t) {
          fake.type = SDL_EVENT_MOUSE_WHEEL;
          fake.wheel.y = g.dy;
          scenes.HandleEvent(fake);
          g.stage = 3;
        }
        continue;
      }
      if (g.stage == 0 && t >= g.t) {
        fake.type = SDL_EVENT_MOUSE_MOTION;
        fake.motion.x = g.x0;
        fake.motion.y = g.y0;
        scenes.HandleEvent(fake);
        fake.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        fake.button.button = SDL_BUTTON_LEFT;
        fake.button.clicks = g.double_click ? 2 : 1;
        fake.button.x = g.x0;
        fake.button.y = g.y0;
        scenes.HandleEvent(fake);
        if (g.double_click) {
          fake.type = SDL_EVENT_MOUSE_BUTTON_UP;
          scenes.HandleEvent(fake);
          g.stage = 3;
          continue;
        }
        g.stage = 1;
      } else if (g.stage == 1 && t >= g.t + 0.3) {
        fake.type = SDL_EVENT_MOUSE_MOTION;
        fake.motion.x = g.x1;
        fake.motion.y = g.y1;
        scenes.HandleEvent(fake);
        g.stage = 2;
      } else if (g.stage == 2 && t >= g.t + 0.6) {
        fake.type = SDL_EVENT_MOUSE_BUTTON_UP;
        fake.button.button = SDL_BUTTON_LEFT;
        fake.button.clicks = 1;
        fake.button.x = g.x1;
        fake.button.y = g.y1;
        scenes.HandleEvent(fake);
        g.stage = 3;
      }
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
        LOG("Screenshot saved to %s", env_shot);
      } else {
        LOG("SDL_RenderReadPixels: %s", SDL_GetError());
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
