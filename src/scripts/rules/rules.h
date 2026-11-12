#pragma once

#include <vector>

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
  // New save deck: hierarchy 4 and 3 cards (good, evil, neutral); field cards fill the rest.
  int starter_hierarchy4 = 26;
  int starter_hierarchy3 = 10;
  // Tutorial battle: the opponent deck repeats only this many of the weakest battle cards.
  int tutorial_opponent_cards = 10;
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

// `modifier` comes from instant field cards that hit this card on the board.
int EffectiveAttack(const CardInfo& card, const Field& field, int modifier = 0);
int EffectiveDefense(const CardInfo& card, const Field& field, int modifier = 0);

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

// A card on the side an instant field card hits; mirrors Board::Card.
struct BoardCard {
  bool defense = false;
  int attack_mod = 0;
  int defense_mod = 0;
};

// What an instant field card does besides changing the hit cards in place.
struct InstantOutcome {
  bool clear_field = false;
  int damage = 0;  // life the hit side loses
  std::vector<int> changed;  // indices of the hit cards that changed
};

// `field_theirs`: the active field was played by the side being hit.
bool HasTarget(const CardInfo& card, bool field_theirs, const std::vector<BoardCard>& theirs);
// Without a target nothing happens.
InstantOutcome ApplyInstant(const CardInfo& card, bool field_theirs,
                            std::vector<BoardCard>& theirs);

}
