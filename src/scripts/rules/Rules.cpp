#include "scripts/rules/Rules.h"

namespace rules {

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

}  // namespace

int EffectiveAttack(const CardInfo& card, const Field& field) {
  const int value = card.attack + Modifier(card, field);
  return value < 0 ? 0 : value;
}

int EffectiveDefense(const CardInfo& card, const Field& field) {
  const int value = card.defense + Modifier(card, field);
  return value < 0 ? 0 : value;
}

Outcome Resolve(int attacker_attack, bool defender_defense_mode, int defender_attack,
                int defender_defense) {
  Outcome out;
  if (!defender_defense_mode) {
    // Contra carta em modo de ataque: quem tem menos ataque é destruído e seu dono perde a
    // diferença; empate destrói as duas sem dano.
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
  // Contra carta em modo de defesa: maior que a defesa destrói sem dano; menor, ninguém é
  // destruído e o atacante perde a diferença; igual, nada acontece.
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

}  // namespace rules
