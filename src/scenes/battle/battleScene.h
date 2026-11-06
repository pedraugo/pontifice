#pragma once

#include <SDL3/SDL.h>

#include <deque>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "scenes/scene.h"
#include "scripts/ai/ai.h"
#include "scripts/audios/sfx.h"
#include "scripts/aura/cardAura.h"
#include "scripts/board/board.h"
#include "scripts/button/button.h"
#include "scripts/button/buttonIcon.h"
#include "scripts/cards/cards.h"
#include "scripts/deck/deck.h"
#include "scripts/dialog/confirmDialog.h"
#include "scripts/gpu/gpu.h"
#include "scripts/hand/hand.h"
#include "scripts/hud/hud.h"
#include "scripts/math/math.h"
#include "scripts/model/model.h"
#include "scripts/rules/rules.h"
#include "scripts/shield/shield.h"
#include "scripts/sword/sword.h"
#include "scripts/torch/torch.h"

class BattleScene : public Scene {
 public:
  ~BattleScene() override;

  void Preload(const PreloadInfo& info) override;
  void OnEnter(SceneContext& ctx) override;
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;

 private:
  enum class Side { kPlayer, kOpponent };
  static int Index(Side side) { return side == Side::kPlayer ? 0 : 1; }
  static Side Other(Side side) { return side == Side::kPlayer ? Side::kOpponent : Side::kPlayer; }

  struct Face {
    SDL_GPUTexture* texture = nullptr;
    bool foil = false;
    const CardInfo* info = nullptr;
  };

  SDL_GPUTexture* UploadSprite(const Image& image);
  SDL_Texture* UploadHudSprite(SDL_Renderer* renderer, const Image& image);
  Face MakeCardFace(const std::string& code);

 public:
  // Public: LoadMistLayers builds the list outside the class.
  struct MistLayer {
    float y;
    float alpha;
  };

 private:
  SDL_Texture* HudFace(const CardInfo& card);
  void ToggleSampleFieldCard();
  void DrawCard();
  void UpdateIntro(float dt_seconds);
  void UpdateCamera();

  void Schedule(float delay, std::function<void()> fn);
  void ScheduleNext(float delay, std::function<void()> fn);
  bool Busy() const;

  void StartMatch();
  void BeginPlayerTurn();
  void EndTurn();
  void RunOpponentTurn();
  void OpponentAttackStep();
  void FinishOpponentTurn();
  bool AttacksAllowed(Side side) const;

  bool CanAct() const;
  bool CanSummon() const;
  bool DraggedIsField() const;
  const char* HoverRole() const;
  void Summon(int slot);
  void PlayField(Side owner, const CardInfo& card);

  Board& BoardOf(Side side) { return side == Side::kPlayer ? board_ : opponent_board_; }
  void PerformAttack(Side attacker, int slot, int target_slot);
  void ResolveAttack(Side attacker, int slot, int target_slot);
  void Destroy(Side owner, int slot);
  void ApplyDamage(Side side, int damage);

  ai::View BuildOpponentView() const;
  void OpponentDraw();
  void OpponentSummon(const CardInfo* info, bool defense, const Face* face = nullptr);

  struct Reveal {
    const CardInfo* card = nullptr;
    SDL_Texture* face = nullptr;
    SDL_GPUTexture* front3d = nullptr;
    bool foil = false;
    bool from_hand = false;
    math::Vec3 from_pos;
    float from_scale = 1.0f;
    float from_roll = 0.0f;
    std::string caption;
    std::string detail;
    float time = 0.0f;
  };
  Reveal reveal_;
  void StartReveal(const CardInfo& card, std::string caption, std::string detail,
                   const math::Mat4* from_world = nullptr);
  float RevealDuration() const;
  void RenderReveal(SDL_Renderer* r, int width, int height) const;
  void RenderReveal3D(int width, int height);
  GpuMesh veil_quad_;
  void DiscardExtraCards();

  math::Vec2 ScreenOf(const math::Vec3& world, int width, int height) const;
  void RenderTooltip(SDL_Renderer* r, int width, int height) const;
  std::string Hint(int width, int height) const;

  bool preloaded_ = false;
  Image hud_field_;
  std::unique_ptr<Gpu> gpu_;
  std::unique_ptr<Model> card_model_;
  CardAura card_aura_;
  std::unique_ptr<Model> arena_model_;
  std::unique_ptr<Torch> torch_;
  math::Mat4 torch_world_[2];
  math::Mat4 torch_light_vp_[2];
  std::vector<SDL_GPUTexture*> textures_;
  SDL_GPUTexture* back_ = nullptr;
  SDL_GPUTexture* front_normal_ = nullptr;
  SDL_GPUTexture* back_normal_ = nullptr;
  Image frames_[4];
  const Image& FrameFor(const CardInfo& card) const;
  std::vector<CardInfo> cards_;
  std::mt19937 rng_;
  deck::Deck deck_;
  deck::Deck opponent_deck_;
  std::vector<const CardInfo*> opponent_hand_;
  std::vector<Face> initial_hand_;
  Hand hand_;
  Hand opponent_hand_view_;
  SDL_GPUTexture* hidden_face_ = nullptr;
  void OpponentHandAdd(const CardInfo* info);
  void OpponentHandRemove(size_t index, math::Mat4* world = nullptr);
  Board board_;
  Board opponent_board_;
  Sword sword_;
  Shield shield_;
  Sfx sfx_;
  std::vector<std::pair<float, std::string>> sfx_queue_;  // (seconds left, effect)
  void PlaySfxLater(float delay, std::string name);
  Hud hud_;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* field_front_2d_ = nullptr;
  std::unordered_map<std::string, SDL_Texture*> hud_faces_;
  std::unordered_map<std::string, Image> arts_;
  const Image* ArtFor(const CardInfo& card) const;
  std::unordered_map<std::string, SDL_Texture*> hud_arts_;
  SDL_Texture* HudArt(const CardInfo& card, bool large = false);
  std::unique_ptr<Button> end_turn_;
  ButtonIcon sound_button_{ButtonIcon::Icon::kSound};
  ButtonIcon hint_button_{ButtonIcon::Icon::kHint};
  bool hints_enabled_ = true;
  ConfirmDialog abandon_dialog_;
  bool paused_ = false;
  void Pause(SceneContext& ctx);
  void Resume(SceneContext& ctx);

  bool started_ = false;  // coin toss done, first turn started
  Side starter_ = Side::kPlayer;
  Side turn_side_ = Side::kPlayer;
  bool player_turn_ = false;
  int turns_played_[2] = {0, 0};
  int turn_ = 1;  // round number: counts your turns
  bool summoned_ = false;  // normal summon already used this turn
  rules::Field field_;
  Side field_owner_ = Side::kPlayer;
  ai::Difficulty difficulty_ = ai::Difficulty::kMedium;
  std::deque<std::pair<float, std::function<void()>>> steps_;  // queued (delay, step) pairs
  float step_timer_ = 0.0f;
  int tooltip_slot_ = -1;
  Side tooltip_side_ = Side::kPlayer;
  int attack_from_ = -1;  // your card pressed to attack; -1 none
  bool attack_drag_ = false;
  math::Vec2 press_{-1, -1};

  math::Mat4 hand_view_proj_;
  math::Mat4 arena_view_proj_;
  math::Mat4 arena_world_;
  math::Mat4 proj_;
  math::Vec3 eye_final_;
  float orbit_ = 0.0f;  // 0 = player view, 1 = opponent view
  float shake_left_ = 0.0f;
  float shake_time_ = 0.0f;
  float orbit_target_ = 0.0f;
  float intro_time_ = 0.0f;
  // Intro progress 0 to 1: arena, torches and flame.
  float intro_ = 0.0f;
  float torch_intro_ = 0.0f;
  float ignite_ = 0.0f;
  GpuMesh mist_quad_;
  Atmosphere arena_atmosphere_;
  SDL_FColor fog_color_{};
  SDL_FColor mist_color_{};
  float field_glow_ = 0.0f;  // 0 normal, 1 golden arena of the field card
  SDL_FColor field_mist_color_{};
  SDL_FColor field_fog_color_{};
  std::vector<MistLayer> mist_layers_;
  std::vector<MistLayer> intro_mist_layers_;
  float torch_light_ = 2.6f;
  math::Mat4 shadow_proj_;
  math::Vec3 shadow_target_{0.0f, 0.3f, 0.0f};
  Atmosphere hand_atmosphere_;
  math::Vec2 mouse_{-1, -1};
  bool ready_ = false;
};
