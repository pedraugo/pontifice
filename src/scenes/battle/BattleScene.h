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
#include "scripts/torch/Torch.h"
#include "scripts/cards/Cards.h"
#include "scripts/deck/Deck.h"

// Cena de batalha. Por enquanto: arena, mão de cartas 3D com hover, seleção (arrastar para
// cima) e inspeção (arrastar para baixo) e HUD (docs/hud.md). Esc sai da inspeção ou volta
// ao menu. Teclas de depuração até existir a lógica de jogo: F alterna uma carta de
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
  // Compõe a frente (moldura + textos) e sobe para a GPU; nulo se a carta não existe.
  // Frente pronta para a mão: textura composta e se é foil (hierarquia 1).
  struct Face {
    SDL_GPUTexture* texture = nullptr;
    bool foil = false;
  };
  Face MakeCardFace(const std::string& code);
  void UpdateIntro(float dt_seconds);  // arena emergindo da névoa e câmera aproximando

  bool preloaded_ = false;
  Image hud_field_;  // frente da carta de campo decodificada para o HUD (renderer 2D)
  std::unique_ptr<Gpu> gpu_;
  std::unique_ptr<Model> card_model_;
  std::unique_ptr<Model> arena_model_;
  std::unique_ptr<Torch> torch_;
  math::Mat4 torch_world_[2];  // tochas fincadas à esquerda e à direita da arena
  math::Mat4 torch_light_vp_[2];  // câmera de sombra de cada chama (perspectiva ampla)
  std::vector<SDL_GPUTexture*> textures_;
  SDL_GPUTexture* back_ = nullptr;       // verso das cartas
  SDL_GPUTexture* front_normal_ = nullptr;  // normal maps da moldura (frente e verso)
  SDL_GPUTexture* back_normal_ = nullptr;
  Image frames_[3];                      // molduras Good, Evil, Field reduzidas (cardface)
  std::vector<CardInfo> cards_;          // todas as cartas (assets/Cards*.json)
  deck::Deck deck_;                      // monte do jogador, já embaralhado
  std::vector<Face> initial_hand_;  // frentes das 5 primeiras, compostas no Preload
  Hand hand_;
  Hud hud_;
  SDL_Texture* field_front_2d_ = nullptr;  // frente da carta de campo no HUD
  math::Mat4 hand_view_proj_;   // câmera fixa da mão (dock)
  math::Mat4 arena_view_proj_;  // câmera da arena, vista do jogador
  math::Mat4 arena_world_;
  math::Mat4 proj_;             // projeção das câmeras 3D
  math::Vec3 eye_final_;        // posição final da câmera da arena
  float intro_time_ = 0.0f;     // segundos desde a entrada (animação de abertura)
  float intro_ = 0.0f;          // progresso suavizado da abertura, 0..1
  float torch_intro_ = 0.0f;    // idem para as tochas, que emergem depois da arena
  float ignite_ = 0.0f;         // fogo das tochas, que acende depois de elas surgirem
  GpuMesh mist_quad_;           // quad horizontal reusado nas camadas de névoa
  Atmosphere arena_atmosphere_;
  SDL_FColor fog_color_{};      // background.fog: fundo e névoa (colors.json)
  SDL_FColor mist_color_{};     // background.mist: camadas de névoa na base
  Atmosphere hand_atmosphere_;
  math::Vec2 mouse_{-1, -1};
  bool ready_ = false;
};
