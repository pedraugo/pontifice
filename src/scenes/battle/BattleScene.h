#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/gpu/Gpu.h"
#include "scripts/hand/Hand.h"
#include "scripts/math/Math.h"
#include "scripts/model/Model.h"

// Cena de batalha. Por enquanto: mão de cartas 3D com hover. Esc volta ao menu.
class BattleScene : public Scene {
 public:
  ~BattleScene() override;

  void OnEnter(SceneContext& ctx) override;
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;

 private:
  SDL_GPUTexture* LoadSprite(const char* file);

  std::unique_ptr<Gpu> gpu_;
  std::unique_ptr<Model> card_model_;
  std::unique_ptr<Model> arena_model_;
  std::vector<SDL_GPUTexture*> textures_;
  Hand hand_;
  math::Mat4 hand_view_proj_;   // câmera fixa da mão (dock)
  math::Mat4 arena_view_proj_;  // câmera da arena, vista do jogador
  math::Mat4 arena_world_;
  math::Vec2 mouse_{-1, -1};
  bool ready_ = false;
};
