#include "scripts/cards/Cards.h"

#include <SDL3/SDL.h>

#include <map>

#include "scripts/deck/Deck.h"
#include "scripts/json/Json.h"

namespace {

int Int(const json::Value* v, int fallback = 0) {
  return v != nullptr && v->IsNumber() ? static_cast<int>(v->number) : fallback;
}

std::string Str(const json::Value* v) { return v != nullptr && v->IsString() ? v->string : ""; }

void LoadFile(const std::string& path, std::vector<CardInfo>& out) {
  json::Value root;
  if (!json::LoadFile(path, root) || !root.IsArray()) return;
  for (const json::Value& item : root.array) {
    if (!item.IsObject()) continue;
    CardInfo card;
    card.code = Str(item.Find("code"));
    card.name = Str(item.Find("name"));
    card.description = Str(item.Find("description"));
    const std::string category = Str(item.Find("category"));
    card.category = category.empty() ? 'N' : category[0];
    card.hierarchy = Int(item.Find("hierarchy"));
    card.attack = Int(item.Path("attributes.attack"));
    card.defense = Int(item.Path("attributes.defense"));
    card.odds = Int(item.Path("attributes.odds"), 1);
    const json::Value* combinable = item.Path("attributes.combinable");
    card.combinable = combinable != nullptr && combinable->boolean;
    card.favored = Str(item.Path("attributes.category"));
    card.buff = Int(item.Path("attributes.buff"));
    card.nerf = Int(item.Path("attributes.nerf"));
    if (!card.code.empty()) out.push_back(card);
  }
}

}  // namespace

const char* CardInfo::FrameSprite() const {
  switch (category) {
    case 'G':
      return "CardFrontGood.png";
    case 'E':
      return "CardFrontEvil.png";
    default:
      return "CardFrontField.png";
  }
}

namespace cards {

std::vector<CardInfo> LoadAll(const std::string& base) {
  std::vector<CardInfo> all;
  for (const char* file : {"CardsGood.json", "CardsEvil.json", "CardsNeutral.json",
                           "CardsField.json"}) {
    LoadFile(base + "assets/" + file, all);
  }
  SDL_Log("Cartas carregadas: %zu", all.size());
  return all;
}

const CardInfo* Find(const std::vector<CardInfo>& all, const std::string& code) {
  for (const CardInfo& card : all) {
    if (card.code == code) return &card;
  }
  return nullptr;
}

std::vector<std::string> BuildDeck(const std::vector<CardInfo>& all, int size,
                                   std::mt19937& rng) {
  std::vector<std::string> deck;
  if (all.empty()) return deck;
  std::map<std::string, int> copies;
  auto limit = [](const CardInfo& card) { return card.hierarchy == 1 ? 1 : 2; };
  // Sorteio por peso entre as cartas que ainda podem entrar; para quando não há mais nenhuma.
  while (static_cast<int>(deck.size()) < size) {
    int total = 0;
    for (const CardInfo& card : all) {
      if (copies[card.code] < limit(card)) total += card.odds;
    }
    if (total <= 0) break;
    std::uniform_int_distribution<int> pick(1, total);
    int roll = pick(rng);
    for (const CardInfo& card : all) {
      if (copies[card.code] >= limit(card)) continue;
      roll -= card.odds;
      if (roll <= 0) {
        deck.push_back(card.code);
        ++copies[card.code];
        break;
      }
    }
  }
  deck::Shuffle(deck, rng);
  return deck;
}

}  // namespace cards
