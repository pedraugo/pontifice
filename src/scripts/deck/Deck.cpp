#include "scripts/deck/Deck.h"

namespace deck {

void Deck::Shuffle() {
  std::random_device device;
  Shuffle(device());
}

void Deck::Shuffle(unsigned seed) {
  std::mt19937 rng(seed);
  deck::Shuffle(cards_, rng);
}

std::optional<std::string> Deck::Draw() {
  if (cards_.empty()) return std::nullopt;
  std::string top = std::move(cards_.back());
  cards_.pop_back();
  return top;
}

}
