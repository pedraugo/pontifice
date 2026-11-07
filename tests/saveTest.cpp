// Save tests: starter deck, tiered opponent deck, odds-weighted reward, JSON and deck editing.
#include <map>
#include <random>
#include <string>
#include <vector>

#include "scripts/cards/cards.h"
#include "scripts/log/log.h"
#include "scripts/rules/rules.h"
#include "scripts/save/save.h"

namespace {

int failures = 0;

void Check(bool ok, const char* what) {
  if (!ok) {
    LOG("FAILED: %s", what);
    ++failures;
  }
}

CardInfo Card(const std::string& code, char category, int hierarchy, int odds) {
  CardInfo card;
  card.code = code;
  card.category = category;
  card.hierarchy = hierarchy;
  card.odds = odds;
  return card;
}

// Same shape as assets/cards*.json: 10 of hierarchy 4 and 5 of hierarchy 3 per category.
std::vector<CardInfo> SampleCards() {
  std::vector<CardInfo> all;
  for (const char category : {'G', 'E'}) {
    for (int h = 1; h <= 2; ++h) {
      for (int i = 1; i <= 3; ++i) {
        all.push_back(Card(std::string(1, category) + std::to_string(h * 100 + i), category, h, 1));
      }
    }
  }
  for (const char category : {'G', 'E', 'N'}) {
    for (int i = 1; i <= 5; ++i) {
      all.push_back(Card(std::string(1, category) + std::to_string(300 + i), category, 3, 10));
    }
    for (int i = 1; i <= 10; ++i) {
      all.push_back(Card(std::string(1, category) + std::to_string(400 + i), category, 4, 20));
    }
  }
  for (int i = 1; i <= 8; ++i) all.push_back(Card("FE0" + std::to_string(i), 'F', 0, 5));
  return all;
}

void TestStarterDeck() {
  const std::vector<CardInfo> all = SampleCards();
  for (unsigned seed = 1; seed <= 50; ++seed) {
    std::mt19937 rng(seed);
    const std::vector<std::string> deck = cards::StarterDeck(all, rng);
    Check(static_cast<int>(deck.size()) == rules::Get().deck_size,
          "starter deck has deckSize cards");
    int h4 = 0, h3 = 0, field = 0, other = 0;
    std::map<std::string, int> copies;
    for (const std::string& code : deck) {
      const CardInfo* card = cards::Find(all, code);
      if (card == nullptr) continue;
      ++copies[code];
      if (card->IsField()) {
        ++field;
      } else if (card->hierarchy == 4) {
        ++h4;
      } else if (card->hierarchy == 3) {
        ++h3;
      } else {
        ++other;
      }
    }
    Check(h4 == rules::Get().starter_hierarchy4, "starter deck hierarchy 4 count");
    Check(h3 == rules::Get().starter_hierarchy3, "starter deck hierarchy 3 count");
    Check(field == rules::Get().deck_size - h4 - h3, "starter deck fills the rest with field");
    Check(other == 0, "starter deck has no hierarchy 1 or 2");
    for (const auto& [code, count] : copies) {
      Check(count <= rules::Get().max_copies, "starter deck respects maxCopies");
    }
  }
}

void TestTieredDeck() {
  const std::vector<CardInfo> all = SampleCards();
  const cards::Tiers tiers{4, 0, 10, 26, 0};
  for (unsigned seed = 1; seed <= 20; ++seed) {
    std::mt19937 rng(seed);
    const std::vector<std::string> deck = cards::BuildDeck(all, tiers, rng);
    Check(cards::CountTiers(all, deck) == tiers, "tiered deck has the asked hierarchies");
  }
  std::mt19937 rng(3);
  // Hierarchy 1 holds only 6 copies (1 each); the other 4 go to hierarchy 2.
  const std::vector<std::string> overflow =
      cards::BuildDeck(all, cards::Tiers{0, 10, 0, 0, 0}, rng);
  Check(cards::CountTiers(all, overflow) == cards::Tiers{0, 6, 4, 0, 0},
        "missing copies of a hierarchy go to the next weaker one");
  Check(cards::CountTiers(all, {"G401", "FE01", "unknown", "G101"}) == cards::Tiers{1, 1, 0, 0, 1},
        "count skips unknown codes");
}

void TestPickByOdds() {
  const std::vector<CardInfo> all = {Card("A", 'G', 4, 1), Card("B", 'G', 4, 3),
                                     Card("Z", 'G', 4, 0)};
  std::mt19937 rng(7);
  std::map<std::string, int> picks;
  for (int i = 0; i < 8000; ++i) ++picks[cards::PickByOdds(all, rng)->code];
  Check(picks["Z"] == 0, "odds 0 is never picked");
  const double ratio = static_cast<double>(picks["B"]) / picks["A"];
  Check(ratio > 2.6 && ratio < 3.4, "picks follow the odds (3:1)");
  Check(cards::PickByOdds({Card("Z", 'G', 4, 0)}, rng) == nullptr, "all odds 0 picks nothing");
}

void TestJson() {
  save::Data data;
  data.name = "Jo\xC3\xA3o \"Pedro\" \\ \t";
  data.language = "es";
  data.difficulty = 3;
  for (const char* code : {"G401", "N301", "FE01", "G401", "G401"}) data.UnlockCard(code);
  data.deck = {"G401", "N301", "FE01", "G401"};
  save::Data back;
  Check(save::FromJson(save::ToJson(data), back), "round trip parses");
  Check(back.name == data.name, "round trip keeps the name");
  Check(back.language == data.language, "round trip keeps the language");
  Check(back.difficulty == 3, "round trip keeps the difficulty");
  Check(back.unlocked.size() == 5 && back.unlocked[3].code == "G401", "round trip keeps history");
  Check(!back.unlocked[0].date.empty() && back.unlocked[0].date == data.unlocked[0].date,
        "round trip keeps the unlock dates");
  Check(back.deck == data.deck, "round trip keeps the deck with repeats");
  Check(back.Available("G401") == 3 && back.Available("XXXX") == 0, "available groups copies");
  Check(back.Available().size() == 3, "available has one entry per code");

  save::Data ignored;
  Check(!save::FromJson("{\"unlocked\": []}", ignored), "save without name is rejected");
  Check(!save::FromJson("{\"name\": \"A\"}", ignored), "save without unlocked is rejected");
  Check(!save::FromJson("not json", ignored), "broken save is rejected");

  save::Data clamped;
  Check(save::FromJson("{\"name\": \"A\", \"unlocked\": [{\"code\": \"G401\"}],"
                       " \"deck\": [\"G401\", \"G401\", \"N301\"]}",
                       clamped),
        "deck beyond the unlocked copies parses");
  Check(clamped.deck == std::vector<std::string>{"G401"}, "deck keeps only unlocked copies");
  Check(clamped.difficulty == rules::Get().default_difficulty,
        "missing difficulty falls back to defaultDifficulty");
}

void TestSetInDeck() {
  save::Data data;
  data.name = "A";
  for (int i = 0; i < 50; ++i) data.UnlockCard("N401");
  data.UnlockCard("G101");
  data.SetInDeck("N401", 45);
  Check(static_cast<int>(data.deck.size()) == rules::Get().deck_size, "deck stops at deckSize");
  data.SetInDeck("G101", 1);
  Check(data.InDeck("G101") == 0, "full deck takes no more cards");
  data.SetInDeck("N401", 30);
  data.SetInDeck("G101", 5);
  Check(data.InDeck("N401") == 30 && data.InDeck("G101") == 1,
        "copies limited by the available ones");
  data.SetInDeck("N401", 0);
  Check(data.deck == std::vector<std::string>{"G101"}, "setting 0 removes every copy");
}

}

int main() {
  TestStarterDeck();
  TestTieredDeck();
  TestPickByOdds();
  TestJson();
  TestSetInDeck();
  if (failures > 0) {
    LOG("saveTest: %d failure(s)", failures);
    return 1;
  }
  LOG("saveTest: ok");
  return 0;
}
