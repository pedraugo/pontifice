#include "scripts/cards/cards.h"

#include <SDL3/SDL.h>

#include <map>

#include "scripts/deck/deck.h"
#include "scripts/i18n/i18n.h"
#include "scripts/json/json.h"
#include "scripts/log/log.h"

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
    card.category_name = Str(item.Find("categoryName"));
    card.hierarchy = Int(item.Find("hierarchy"));
    card.hierarchy_name = Str(item.Find("hierarchyName"));
    card.image_url = Str(item.Find("imageUrl"));
    card.attack = Int(item.Path("attributes.attack"));
    card.defense = Int(item.Path("attributes.defense"));
    card.odds = Int(item.Path("attributes.odds"), 1);
    const json::Value* combinable = item.Path("attributes.combinable");
    card.combinable = combinable != nullptr && combinable->boolean;
    card.favored = Str(item.Path("attributes.category"));
    card.buff = Int(item.Path("attributes.buff"));
    card.nerf = Int(item.Path("attributes.nerf"));
    if (card.code.empty()) continue;
    // Missing translation key keeps the JSON text.
    card.name = i18n::GetOr("card." + card.code + ".name", card.name);
    card.description = i18n::GetOr("card." + card.code + ".description", card.description);
    if (card.hierarchy > 0 && !card.category_name.empty()) {
      card.hierarchy_name = i18n::GetOr(
          "hierarchy." + card.category_name + "." + std::to_string(card.hierarchy),
          card.hierarchy_name);
    }
    out.push_back(card);
  }
}

}

std::string CardInfo::CategoryLabel() const {
  switch (category) {
    case 'G':
      return i18n::Get("category.good");
    case 'E':
      return i18n::Get("category.evil");
    case 'N':
      return i18n::Get("category.neutral");
    default:
      return i18n::Get("category.field");
  }
}

const char* CardInfo::FrameSprite() const {
  switch (category) {
    case 'G':
      return "cardFrontGood.webp";
    case 'E':
      return "cardFrontEvil.webp";
    case 'N':
      return "cardFrontNeutral.webp";
    default:
      return "cardFrontField.webp";
  }
}

namespace cards {

std::vector<CardInfo> LoadAll(const std::string& base) {
  std::vector<CardInfo> all;
  for (const char* file : {"cardsGood.json", "cardsEvil.json", "cardsNeutral.json",
                           "cardsField.json"}) {
    LoadFile(base + "assets/" + file, all);
  }
  LOG("Cards loaded: %zu", all.size());
  return all;
}

const CardInfo* Find(const std::vector<CardInfo>& all, const std::string& code) {
  for (const CardInfo& card : all) {
    if (card.code == code) return &card;
  }
  return nullptr;
}

std::vector<std::string> BuildDeck(const std::vector<CardInfo>& all, int size,
                                   std::mt19937& rng, const Weight& weight) {
  std::vector<std::string> deck;
  if (all.empty()) return deck;
  std::map<std::string, int> copies;
  auto limit = [](const CardInfo& card) { return card.hierarchy == 1 ? 1 : 2; };
  auto w = [&weight](const CardInfo& card) { return weight ? weight(card) : card.odds; };
  while (static_cast<int>(deck.size()) < size) {
    long total = 0;
    for (const CardInfo& card : all) {
      if (copies[card.code] < limit(card)) total += w(card);
    }
    if (total <= 0) break;
    std::uniform_int_distribution<long> pick(1, total);
    long roll = pick(rng);
    for (const CardInfo& card : all) {
      if (copies[card.code] >= limit(card)) continue;
      roll -= w(card);
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

}
