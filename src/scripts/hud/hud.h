#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "scripts/rules/rules.h"

enum class Category { kGood, kEvil };

struct FieldCardInfo {
  std::string name;
  Category favored = Category::kGood;
  int buff = 0;
  int nerf = 0;
  SDL_Texture* image = nullptr;
  SDL_Texture* art = nullptr;
  SDL_Texture* art_large = nullptr;
};

// Match numbers for the result screen; index 0 = player, 1 = opponent.
struct MatchStats {
  float seconds = 0.0f;
  int rounds = 0;
  int drawn[2] = {0, 0};
  int played[2] = {0, 0};  // from hand to the field (summons and field cards)
  int graveyard[2] = {0, 0};
  int attacks[2] = {0, 0};
  int direct_attacks[2] = {0, 0};
  int destroyed[2] = {0, 0};  // cards of the other side destroyed by this side
  int damage[2] = {0, 0};  // life taken from the other side
  int biggest_hit[2] = {0, 0};
};

class Hud {
 public:
  void SetLife(int opponent, int player);
  int OpponentLife() const { return opponent_life_; }
  int PlayerLife() const { return player_life_; }
  bool Over() const { return opponent_life_ <= 0 || player_life_ <= 0; }

  void SetFieldCard(const FieldCardInfo& card);
  void ClearFieldCard();
  bool HasFieldCard() const { return has_field_; }

  void SetDeck(int used, int remaining);  // used + remaining == rules deck_size

  void SetGraveyard(int count);
  int Graveyard() const { return graveyard_; }

  void SetStats(const MatchStats& stats) { stats_ = stats; }

  void Update(float dt, float mouse_x, float mouse_y);  // mouse in logical px

  void Render(SDL_Renderer* r, int width, int height) const;

 private:
  struct FieldLayout {
    SDL_FRect panel{};
    SDL_FRect thumb{};
    bool art = false;
    std::string buff, nerf;
  };
  FieldLayout LayoutField() const;
  SDL_FRect ZoomRect() const;
  void RenderFieldPanel(SDL_Renderer* r) const;
  void RenderFieldZoom(SDL_Renderer* r) const;
  void RenderLifePanel(SDL_Renderer* r, int width) const;
  void RenderPilesPanel(SDL_Renderer* r, int width, int height) const;
  void RenderResult(SDL_Renderer* r, int width, int height) const;
  void RenderDamagePopups(SDL_Renderer* r, int width) const;
  void OnDamage(bool player, int amount);

  struct DamagePopup {
    bool player = false;
    int amount = 0;
    float time = 0.0f;
    float drift = -1.0f;  // horizontal direction; alternates per popup
  };
  std::vector<DamagePopup> popups_;
  float life_shake_ = 0.0f;
  float life_shake_time_ = 0.0f;
  float life_flash_[2] = {0.0f, 0.0f};

  int opponent_life_ = rules::Get().max_life;
  int player_life_ = rules::Get().max_life;
  FieldCardInfo field_;
  bool has_field_ = false;
  int deck_used_ = 0;
  int deck_remaining_ = rules::Get().deck_size;
  int graveyard_ = 0;
  MatchStats stats_;
  float zoom_t_ = 0.0f;
  bool zoom_hover_ = false;
};
