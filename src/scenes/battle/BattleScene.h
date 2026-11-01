#pragma once

#include <SDL3/SDL.h>

#include <memory>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/gpu/Gpu.h"
#include "scripts/hand/Hand.h"
#include "scripts/hud/Hud.h"
#include "scripts/math/Math.h"
#include "scripts/model/Model.h"

// Cena de batalha. Por enquanto: arena, mão de cartas 3D com hover e HUD (docs/hud.md).
// Esc volta ao menu. Teclas de depuração até existir a lógica de jogo: F alterna uma carta de
// campo de exemplo; 1 e 2 tiram 700 pontos de vida do oponente e de você; D compra uma carta
// do monte; G manda uma carta para o cemitério.
class BattleScene : public Scene {
 public:
  ~BattleScene() override;

  // Carga pesada (sprites, modelos e envio à GPU); roda em outra thread pela tela de loading.
  void Preload(const PreloadInfo& info) override;
  void OnEnter(SceneContext& ctx) override;
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;

 private:
  SDL_GPUTexture* UploadSprite(const Image& image);
  // Textura do renderer 2D (HUD) a partir da mesma imagem de sprite.
  SDL_Texture* UploadHudSprite(SDL_Renderer* renderer, const Image& image);
  void ToggleSampleFieldCard();
  void DrawCard();

  bool preloaded_ = false;
  Image hud_field_;  // frente da carta de campo decodificada para o HUD (renderer 2D)
  std::unique_ptr<Gpu> gpu_;
  std::unique_ptr<Model> card_model_;
  std::unique_ptr<Model> arena_model_;
  std::vector<SDL_GPUTexture*> textures_;
  SDL_GPUTexture* back_ = nullptr;       // verso das cartas
  std::vector<SDL_GPUTexture*> fronts_;  // frentes: Good, Evil, Field (compra de exemplo)
  Hand hand_;
  Hud hud_;
  SDL_Texture* field_front_2d_ = nullptr;  // frente da carta de campo no HUD
  math::Mat4 hand_view_proj_;   // câmera fixa da mão (dock)
  math::Mat4 arena_view_proj_;  // câmera da arena, vista do jogador
  math::Mat4 arena_world_;
  GpuMesh mist_quad_;           // quad horizontal reusado nas camadas de névoa
  Atmosphere arena_atmosphere_;
  SDL_FColor fog_color_{};      // background.fog: fundo e névoa (colors.json)
  SDL_FColor mist_color_{};     // background.mist: camadas de névoa na base
  Atmosphere hand_atmosphere_;
  math::Vec2 mouse_{-1, -1};
  bool ready_ = false;
};
