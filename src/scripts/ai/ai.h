#pragma once

#include <optional>
#include <random>
#include <vector>

#include "scripts/cards/cards.h"
#include "scripts/rules/rules.h"

namespace ai {

// Only changes the opponent deck (OpponentTiers); decisions are the same at every level.
enum class Difficulty { kEasy, kMedium, kHard, kImpossible };
const char* DifficultyKey(Difficulty d);
Difficulty NextDifficulty(Difficulty d);
// Player's hierarchies shifted: easy one weaker, hard half one stronger, impossible all stronger.
cards::Tiers OpponentTiers(const cards::Tiers& player, Difficulty d);

struct FieldCard {
  int slot = -1;
  const CardInfo* info = nullptr;
  bool defense = false;
  bool can_attack = false;  // in attack mode and has not attacked this turn
  bool summoned_this_turn = false;
  int attack_mod = 0;
  int defense_mod = 0;
};

// Snapshot from the computer's point of view: mine = computer cards, theirs = player cards.
struct View {
  std::vector<const CardInfo*> hand;
  std::vector<FieldCard> mine;
  std::vector<FieldCard> theirs;
  rules::Field field;
  bool field_mine = false;  // the active field was played by the computer
  int my_life = 0;
  int their_life = 0;
  bool can_summon = true;
  bool can_attack = true;  // false on the first turn of whoever starts
  int free_slots = 0;
};

struct Summon {
  int hand_index = -1;
  bool defense = false;
};
// Hand index of the best field card (lasting or instant) worth playing now; -1 = none.
int ChooseFieldCard(const View& view);
std::optional<Summon> ChooseSummon(const View& view);
int ChooseSlot(const std::vector<int>& free_slots, int cols);
std::vector<int> ChoosePositionChanges(const View& view);

struct Attack {
  int slot = -1;
  int target_slot = -1;  // -1 = direct attack
};
std::optional<Attack> ChooseAttack(const View& view);

}
