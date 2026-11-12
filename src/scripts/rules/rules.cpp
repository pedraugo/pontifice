#include "scripts/rules/rules.h"

#include <algorithm>

namespace rules {

namespace {

Settings g_settings;

}

const Settings& Get() { return g_settings; }

void Set(const Settings& settings) {
  g_settings = settings;
  g_settings.max_life = std::max(1, settings.max_life);
  g_settings.deck_size = std::max(1, settings.deck_size);
  g_settings.initial_hand = std::clamp(settings.initial_hand, 0, g_settings.deck_size);
  g_settings.hand_limit = std::max(1, settings.hand_limit);
  g_settings.max_copies = std::max(1, settings.max_copies);
  g_settings.max_copies_rarest = std::max(0, settings.max_copies_rarest);
  g_settings.default_difficulty = std::clamp(settings.default_difficulty, 0, 3);
  g_settings.starter_hierarchy4 = std::clamp(settings.starter_hierarchy4, 0, g_settings.deck_size);
  g_settings.starter_hierarchy3 = std::clamp(settings.starter_hierarchy3, 0,
                                             g_settings.deck_size - g_settings.starter_hierarchy4);
  g_settings.tutorial_opponent_cards = std::max(1, settings.tutorial_opponent_cards);
}

Field Field::From(const CardInfo& card) {
  Field f;
  f.active = card.IsField();
  f.favored = card.favored == "evil" ? 'E' : 'G';
  f.buff = card.buff;
  f.nerf = card.nerf;
  return f;
}

namespace {

int Modifier(const CardInfo& card, const Field& field) {
  if (!field.active || card.IsField() || card.category == 'N') return 0;
  return card.category == field.favored ? field.buff : -field.nerf;
}

}

int EffectiveAttack(const CardInfo& card, const Field& field, int modifier) {
  const int value = card.attack + Modifier(card, field) + modifier;
  return value < 0 ? 0 : value;
}

int EffectiveDefense(const CardInfo& card, const Field& field, int modifier) {
  const int value = card.defense + Modifier(card, field) + modifier;
  return value < 0 ? 0 : value;
}

Outcome Resolve(int attacker_attack, bool defender_defense_mode, int defender_attack,
                int defender_defense) {
  Outcome out;
  if (!defender_defense_mode) {
    // Attack mode: lower attack is destroyed and its owner loses the difference; tie destroys both.
    if (attacker_attack > defender_attack) {
      out.defender_destroyed = true;
      out.defender_damage = attacker_attack - defender_attack;
    } else if (attacker_attack < defender_attack) {
      out.attacker_destroyed = true;
      out.attacker_damage = defender_attack - attacker_attack;
    } else {
      out.attacker_destroyed = out.defender_destroyed = true;
    }
    return out;
  }
  // Defense mode: higher destroys without damage; lower hurts the attacker; equal does nothing.
  if (attacker_attack > defender_defense) {
    out.defender_destroyed = true;
  } else if (attacker_attack < defender_defense) {
    out.attacker_damage = defender_defense - attacker_attack;
  }
  return out;
}

Outcome ResolveDirect(int attacker_attack) {
  Outcome out;
  out.defender_damage = attacker_attack;
  return out;
}

bool HasTarget(const CardInfo& card, bool field_theirs, const std::vector<BoardCard>& theirs) {
  if (!card.IsInstant()) return false;
  switch (card.effect) {
    case FieldEffect::kClearField:
      return field_theirs;
    case FieldEffect::kAttackMode:
      return std::any_of(theirs.begin(), theirs.end(),
                         [](const BoardCard& c) { return c.defense; });
    case FieldEffect::kDamage:
      return card.value > 0;
    case FieldEffect::kWeakenDefense:
    case FieldEffect::kWeakenAttack:
      return !theirs.empty();
    default:
      return false;
  }
}

InstantOutcome ApplyInstant(const CardInfo& card, bool field_theirs,
                            std::vector<BoardCard>& theirs) {
  InstantOutcome out;
  if (!HasTarget(card, field_theirs, theirs)) return out;
  const int count = static_cast<int>(theirs.size());
  switch (card.effect) {
    case FieldEffect::kClearField:
      out.clear_field = true;
      break;
    case FieldEffect::kAttackMode:
      for (int i = 0; i < count; ++i) {
        if (!theirs[i].defense) continue;
        theirs[i].defense = false;
        out.changed.push_back(i);
      }
      break;
    case FieldEffect::kDamage:
      out.damage = card.value;
      break;
    case FieldEffect::kWeakenDefense:
    case FieldEffect::kWeakenAttack:
      for (int i = 0; i < count; ++i) {
        int& mod = card.effect == FieldEffect::kWeakenAttack ? theirs[i].attack_mod
                                                             : theirs[i].defense_mod;
        mod -= card.value;
        out.changed.push_back(i);
      }
      break;
    default:
      break;
  }
  return out;
}

}
