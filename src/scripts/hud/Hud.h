#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

// Categoria favorecida por uma carta de campo. Neutras não são afetadas (docs/batalha.md).
enum class Category { kGood, kEvil };

// Carta de campo ativa na arena, como o HUD precisa exibi-la (docs/hud.md).
struct FieldCardInfo {
  std::string name;
  Category favored = Category::kGood;  // recebe buff; a outra recebe nerf
  int buff = 0;
  int nerf = 0;
  SDL_Texture* image = nullptr;  // frente da carta (não é dono): só sem a ilustração
  SDL_Texture* art = nullptr;    // ilustração da carta (não é dono): preferida no painel
  SDL_Texture* art_large = nullptr;  // ilustração em resolução cheia, para a ampliação
};

// HUD da batalha (docs/hud.md): camada 2D fixa por cima do campo.
// Cantos superiores: carta de campo ativa (esquerda) e pontos de vida (direita). Cantos
// inferiores: seu monte, usadas e restantes (esquerda) e seu cemitério (direita). Quando
// alguém chega a 0 de vida, exibe o resultado.
class Hud {
 public:
  static constexpr int kMaxLife = 5000;
  static constexpr int kDeckSize = 40;

  // Vida perdida (valor novo menor que o atual) dispara o tremor do painel de vida e um
  // número de dano ("-200" em vermelho) que salta do valor; o número mostra o dano pedido,
  // mesmo que a vida pare em 0.
  void SetLife(int opponent, int player);
  int OpponentLife() const { return opponent_life_; }
  int PlayerLife() const { return player_life_; }
  bool Over() const { return opponent_life_ <= 0 || player_life_ <= 0; }

  // Substitui a carta de campo ativa imediatamente. ClearFieldCard volta a "Sem campo"
  // (hud.noField).
  void SetFieldCard(const FieldCardInfo& card);
  void ClearFieldCard();
  bool HasFieldCard() const { return has_field_; }

  // Monte: `used` cartas já saíram, `remaining` ainda estão nele; a soma é kDeckSize.
  void SetDeck(int used, int remaining);

  void SetGraveyard(int count);
  int Graveyard() const { return graveyard_; }

  // Mouse em px lógicos: com o mouse sobre a ilustração do painel de campo, ela cresce da
  // miniatura até o tamanho nativo logo abaixo do painel (animação de kZoomSeconds). Também
  // anima o tremor do painel de vida e os números de dano.
  void Update(float dt, float mouse_x, float mouse_y);

  void Render(SDL_Renderer* r, int width, int height) const;

 private:
  // Retângulos do painel de campo (a largura cresce com o texto) e da miniatura.
  struct FieldLayout {
    SDL_FRect panel{};
    SDL_FRect thumb{};
    bool art = false;  // miniatura é a ilustração (senão, a frente inteira)
    std::string buff, nerf;
  };
  FieldLayout LayoutField() const;
  SDL_FRect ZoomRect() const;  // ilustração ampliada (tamanho nativo), abaixo do painel
  void RenderFieldPanel(SDL_Renderer* r) const;
  void RenderFieldZoom(SDL_Renderer* r) const;
  void RenderLifePanel(SDL_Renderer* r, int width) const;
  // Monte e cemitério no mesmo painel, no canto inferior direito.
  void RenderPilesPanel(SDL_Renderer* r, int width, int height) const;
  void RenderResult(SDL_Renderer* r, int width, int height) const;
  void RenderDamagePopups(SDL_Renderer* r, int width) const;
  void OnDamage(bool player, int amount);

  // Número de dano saltando do valor de vida: linha (oponente/jogador), valor, tempo de vida
  // e deriva horizontal (alterna o lado para números seguidos não se sobreporem).
  struct DamagePopup {
    bool player = false;
    int amount = 0;
    float time = 0.0f;
    float drift = -1.0f;
  };
  std::vector<DamagePopup> popups_;
  float life_shake_ = 0.0f;       // segundos restantes do tremor do painel de vida
  float life_shake_time_ = 0.0f;  // fase do tremor
  float life_flash_[2] = {0.0f, 0.0f};  // valor piscando em vermelho: oponente, jogador

  int opponent_life_ = kMaxLife;
  int player_life_ = kMaxLife;
  FieldCardInfo field_;
  bool has_field_ = false;
  int deck_used_ = 0;
  int deck_remaining_ = kDeckSize;
  int graveyard_ = 0;
  float zoom_t_ = 0.0f;  // 0 miniatura .. 1 ilustração ampliada
  bool zoom_hover_ = false;
};
