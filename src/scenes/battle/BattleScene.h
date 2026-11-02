#pragma once

#include <SDL3/SDL.h>

#include <deque>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "scenes/Scene.h"
#include "scripts/ai/Ai.h"
#include "scripts/board/Board.h"
#include "scripts/button/ButtonAction.h"
#include "scripts/cards/Cards.h"
#include "scripts/deck/Deck.h"
#include "scripts/gpu/Gpu.h"
#include "scripts/hand/Hand.h"
#include "scripts/hud/Hud.h"
#include "scripts/math/Math.h"
#include "scripts/model/Model.h"
#include "scripts/rules/Rules.h"
#include "scripts/soundbutton/SoundButton.h"
#include "scripts/sword/Sword.h"
#include "scripts/torch/Torch.h"

// Cena de batalha: você contra o computador (docs/batalha.md, docs/campo.md).
//
// Mesa: arena 3D com as duas plataformas (Board do jogador na frente, do oponente no fundo),
// mão de cartas 3D em dock, HUD (docs/hud.md). Sorteio decide quem começa; quem começa não
// compra nem ataca no primeiro turno.
//
// Seu turno: arrastar uma carta da mão para cima a seleciona; clicar num slot do seu lado a
// invoca virada para cima em ataque (uma invocação por turno; num slot ocupado a carta
// anterior vai para o cemitério). Carta de campo selecionada: clicar na arena a ativa
// (substitui a anterior). Clicar numa carta sua no campo alterna ataque <-> defesa.
// Arrastar uma carta sua em ataque até uma carta do oponente a ataca; até o lado
// vazio dele, ataque direto. "Passar turno" encerra (com mais de 6 cartas na mão, as de menor
// ataque são descartadas).
//
// Turno do computador: a câmera orbita 180° para o lado dele, a mão se recolhe e a IA
// (scripts/ai) joga em passos com pausas: compra, troca posições, carta de campo, invoca e
// ataca; depois a câmera volta e seu turno começa com a compra. Pontos de vida em 0 encerram.
//
// Esc sai da inspeção ou volta ao menu. Teclas de depuração: F alterna uma carta de campo de
// exemplo; 1 e 2 tiram 700 pontos de vida do oponente e de você; D compra; G soma 1 ao
// cemitério. PONTIFICE_STARTER=player|enemy força quem começa.
class BattleScene : public Scene {
 public:
  ~BattleScene() override;

  void Preload(const PreloadInfo& info) override;
  void OnEnter(SceneContext& ctx) override;
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;

 private:
  enum class Side { kPlayer, kEnemy };
  static int Index(Side side) { return side == Side::kPlayer ? 0 : 1; }
  static Side Other(Side side) { return side == Side::kPlayer ? Side::kEnemy : Side::kPlayer; }

  // Frente pronta para a mão/campo: textura composta, se é foil (hierarquia 1) e os dados.
  struct Face {
    SDL_GPUTexture* texture = nullptr;
    bool foil = false;
    const CardInfo* info = nullptr;
  };

  SDL_GPUTexture* UploadSprite(const Image& image);
  SDL_Texture* UploadHudSprite(SDL_Renderer* renderer, const Image& image);
  Face MakeCardFace(const std::string& code);
  // Frente da carta como SDL_Texture (renderer 2D), para o HUD e o tooltip; composta uma vez
  // por código e guardada.
  SDL_Texture* HudFace(const CardInfo& card);
  void ToggleSampleFieldCard();
  void DrawCard();  // você compra do monte para a mão
  void UpdateIntro(float dt_seconds);
  void UpdateCamera();

  // Agenda de passos com pausa (turno do computador, resolução de ataques).
  void Schedule(float delay, std::function<void()> fn);
  bool Busy() const;  // passos pendentes, animações no campo ou câmera orbitando

  // Fluxo da partida.
  void StartMatch();
  void BeginPlayerTurn();
  void EndTurn();  // botão "Passar turno"
  void RunEnemyTurn();
  void EnemyAttackStep();
  void FinishEnemyTurn();
  bool AttacksAllowed(Side side) const;  // quem começou não ataca no primeiro turno

  // Ações do jogador.
  bool CanAct() const;     // seu turno, sem inspeção, sem passos pendentes
  bool CanSummon() const;  // CanAct, carta selecionada, invocação ainda disponível
  bool CandidateIsField() const;  // a carta selecionada é de campo
  void Summon(int slot);   // carta selecionada entra no slot (ou ativa a carta de campo)
  void PlayField(Side owner, const CardInfo& card);

  // Combate: investida e, 0,25 s depois, dano e destruição (docs/batalha.md).
  Board& BoardOf(Side side) { return side == Side::kPlayer ? board_ : enemy_board_; }
  void PerformAttack(Side attacker, int slot, int target_slot);  // target -1 = direto
  void ResolveAttack(Side attacker, int slot, int target_slot);
  void Destroy(Side owner, int slot);
  void ApplyDamage(Side side, int damage);

  // Computador.
  ai::View BuildEnemyView() const;
  void EnemyDraw();
  void EnemySummon(int hand_index, bool defense);
  void DiscardExtraCards();  // fim de turno: mão com mais de 6 cartas

  // Projeção de um ponto do mundo na tela (câmera da arena).
  math::Vec2 ScreenOf(const math::Vec3& world, int width, int height) const;
  // Tooltip da carta sob o mouse no campo: nome, ataque e defesa efetivos, posição e dono.
  void RenderTooltip(SDL_Renderer* r, int width, int height) const;

  bool preloaded_ = false;
  Image hud_field_;
  std::unique_ptr<Gpu> gpu_;
  std::unique_ptr<Model> card_model_;
  std::unique_ptr<Model> arena_model_;
  std::unique_ptr<Torch> torch_;
  math::Mat4 torch_world_[2];
  math::Mat4 torch_light_vp_[2];
  std::vector<SDL_GPUTexture*> textures_;
  SDL_GPUTexture* back_ = nullptr;
  SDL_GPUTexture* front_normal_ = nullptr;
  SDL_GPUTexture* back_normal_ = nullptr;
  Image frames_[3];
  std::vector<CardInfo> cards_;
  std::mt19937 rng_;
  deck::Deck deck_;        // seu monte
  deck::Deck enemy_deck_;  // monte do computador
  std::vector<const CardInfo*> enemy_hand_;
  std::vector<Face> initial_hand_;
  Hand hand_;
  Board board_;        // seu lado do campo
  Board enemy_board_;  // lado do computador
  Sword sword_;        // efeito de golpe nos ataques
  Hud hud_;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* field_front_2d_ = nullptr;
  std::unordered_map<std::string, SDL_Texture*> hud_faces_;  // frentes no HUD por código
  std::unique_ptr<ButtonAction> end_turn_;
  SoundButton sound_button_;  // canto inferior esquerdo: liga/desliga a música

  // Partida.
  bool started_ = false;             // sorteio feito, primeiro turno começou
  Side starter_ = Side::kPlayer;
  Side turn_side_ = Side::kPlayer;
  bool player_turn_ = false;         // seu turno aberto para ações
  int turns_played_[2] = {0, 0};
  int turn_ = 1;                     // número da rodada (turnos seus)
  bool summoned_ = false;            // invocação normal já usada no seu turno
  rules::Field field_;               // carta de campo ativa
  Side field_owner_ = Side::kPlayer;
  ai::Difficulty difficulty_ = ai::Difficulty::kMedium;
  std::deque<std::pair<float, std::function<void()>>> steps_;
  float step_timer_ = 0.0f;
  int tooltip_slot_ = -1;    // carta no campo sob o mouse (tooltip); -1 nenhuma
  Side tooltip_side_ = Side::kPlayer;
  int attack_from_ = -1;     // carta sua pressionada para atacar (arrasto)
  bool attack_drag_ = false;
  math::Vec2 press_{-1, -1};

  // Câmera.
  math::Mat4 hand_view_proj_;
  math::Mat4 arena_view_proj_;
  math::Mat4 arena_world_;
  math::Mat4 proj_;
  math::Vec3 eye_final_;
  float orbit_ = 0.0f;         // 0 = vista do jogador, 1 = do oponente (animado)
  float orbit_target_ = 0.0f;
  float intro_time_ = 0.0f;
  float intro_ = 0.0f;
  float torch_intro_ = 0.0f;
  float ignite_ = 0.0f;
  GpuMesh mist_quad_;
  Atmosphere arena_atmosphere_;
  SDL_FColor fog_color_{};
  SDL_FColor mist_color_{};
  Atmosphere hand_atmosphere_;
  math::Vec2 mouse_{-1, -1};
  bool ready_ = false;
};
