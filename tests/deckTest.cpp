// Deck tests: shuffle keeps the cards, follows the seed, and draws from the top.
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "scripts/deck/deck.h"
#include "scripts/log/log.h"

namespace {

int failures = 0;

void Check(bool ok, const char* what) {
  if (!ok) {
    LOG("FAILED: %s", what);
    ++failures;
  }
}

std::vector<std::string> SampleDeck(int n) {
  std::vector<std::string> cards;
  for (int i = 0; i < n; ++i) cards.push_back("C" + std::to_string(100 + i));
  return cards;
}

}

int main() {
  const std::vector<std::string> original = SampleDeck(40);

  deck::Deck a(original);
  a.Shuffle(42u);
  std::vector<std::string> sorted = a.Cards();
  std::sort(sorted.begin(), sorted.end());
  Check(sorted == original, "shuffle keeps the same cards");
  Check(a.Size() == 40, "size stays 40");

  deck::Deck b(original);
  b.Shuffle(42u);
  Check(a.Cards() == b.Cards(), "same seed, same order");
  deck::Deck c(original);
  c.Shuffle(7u);
  Check(c.Cards() != a.Cards(), "different seeds, different orders");
  Check(a.Cards() != original, "order changes from the original");

  deck::Deck d(original);
  d.Shuffle();
  sorted = d.Cards();
  std::sort(sorted.begin(), sorted.end());
  Check(sorted == original, "random seed keeps the same cards");

  deck::Deck e(SampleDeck(3));
  Check(e.Draw() == std::string("C102"), "draw takes the top card");
  Check(e.Draw() == std::string("C101"), "second draw");
  e.PutOnBottom("X");
  Check(e.Draw() == std::string("C100"), "card put on the bottom is not the top");
  Check(e.Draw() == std::string("X"), "the bottom card comes out last");
  Check(!e.Draw().has_value(), "empty deck draws nothing");
  Check(e.Empty(), "deck is empty");

  deck::Deck empty;
  empty.Shuffle(1u);
  Check(empty.Empty(), "shuffle an empty deck");
  deck::Deck one(SampleDeck(1));
  one.Shuffle(1u);
  Check(one.Cards() == SampleDeck(1), "shuffle a single card");

  if (failures == 0) LOG("deckTest: ok");
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
