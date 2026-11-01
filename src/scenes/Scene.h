#pragma once

#include <SDL3/SDL.h>

#include <optional>

enum class SceneId { kMenu, kBattle };

// Estado compartilhado entre o loop principal e a cena ativa.
// A cena pede troca de cena ou saída do jogo escrevendo aqui.
struct SceneContext {
  SDL_Renderer* renderer = nullptr;
  int logical_width = 0;
  int logical_height = 0;
  bool quit = false;
  std::optional<SceneId> next_scene;
};

class Scene {
 public:
  virtual ~Scene() = default;

  virtual void OnEnter(SceneContext& /*ctx*/) {}
  virtual void OnExit(SceneContext& /*ctx*/) {}
  virtual void HandleEvent(SceneContext& ctx, const SDL_Event& event) = 0;
  virtual void Update(SceneContext& /*ctx*/, float /*dt_seconds*/) {}
  virtual void Render(SceneContext& ctx) = 0;
};
