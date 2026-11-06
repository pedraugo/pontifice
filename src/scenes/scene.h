#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <optional>

class Music;

enum class SceneId { kLanguage, kMenu, kLoading, kBattle, kGrimoire };
constexpr int kDefaultDifficulty = 1;

class Scene;

struct PreloadInfo {
  SDL_GPUDevice* gpu_device = nullptr;
  int logical_width = 0;
  int logical_height = 0;
  int difficulty = kDefaultDifficulty;
};

struct SceneContext {
  SDL_Renderer* renderer = nullptr;
  Music* music = nullptr;
  SDL_GPUDevice* gpu_device = nullptr;
  int logical_width = 0;
  int logical_height = 0;

  int difficulty = kDefaultDifficulty;
  PreloadInfo Preload() const {
    return {gpu_device, logical_width, logical_height, difficulty};
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
  virtual void Render(SceneContext& ctx) = 0;
};
