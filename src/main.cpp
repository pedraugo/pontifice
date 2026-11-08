#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "scenes/scene.h"
#include "scenes/sceneManager.h"
#include "scripts/colors/colors.h"
#include "scripts/opacity/opacity.h"
#include "scripts/config/config.h"
#include "scripts/rules/rules.h"
#include "scripts/save/save.h"
#include "scripts/image/image.h"
#include "scripts/i18n/i18n.h"
#include "scripts/audios/music.h"
#include "scripts/text/text.h"
#include "scripts/ui/ui.h"
#include "scripts/vignette/vignette.h"
#include "scripts/log/log.h"

namespace {

struct App {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
};

// Window and Dock icon; the executable icon comes from src/icons (.rc on Windows).
void SetWindowIcon(SDL_Window* window) {
  const char* base = SDL_GetBasePath();
  const std::string path = std::string(base != nullptr ? base : "./") + "icons/icon.png";
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
  // Outside macOS 1024 px is not needed; X11 copies the whole icon into a window property.
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

  // Borderless fullscreen by default; PONTIFICE_WINDOWED=1 or WxH opens a debug window instead.
  const char* env_windowed = std::getenv("PONTIFICE_WINDOWED");
  const bool windowed = env_windowed != nullptr;
  int window_w = static_cast<int>(ui::Number("screen.windowedWidth", 1280.0f));
  int window_h = static_cast<int>(ui::Number("screen.windowedHeight", 720.0f));
  if (windowed) {
    int w = 0, h = 0;
    if (std::sscanf(env_windowed, "%dx%d", &w, &h) == 2 && w > 0 && h > 0) {
      window_w = w;
      window_h = h;
    }
  }
  SDL_WindowFlags flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
  if (!windowed) flags |= SDL_WINDOW_FULLSCREEN;
  app.window = SDL_CreateWindow("PONTIFICE", windowed ? window_w : ui::Width(),
                                windowed ? window_h : ui::Height(), flags);
  if (app.window == nullptr) {
    LOG("SDL_CreateWindow: %s", SDL_GetError());
    return false;
  }
  SetWindowIcon(app.window);
  if (!windowed) SDL_SetWindowFullscreenMode(app.window, nullptr);

  // 2D renderer on SDL_GPU; its device also draws the 3D scenes.
  app.renderer = SDL_CreateGPURenderer(nullptr, app.window);
  if (app.renderer == nullptr) {
    LOG("SDL_CreateGPURenderer failed (%s), using the default renderer", SDL_GetError());
    app.renderer = SDL_CreateRenderer(app.window, nullptr);
  }
  if (app.renderer == nullptr) {
    LOG("SDL_CreateRenderer: %s", SDL_GetError());
    return false;
  }

  SDL_SetRenderVSync(app.renderer, 1);
  SDL_SetRenderLogicalPresentation(app.renderer, ui::Width(), ui::Height(),
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  LOG("Renderer: %s", SDL_GetRendererName(app.renderer));
  return true;
}

// Runs `draw` in output pixels, outside the 1920x1080 logical presentation.
template <typename Draw>
void InOutputPixels(SDL_Renderer* r, Draw draw) {
  int w = 0, h = 0;
  SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
  SDL_GetRenderLogicalPresentation(r, &w, &h, &mode);
  SDL_SetRenderLogicalPresentation(r, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
  draw();
  SDL_SetRenderLogicalPresentation(r, w, h, mode);
}

// Black bars around the 16:9 area on screens of another aspect; scenes clear them to their color.
void DrawLetterbox(SDL_Renderer* r) {
  SDL_FRect view{};
  int w = 0, h = 0;
  if (!SDL_GetRenderLogicalPresentationRect(r, &view) || !SDL_GetRenderOutputSize(r, &w, &h)) {
    return;
  }
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  if (view.x <= 0.5f && view.y <= 0.5f && view.w >= fw - 0.5f && view.h >= fh - 0.5f) return;
  InOutputPixels(r, [&] {
    const SDL_FRect bars[4] = {{0, 0, fw, view.y},
                               {0, view.y + view.h, fw, fh - view.y - view.h},
                               {0, view.y, view.x, view.h},
                               {view.x + view.w, view.y, fw - view.x - view.w, view.h}};
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderFillRects(r, bars, 4);
  });
}

void Shutdown(App& app) {
  if (app.renderer != nullptr) SDL_DestroyRenderer(app.renderer);
  if (app.window != nullptr) SDL_DestroyWindow(app.window);
  SDL_Quit();
}

}  // namespace

int main(int /*argc*/, char** /*argv*/) {
  const char* base_path = SDL_GetBasePath();
  const std::string base = base_path != nullptr ? base_path : "./";

  {
    const std::string palette = base + "assets/colors.json";
    if (!colors::Load(palette)) {
      LOG("Palette not loaded (%s); colors fall back to magenta", palette.c_str());
    }
  }
  {
    const std::string opacities = base + "assets/opacity.json";
    if (!opacity::Load(opacities)) {
      LOG("Opacities not loaded (%s); everything stays opaque", opacities.c_str());
    }
  }
  // Settings by name; a missing value falls back to the code default with a log warning.
  for (const auto& [name, file] :
       {std::pair{"light", "light.json"}, std::pair{"fog", "fog.json"},
        std::pair{"shadow", "shadow.json"}, std::pair{"button", "button.json"},
        std::pair{"ui", "ui.json"}, std::pair{"layout", "layout.json"},
        std::pair{"text", "text.json"}}) {
    const std::string path = base + "assets/" + file;
    if (!config::Load(name, path)) {
      LOG("Settings not loaded (%s); using defaults", path.c_str());
    }
  }
  // Match rules (assets/rules.json): life, deck, hand and default opponent level.
  {
    const std::string path = base + "assets/rules.json";
    if (!config::Load("rules", path)) LOG("Rules not loaded (%s); using defaults", path.c_str());
    rules::Settings settings;
    settings.max_life = config::Int("rules", "maxLife", settings.max_life);
    settings.deck_size = config::Int("rules", "deckSize", settings.deck_size);
    settings.initial_hand = config::Int("rules", "initialHand", settings.initial_hand);
    settings.hand_limit = config::Int("rules", "handLimit", settings.hand_limit);
    settings.max_copies = config::Int("rules", "maxCopies", settings.max_copies);
    settings.max_copies_rarest =
        config::Int("rules", "maxCopiesRarest", settings.max_copies_rarest);
    settings.default_difficulty =
        config::Int("rules", "defaultDifficulty", settings.default_difficulty);
    settings.starter_hierarchy4 =
        config::Int("rules", "starterHierarchy4", settings.starter_hierarchy4);
    settings.starter_hierarchy3 =
        config::Int("rules", "starterHierarchy3", settings.starter_hierarchy3);
    settings.tutorial_opponent_cards =
        config::Int("rules", "tutorialOpponentCards", settings.tutorial_opponent_cards);
    rules::Set(settings);
  }
  {
    const std::string translate = base + "assets/translate.json";
    if (!i18n::Load(translate)) {
      LOG("Translations not loaded (%s); texts fall back to keys", translate.c_str());
    }
  }
  // Debug: PONTIFICE_LANG=en|es|pt sets the language and skips the language scene.
  const char* env_lang = std::getenv("PONTIFICE_LANG");
  // A save brings back the player's language, so the language scene is skipped too.
  std::optional<save::Data> saved = save::Load();
  const bool has_save = saved.has_value();
  if (env_lang != nullptr) {
    i18n::SetLanguage(env_lang);
  } else if (has_save && !saved->language.empty()) {
    i18n::SetLanguage(saved->language);
  }

  // Window and renderer after the settings: the resolution comes from assets/ui.json.
  App app;
  if (!Init(app)) {
    Shutdown(app);
    return 1;
  }
  // Title and body fonts; a missing one falls back to the other, then the debug font.
  text::Init(app.renderer, base + "fonts/pirataOne.ttf", text::Face::kTitle);
  text::Init(app.renderer, base + "fonts/newsreader.ttf", text::Face::kBody);

  SceneContext ctx;
  ctx.renderer = app.renderer;
  ctx.gpu_device = SDL_GetGPURendererDevice(app.renderer);  // null on the fallback renderer
  ctx.logical_width = ui::Width();
  ctx.logical_height = ui::Height();
  if (saved) ctx.difficulty = saved->difficulty;
  ctx.save = std::move(saved);

  // Own block: scenes and their GPU resources die before Shutdown destroys the renderer.
  {
  // Soundtrack loops through every scene.
  Music music;
  music.Init(base);
  ctx.music = &music;  // declared before the scenes, so it outlives them
  SceneManager scenes(std::move(ctx));

  // Debug: PONTIFICE_SCENE opens that scene first; PONTIFICE_MOUSE=x,y injects a mouse move.
  const char* env_scene = std::getenv("PONTIFICE_SCENE");
  const char* env_mouse = std::getenv("PONTIFICE_MOUSE");
  // Debug: PONTIFICE_DRAG lists drags "x0,y0,x1,y1,t", wheels "w,dy,t", double clicks "d,x,y,t".
  struct Gesture {
    bool wheel = false;
    bool double_click = false;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0, t = 2.0f, dy = 0;
    int stage = 0;  // 0 idle, 1 pressed, 2 moved, 3 done
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
  // Debug: PONTIFICE_KEYS is a ';' list of "name,t" SDL key presses at t seconds.
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
  // Debug: screenshot at PONTIFICE_SCREENSHOT_FRAME (60) or PONTIFICE_SCREENSHOT_TIME, then quit.
  const char* env_shot = std::getenv("PONTIFICE_SCREENSHOT");
  const char* env_shot_frame = std::getenv("PONTIFICE_SCREENSHOT_FRAME");
  const char* env_shot_time = std::getenv("PONTIFICE_SCREENSHOT_TIME");
  const int shot_frame = env_shot_frame != nullptr ? std::atoi(env_shot_frame) : 60;
  const double shot_time = env_shot_time != nullptr ? std::atof(env_shot_time) : -1.0;
  // First scene: language choice, or the menu when PONTIFICE_LANG is set or a save exists.
  SceneId first = env_lang != nullptr || has_save ? SceneId::kMenu : SceneId::kLanguage;
  if (env_scene != nullptr && std::strcmp(env_scene, "language") == 0) first = SceneId::kLanguage;
  if (env_scene != nullptr && std::strcmp(env_scene, "menu") == 0) first = SceneId::kMenu;
  if (env_scene != nullptr && std::strcmp(env_scene, "battle") == 0) first = SceneId::kBattle;
  if (env_scene != nullptr && std::strcmp(env_scene, "loading") == 0) first = SceneId::kLoading;
  if (env_scene != nullptr && std::strcmp(env_scene, "grimoire") == 0) first = SceneId::kGrimoire;
  scenes.SwitchTo(first);
  int frame = 0;

  const Uint64 start = SDL_GetTicksNS();  // after the first scene loads
  Uint64 last = start;
  while (!scenes.ShouldQuit()) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        scenes.RequestQuit();
        break;
      }
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
    DrawLetterbox(app.renderer);

    ++frame;
    const double elapsed = static_cast<double>(now - start) / 1e9;
    const bool shot_now = shot_time >= 0.0 ? elapsed >= shot_time : frame == shot_frame;
    if (env_shot != nullptr && shot_now) {
      // Whole output, letterbox bars included, as the player sees it.
      SDL_Surface* shot = nullptr;
      InOutputPixels(app.renderer, [&] { shot = SDL_RenderReadPixels(app.renderer, nullptr); });
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
  }  // scenes destroyed here

  text::Shutdown();  // font and vignette textures before the renderer goes away
  vignette::Shutdown();
  Shutdown(app);
  return 0;
}
