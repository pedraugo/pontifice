#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <optional>

#include "scripts/rules/rules.h"
#include "scripts/save/save.h"

class Music;

enum class SceneId { kLanguage, kMenu, kLoading, kBattle, kGrimoire };
// kMain: Play, Language and Exit; kNewSave: name form; kSave: Battle, Level, Grimoire and Exit.
enum class MenuPage { kMain, kNewSave, kSave };

class Scene;

struct PreloadInfo {
  SDL_GPUDevice* gpu_device = nullptr;
  int logical_width = 0;
  int logical_height = 0;
  int difficulty = rules::Get().default_difficulty;
  std::optional<save::Data> save;  // copy for the Preload thread
};

struct SceneContext {
  SDL_Renderer* renderer = nullptr;
  Music* music = nullptr;
  SDL_GPUDevice* gpu_device = nullptr;
  int logical_width = 0;
  int logical_height = 0;

  int difficulty = rules::Get().default_difficulty;
  std::optional<save::Data> save;
  MenuPage menu_page = MenuPage::kMain;  // page the menu reopens on
  bool language_from_menu = false;  // Esc in the language scene returns to the menu
  PreloadInfo Preload() const {
    return {gpu_device, logical_width, logical_height, difficulty, save};
  }
  bool quit = false;
  std::optional<SceneId> next_scene;
  SceneId loading_target = SceneId::kBattle;  // scene the loading screen preloads
  std::unique_ptr<Scene> preloaded;  // already loaded, OnEnter called by loading
  float fade_in_seconds = 0.0f;
  float fade_out_seconds = 0.0f;
};

class Scene {
 public:
  virtual ~Scene() = default;

  virtual void Preload(const PreloadInfo&) {}
  virtual void OnEnter(SceneContext&) {}
  virtual void OnExit(SceneContext&) {}
  virtual void HandleEvent(SceneContext& ctx, const SDL_Event& event) = 0;
  virtual void Update(SceneContext&, float) {}
  // Layers, back to front: Render (game), the vignette, RenderHud (interface).
  virtual void Render(SceneContext& ctx) = 0;
  virtual void RenderHud(SceneContext&) {}
};
