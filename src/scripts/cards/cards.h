#pragma once

#include <array>
#include <functional>
#include <random>
#include <string>
#include <vector>

// kAura is the lasting buff/nerf field; the others act once on the opponent when played.
enum class FieldEffect { kAura, kClearField, kAttackMode, kDamage, kWeakenDefense, kWeakenAttack };

struct CardInfo {
  std::string code;  // "G101", "E203", "N301", "FE01"
  std::string name;
  std::string description;
  char category = 'N';  // 'G' good, 'E' evil, 'N' neutral, 'F' field
  std::string category_name;
  int hierarchy = 0;  // 1 (rarest) to 4; 0 for field cards
  std::string hierarchy_name;
  int attack = 0;
  int defense = 0;
  int odds = 1;  // deck draw weight; higher = more common
  bool combinable = false;
  std::string image_url;
  // Field cards only: favored category ("good"/"evil"), buff and nerf.
  std::string favored;
  int buff = 0;
  int nerf = 0;
  FieldEffect effect = FieldEffect::kAura;
  int value = 0;  // life or points the effect removes

  bool IsField() const { return category == 'F'; }
  bool IsInstant() const { return IsField() && effect != FieldEffect::kAura; }
  std::string CategoryLabel() const;
  const char* FrameSprite() const;
};

namespace cards {

std::vector<CardInfo> LoadAll(const std::string& base);

const CardInfo* Find(const std::vector<CardInfo>& all, const std::string& code);

// JSON and translation key of an effect ("clearField"); empty for kAura.
const char* EffectName(FieldEffect effect);

// Weighted draw; copy limits from rules::Get(); weight 0 never enters.
using Weight = std::function<int(const CardInfo&)>;
std::vector<std::string> BuildDeck(const std::vector<CardInfo>& all, int size, std::mt19937& rng,
                                   const Weight& weight = Weight());

// Card count per hierarchy: index 0 = field cards, 1 (rarest) to 4.
using Tiers = std::array<int, 5>;
// Unknown codes are skipped.
Tiers CountTiers(const std::vector<CardInfo>& all, const std::vector<std::string>& codes);
// tiers[h] cards of each hierarchy by odds; copies a hierarchy lacks go to the next weaker one.
std::vector<std::string> BuildDeck(const std::vector<CardInfo>& all, const Tiers& tiers,
                                   std::mt19937& rng);

// One card with chance proportional to its odds; nullptr when every odds is 0.
const CardInfo* PickByOdds(const std::vector<CardInfo>& all, std::mt19937& rng);

// New save deck, uniform draw sized by rules::Get(): hierarchy 4, hierarchy 3, then field cards.
std::vector<std::string> StarterDeck(const std::vector<CardInfo>& all, std::mt19937& rng);

}
