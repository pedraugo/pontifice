#pragma once

#include "scripts/cards/Cards.h"

// Regras de combate (docs/batalha.md), sem SDL: carta de campo ativa, valores efetivos e
// cálculo de dano de um ataque. Usadas pela cena de batalha e pela IA.
namespace rules {

// Carta de campo ativa na arena: a categoria favorecida ganha buff em ataque e defesa, a
// outra perde nerf; neutras não são afetadas.
struct Field {
  bool active = false;
  char favored = 'G';  // 'G' ou 'E'
  int buff = 0;
  int nerf = 0;

  static Field From(const CardInfo& card);  // de uma carta de campo (category 'F')
};

int EffectiveAttack(const CardInfo& card, const Field& field);
int EffectiveDefense(const CardInfo& card, const Field& field);

// Resultado de um ataque. Dano já é o valor a descontar dos pontos de vida de cada lado.
struct Outcome {
  bool attacker_destroyed = false;
  bool defender_destroyed = false;
  int attacker_damage = 0;  // pontos de vida que o atacante perde
  int defender_damage = 0;  // pontos de vida que o dono da carta atacada perde
};

// Ataque contra uma carta. `defender_defense_mode` diz a posição dela; `defender_attack` e
// `defender_defense` são os valores efetivos.
Outcome Resolve(int attacker_attack, bool defender_defense_mode, int defender_attack,
                int defender_defense);
// Ataque direto (todos os slots do oponente vazios): todo o ataque vira dano.
Outcome ResolveDirect(int attacker_attack);

}  // namespace rules
