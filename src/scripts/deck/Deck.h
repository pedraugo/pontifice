#pragma once

#include <cstddef>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

// Top of the deck is the back of the vector.
namespace deck {

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

  void Shuffle();
  void Shuffle(unsigned seed);  // fixed seed: same order (tests, replays)

  std::optional<std::string> Draw();
  void PutOnBottom(std::string code) { cards_.insert(cards_.begin(), std::move(code)); }

  size_t Size() const { return cards_.size(); }
  bool Empty() const { return cards_.empty(); }
  const std::vector<std::string>& Cards() const { return cards_; }

 private:
  std::vector<std::string> cards_;
};

}
