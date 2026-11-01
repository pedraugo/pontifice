#pragma once

#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

// Monte (deck) de cartas de um jogador: lista de códigos de carta ("G101", "E203"...) com o
// topo no fim do vetor. Sem dependência do SDL, para poder ser testado isoladamente.
namespace deck {

// Embaralha `cards` no lugar (Fisher-Yates) com o gerador dado: cada permutação tem a mesma
// chance e o resultado é determinístico para a mesma semente.
template <typename T>
void Shuffle(std::vector<T>& cards, std::mt19937& rng) {
  for (size_t i = cards.size(); i > 1; --i) {
    std::uniform_int_distribution<size_t> pick(0, i - 1);
    std::swap(cards[i - 1], cards[pick(rng)]);
  }
}

class Deck {
 public:
  Deck() = default;
  explicit Deck(std::vector<std::string> cards) : cards_(std::move(cards)) {}

  // Embaralha com semente aleatória (std::random_device).
  void Shuffle();
  // Embaralha com semente fixa: mesma ordem a cada vez (testes, replays, partida em rede).
  void Shuffle(unsigned seed);

  // Tira a carta do topo; vazio se o monte acabou (o jogador perde, ver docs/regra_jogo.md).
  std::optional<std::string> Draw();
  void PutOnTop(std::string code) { cards_.push_back(std::move(code)); }
  void PutOnBottom(std::string code) { cards_.insert(cards_.begin(), std::move(code)); }

  size_t Size() const { return cards_.size(); }
  bool Empty() const { return cards_.empty(); }
  const std::vector<std::string>& Cards() const { return cards_; }

 private:
  std::vector<std::string> cards_;
};

}  // namespace deck
