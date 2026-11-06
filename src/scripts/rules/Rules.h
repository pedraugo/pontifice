#pragma once

#include "scripts/cards/Cards.h"

namespace rules {

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
