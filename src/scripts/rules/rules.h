#pragma once

#include "scripts/cards/cards.h"

namespace rules {

// Match settings from assets/rules.json; defaults apply when the file or a key is missing.
struct Settings {
  int max_life = 5000;
  int deck_size = 40;
  int initial_hand = 5;
  int hand_limit = 6;
  int max_copies = 2;
  int max_copies_rarest = 1;  // hierarchy 1
  int default_difficulty = 1;  // ai::Difficulty: 0 easy to 3 impossible
};

const Settings& Get();
void Set(const Settings& settings);  // clamps to valid ranges

// Favored category gains buff on attack and defense, the other loses nerf; neutral unaffected.
struct Field {
  bool active = false;
  char favored = 'G';  // 'G' or 'E'
  int buff = 0;
  int nerf = 0;

  static Field From(const CardInfo& card);
};

int EffectiveAttack(const CardInfo& card, const Field& field);
int EffectiveDefense(const CardInfo& card, const Field& field);

// Damage is the life each side's owner loses.
struct Outcome {
  bool attacker_destroyed = false;
  bool defender_destroyed = false;
  int attacker_damage = 0;
  int defender_damage = 0;
};

Outcome Resolve(int attacker_attack, bool defender_defense_mode, int defender_attack,
                int defender_defense);
Outcome ResolveDirect(int attacker_attack);

}
