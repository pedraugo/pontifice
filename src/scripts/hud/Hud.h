#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

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

class Hud {
 public:
  static constexpr int kMaxLife = 5000;
  static constexpr int kDeckSize = 40;

  void SetLife(int opponent, int player);
  int OpponentLife() const { return opponent_life_; }
  int PlayerLife() const { return player_life_; }
  bool Over() const { return opponent_life_ <= 0 || player_life_ <= 0; }

  void SetFieldCard(const FieldCardInfo& card);
  void ClearFieldCard();
  bool HasFieldCard() const { return has_field_; }

  void SetDeck(int used, int remaining);  // used + remaining == kDeckSize

  void SetGraveyard(int count);
  int Graveyard() const { return graveyard_; }

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

  int opponent_life_ = kMaxLife;
  int player_life_ = kMaxLife;
  FieldCardInfo field_;
  bool has_field_ = false;
  int deck_used_ = 0;
  int deck_remaining_ = kDeckSize;
  int graveyard_ = 0;
  float zoom_t_ = 0.0f;
  bool zoom_hover_ = false;
};
