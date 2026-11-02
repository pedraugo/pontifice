#pragma once

#include <SDL3/SDL.h>

#include <string>

// Categoria favorecida por uma carta de campo. Neutras não são afetadas (docs/batalha.md).
enum class Category { kGood, kEvil };

// Carta de campo ativa na arena, como o HUD precisa exibi-la (docs/hud.md).
struct FieldCardInfo {
  std::string name;
  Category favored = Category::kGood;  // recebe buff; a outra recebe nerf
  int buff = 0;
  int nerf = 0;
  SDL_Texture* image = nullptr;  // frente da carta (não é dono)
};

// HUD da batalha (docs/hud.md): camada 2D fixa por cima do campo.
// Cantos superiores: carta de campo ativa (esquerda) e pontos de vida (direita). Cantos
// inferiores: seu monte, usadas e restantes (esquerda) e seu cemitério (direita). Quando
// alguém chega a 0 de vida, exibe o resultado.
class Hud {
 public:
  static constexpr int kMaxLife = 5000;
  static constexpr int kDeckSize = 40;

  void SetLife(int opponent, int player);
  int OpponentLife() const { return opponent_life_; }
  int PlayerLife() const { return player_life_; }
  bool Over() const { return opponent_life_ <= 0 || player_life_ <= 0; }

  // Substitui a carta de campo ativa imediatamente. ClearFieldCard volta a "Sem campo".
  void SetFieldCard(const FieldCardInfo& card);
  void ClearFieldCard();
  bool HasFieldCard() const { return has_field_; }

  // Monte: `used` cartas já saíram, `remaining` ainda estão nele; a soma é kDeckSize.
  void SetDeck(int used, int remaining);
  int DeckUsed() const { return deck_used_; }
  int DeckRemaining() const { return deck_remaining_; }

  void SetGraveyard(int count);
  int Graveyard() const { return graveyard_; }

  // Borda esquerda do painel do monte (px lógicos); por padrão a margem. A batalha a empurra
  // para a direita do botão de som.
  void SetDeckPanelLeft(float x) { deck_panel_left_ = x; }

  void Render(SDL_Renderer* r, int width, int height) const;

 private:
  void RenderFieldPanel(SDL_Renderer* r) const;
  void RenderLifePanel(SDL_Renderer* r, int width) const;
  void RenderDeckPanel(SDL_Renderer* r, int height) const;
  void RenderGraveyardPanel(SDL_Renderer* r, int width, int height) const;
  void RenderResult(SDL_Renderer* r, int width, int height) const;

  int opponent_life_ = kMaxLife;
  int player_life_ = kMaxLife;
  FieldCardInfo field_;
  bool has_field_ = false;
  int deck_used_ = 0;
  int deck_remaining_ = kDeckSize;
  int graveyard_ = 0;
  float deck_panel_left_ = -1.0f;  // < 0: margem padrão
};
