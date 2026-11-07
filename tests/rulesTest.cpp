// Combat rules and opponent AI tests: damage, field cards, choices and levels.
#include <vector>

#include "scripts/ai/ai.h"
#include "scripts/log/log.h"
#include "scripts/rules/rules.h"

namespace {

int failures = 0;

void Check(bool ok, const char* what) {
  if (!ok) {
    LOG("FAILED: %s", what);
    ++failures;
  }
}

CardInfo Make(const char* code, char category, int attack, int defense) {
  CardInfo c;
  c.code = code;
  c.category = category;
  c.attack = attack;
  c.defense = defense;
  c.odds = 10;
  return c;
}

}

int main() {
  rules::Outcome o = rules::Resolve(1000, false, 700, 900);
  Check(o.defender_destroyed && !o.attacker_destroyed && o.defender_damage == 300 &&
            o.attacker_damage == 0,
        "stronger attack destroys the defender and deals the difference");
  o = rules::Resolve(500, false, 700, 900);
  Check(o.attacker_destroyed && !o.defender_destroyed && o.attacker_damage == 200,
        "weaker attack destroys the attacker and deals the difference");
  o = rules::Resolve(700, false, 700, 900);
  Check(o.attacker_destroyed && o.defender_destroyed && o.attacker_damage == 0 &&
            o.defender_damage == 0,
        "tie destroys both without damage");
  o = rules::Resolve(1000, true, 700, 900);
  Check(o.defender_destroyed && o.defender_damage == 0,
        "attack above defense destroys without damage");
  o = rules::Resolve(500, true, 700, 900);
  Check(!o.defender_destroyed && !o.attacker_destroyed && o.attacker_damage == 400,
        "attack below defense: the attacker loses the difference");
  o = rules::Resolve(900, true, 700, 900);
  Check(!o.defender_destroyed && o.attacker_damage == 0,
        "attack equal to defense: nothing happens");
  o = rules::ResolveDirect(1200);
  Check(o.defender_damage == 1200 && !o.defender_destroyed, "direct attack deals all of it");

  CardInfo field = Make("FE01", 'F', 0, 0);
  field.favored = "evil";
  field.buff = 300;
  field.nerf = 300;
  const rules::Field f = rules::Field::From(field);
  const CardInfo good = Make("G401", 'G', 500, 500);
  const CardInfo evil = Make("E401", 'E', 450, 200);
  const CardInfo neutral = Make("N401", 'N', 500, 500);
  Check(rules::EffectiveAttack(evil, f) == 750 && rules::EffectiveDefense(evil, f) == 500,
        "favored category gains the buff");
  Check(rules::EffectiveAttack(good, f) == 200 && rules::EffectiveDefense(good, f) == 200,
        "other category loses the nerf");
  Check(rules::EffectiveAttack(neutral, f) == 500, "neutral card does not change");
  Check(rules::EffectiveAttack(Make("E410", 'E', 250, 50), rules::Field{true, 'G', 300, 300}) == 0,
        "never goes negative");

  const CardInfo strong = Make("G201", 'G', 1800, 1900);
  const CardInfo weak = Make("N409", 'N', 100, 100);
  const CardInfo wall = Make("G305", 'G', 500, 700);
  ai::View v;
  v.hand = {&weak, &strong, &wall};
  v.free_slots = 10;
  v.theirs.push_back(ai::FieldCard{0, &good, false, true, false});
  std::optional<ai::Summon> s = ai::ChooseSummon(v);
  Check(s && s->hand_index == 1 && !s->defense, "summons the strongest in attack mode");
  v.hand = {&weak, &wall};
  s = ai::ChooseSummon(v);
  Check(s && s->hand_index == 1 && s->defense,
        "unable to beat the field: highest defense in defense mode");
  v.theirs.push_back(ai::FieldCard{1, &weak, false, true, false});
  v.hand = {&weak, &wall, &good};
  s = ai::ChooseSummon(v);
  Check(s && (s->hand_index == 1 || s->hand_index == 2) && !s->defense,
        "beats the weaker card: summons in attack mode (ATK 500)");
  v.theirs.pop_back();
  v.can_summon = false;
  Check(!ai::ChooseSummon(v), "no summon left: does not summon");

  ai::View a;
  a.mine.push_back(ai::FieldCard{7, &strong, false, true, false});
  std::optional<ai::Attack> at = ai::ChooseAttack(a);
  Check(at && at->slot == 7 && at->target_slot == -1, "empty field: direct attack");
  a.theirs.push_back(ai::FieldCard{2, &good, false, true, false});
  at = ai::ChooseAttack(a);
  Check(at && at->target_slot == 2, "attacks a weaker card in attack mode");
  a.mine[0].info = &weak;
  Check(!ai::ChooseAttack(a), "does not attack a stronger card");
  a.mine[0].info = &strong;
  a.can_attack = false;
  Check(!ai::ChooseAttack(a), "no attack on the first turn of who started");

  Check(ai::ChooseSlot({0, 1, 2, 5, 6, 7, 9}, 5) == 7, "prefers slot B3, back row center");
  ai::View fc;
  fc.hand = {&field, &evil};
  fc.mine.push_back(ai::FieldCard{5, &evil, false, true, false});
  Check(ai::ChooseFieldCard(fc) == 0, "plays the field card that favors evil cards");
  fc.mine[0].info = &good;
  fc.hand = {&field, &good};
  Check(ai::ChooseFieldCard(fc) == -1, "skips a field card that hurts its own cards");

  // Starter deck: 26 of hierarchy 4, 10 of hierarchy 3 and 4 field cards.
  const cards::Tiers starter{4, 0, 0, 10, 26};
  Check(ai::OpponentTiers(starter, ai::Difficulty::kEasy) == cards::Tiers{4, 0, 0, 0, 36},
        "easy: every card one hierarchy weaker");
  Check(ai::OpponentTiers(starter, ai::Difficulty::kMedium) == starter,
        "medium: same hierarchies as the player");
  Check(ai::OpponentTiers(starter, ai::Difficulty::kHard) == cards::Tiers{4, 0, 5, 18, 13},
        "hard: half of each hierarchy one stronger");
  Check(ai::OpponentTiers(starter, ai::Difficulty::kImpossible) == cards::Tiers{4, 0, 10, 26, 0},
        "impossible: every card one hierarchy stronger");
  const cards::Tiers edges{2, 6, 0, 0, 3};
  Check(ai::OpponentTiers(edges, ai::Difficulty::kImpossible) == cards::Tiers{2, 6, 0, 3, 0} &&
            ai::OpponentTiers(edges, ai::Difficulty::kEasy) == cards::Tiers{2, 0, 6, 0, 3},
        "hierarchy 1 has nothing stronger and hierarchy 4 nothing weaker");
  Check(ai::NextDifficulty(ai::Difficulty::kImpossible) == ai::Difficulty::kEasy,
        "level cycles back to easy");

  if (failures == 0) LOG("rulesTest: ok");
  return failures == 0 ? 0 : 1;
}
