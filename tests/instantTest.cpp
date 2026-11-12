// Instant field cards: each effect with and without a target, combat after it, AI choices, catalog.
#include <string>
#include <vector>

#include "scripts/ai/ai.h"
#include "scripts/cards/cards.h"
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

CardInfo Battle(const char* code, char category, int attack, int defense) {
  CardInfo c;
  c.code = code;
  c.category = category;
  c.attack = attack;
  c.defense = defense;
  c.hierarchy = 4;
  return c;
}

CardInfo Instant(const char* code, FieldEffect effect, int value = 0) {
  CardInfo c;
  c.code = code;
  c.category = 'F';
  c.effect = effect;
  c.value = value;
  c.odds = 5;
  return c;
}

CardInfo Aura(const char* code, const char* favored) {
  CardInfo c;
  c.code = code;
  c.category = 'F';
  c.favored = favored;
  c.buff = 300;
  c.nerf = 300;
  c.odds = 5;
  return c;
}

rules::BoardCard InAttack() { return rules::BoardCard{false, 0, 0}; }
rules::BoardCard InDefense() { return rules::BoardCard{true, 0, 0}; }

ai::FieldCard Mine(int slot, const CardInfo* info, bool can_attack = true) {
  return ai::FieldCard{slot, info, false, can_attack, false};
}

ai::FieldCard Theirs(int slot, const CardInfo* info, bool defense, int attack_mod = 0,
                     int defense_mod = 0) {
  return ai::FieldCard{slot, info, defense, false, false, attack_mod, defense_mod};
}

ai::View EmptyView() {
  ai::View v;
  v.my_life = 5000;
  v.their_life = 5000;
  return v;
}

const CardInfo kStrong = Battle("G201", 'G', 1800, 1900);
const CardInfo kWall = Battle("G305", 'G', 500, 700);
const CardInfo kGood = Battle("G401", 'G', 500, 500);
const CardInfo kEvil = Battle("E401", 'E', 450, 200);
const CardInfo kWeak = Battle("N409", 'N', 100, 100);

void CalmingOfTheStorm() {
  const CardInfo storm = Instant("FN01", FieldEffect::kClearField);
  std::vector<rules::BoardCard> board = {InDefense()};
  Check(!rules::HasTarget(storm, false, board), "FN01: no opponent field, no target");
  rules::InstantOutcome out = rules::ApplyInstant(storm, false, board);
  Check(!out.clear_field, "FN01: an own field or no field is never cancelled");
  out = rules::ApplyInstant(storm, true, board);
  Check(out.clear_field && out.damage == 0 && out.changed.empty() && board[0].defense,
        "FN01: cancels the opponent field and touches nothing else");

  const CardInfo evil_field = Aura("FE01", "evil");
  ai::View v = EmptyView();
  v.hand = {&storm};
  v.mine = {Mine(0, &kGood)};
  Check(ai::ChooseFieldCard(v) == -1, "FN01 AI: holds it when no field is active");
  v.field = rules::Field::From(evil_field);
  Check(ai::ChooseFieldCard(v) == 0, "FN01 AI: cancels the player field that weakens its cards");
  v.field_mine = true;
  Check(ai::ChooseFieldCard(v) == -1, "FN01 AI: never cancels its own field");
  v.field_mine = false;
  v.mine = {Mine(0, &kEvil)};
  Check(ai::ChooseFieldCard(v) == -1, "FN01 AI: keeps a player field that helps its cards");
}

void GoliathsChallenge() {
  const CardInfo challenge = Instant("FN02", FieldEffect::kAttackMode);
  std::vector<rules::BoardCard> board;
  Check(!rules::HasTarget(challenge, false, board), "FN02: empty side, no target");
  board = {InAttack(), InAttack()};
  Check(!rules::HasTarget(challenge, false, board), "FN02: only attack-mode cards, no target");
  board = {InDefense(), InAttack(), InDefense()};
  const rules::InstantOutcome out = rules::ApplyInstant(challenge, false, board);
  Check(out.changed == std::vector<int>{0, 2} && !board[0].defense && !board[1].defense &&
            !board[2].defense,
        "FN02: every defense card goes to attack, attack cards are untouched");
  Check(out.damage == 0 && !out.clear_field && board[0].attack_mod == 0 &&
            board[0].defense_mod == 0,
        "FN02: no damage, field and values stay");

  const rules::Outcome before = rules::Resolve(400, true, 300, 700);
  const rules::Outcome after = rules::Resolve(400, board[0].defense, 300, 700);
  Check(!before.defender_destroyed && before.attacker_damage == 300 &&
            after.defender_destroyed && after.defender_damage == 100,
        "FN02: a defender that blocked now dies and its owner takes the difference");

  ai::View v = EmptyView();
  v.hand = {&challenge};
  v.mine = {Mine(0, &kStrong)};
  v.theirs = {Theirs(1, &kWall, true)};
  Check(ai::ChooseFieldCard(v) == 0, "FN02 AI: exposes a defender its attacker beats");
  v.can_attack = false;
  Check(ai::ChooseFieldCard(v) == -1, "FN02 AI: holds it on a turn it cannot attack");
  v.can_attack = true;
  v.mine = {Mine(0, &kWeak)};
  Check(ai::ChooseFieldCard(v) == -1, "FN02 AI: holds it when no attacker beats the exposed card");
  v.mine = {Mine(0, &kStrong, false)};
  Check(ai::ChooseFieldCard(v) == -1, "FN02 AI: holds it when its cards cannot attack");
  v.mine = {Mine(0, &kStrong)};
  v.theirs = {Theirs(1, &kWall, false)};
  Check(ai::ChooseFieldCard(v) == -1, "FN02 AI: holds it when the player has no defense card");
}

void DavidsSling() {
  const CardInfo sling = Instant("FN03", FieldEffect::kDamage, 100);
  std::vector<rules::BoardCard> board;
  Check(rules::HasTarget(sling, false, board), "FN03: hits even with an empty side");
  board = {InDefense(), InAttack()};
  const rules::InstantOutcome out = rules::ApplyInstant(sling, false, board);
  Check(out.damage == 100 && out.changed.empty() && !out.clear_field && board[0].defense &&
            board[1].attack_mod == 0,
        "FN03: the opponent loses 100 life, cards untouched");
  Check(!rules::HasTarget(Instant("FN03", FieldEffect::kDamage, 0), false, board),
        "FN03: without a value it has no effect");

  ai::View v = EmptyView();
  v.hand = {&sling};
  Check(ai::ChooseFieldCard(v) == 0, "FN03 AI: plays it whenever it holds it");
  const CardInfo plow = Instant("FN05", FieldEffect::kWeakenAttack, 300);
  v.hand = {&plow, &sling};
  v.theirs = {Theirs(0, &kStrong, false), Theirs(1, &kStrong, false), Theirs(2, &kGood, false)};
  Check(ai::ChooseFieldCard(v) == 0, "FN03 AI: weakening many cards comes before 100 damage");
  v.their_life = 100;
  Check(ai::ChooseFieldCard(v) == 1, "FN03 AI: the sling comes first when it finishes the player");
}

void WallsOfJericho() {
  const CardInfo walls = Instant("FN04", FieldEffect::kWeakenDefense, 300);
  std::vector<rules::BoardCard> board;
  Check(!rules::HasTarget(walls, false, board), "FN04: empty side, no target");
  board = {InDefense(), InAttack()};
  rules::InstantOutcome out = rules::ApplyInstant(walls, false, board);
  Check(out.changed == std::vector<int>{0, 1} && board[0].defense_mod == -300 &&
            board[1].defense_mod == -300 && board[0].attack_mod == 0 && board[0].defense &&
            !board[1].defense,
        "FN04: every card loses 300 DEF, ATK and position stay");
  out = rules::ApplyInstant(walls, false, board);
  Check(board[0].defense_mod == -600, "FN04: a second copy stacks");

  Check(rules::EffectiveDefense(kWall, rules::Field{}, -300) == 400, "FN04: DEF 700 becomes 400");
  Check(rules::EffectiveDefense(kEvil, rules::Field{}, -300) == 0, "FN04: DEF never below zero");
  const rules::Field evil_field = rules::Field::From(Aura("FE01", "evil"));
  Check(rules::EffectiveDefense(kWall, evil_field, -300) == 100,
        "FN04: adds to the nerf of the active field");
  const rules::Outcome before = rules::Resolve(500, true, kWall.attack, kWall.defense);
  const rules::Outcome after =
      rules::Resolve(500, true, kWall.attack, rules::EffectiveDefense(kWall, rules::Field{}, -300));
  Check(!before.defender_destroyed && before.attacker_damage == 200 &&
            after.defender_destroyed && after.defender_damage == 0 && after.attacker_damage == 0,
        "FN04: the attacker now breaks the lowered defense");

  ai::View v = EmptyView();
  v.hand = {&walls};
  v.theirs = {Theirs(1, &kWall, true)};
  Check(ai::ChooseFieldCard(v) == 0, "FN04 AI: lowers the DEF of a defending card");
  v.can_attack = false;
  Check(ai::ChooseFieldCard(v) == -1, "FN04 AI: holds it on a turn it cannot attack");
  v.can_attack = true;
  v.theirs = {Theirs(1, &kWall, false)};
  Check(ai::ChooseFieldCard(v) == -1, "FN04 AI: holds it when no card defends");

  ai::View a = EmptyView();
  a.mine = {Mine(0, &kGood)};
  a.theirs = {Theirs(1, &kWall, true)};
  Check(!ai::ChooseAttack(a), "FN04 AI: does not attack DEF 700 with ATK 500");
  a.theirs = {Theirs(1, &kWall, true, 0, -300)};
  const std::optional<ai::Attack> at = ai::ChooseAttack(a);
  Check(at && at->target_slot == 1, "FN04 AI: attacks the defense the card lowered");
}

void SwordsIntoPlowshares() {
  const CardInfo plow = Instant("FN05", FieldEffect::kWeakenAttack, 300);
  std::vector<rules::BoardCard> board;
  Check(!rules::HasTarget(plow, false, board), "FN05: empty side, no target");
  board = {InAttack(), InDefense()};
  rules::ApplyInstant(plow, false, board);
  Check(board[0].attack_mod == -300 && board[1].attack_mod == -300 &&
            board[0].defense_mod == 0 && !board[0].defense && board[1].defense,
        "FN05: every card loses 300 ATK, DEF and position stay");

  const CardInfo brute = Battle("E301", 'E', 800, 600);
  Check(rules::EffectiveAttack(brute, rules::Field{}, -300) == 500, "FN05: ATK 800 becomes 500");
  Check(rules::EffectiveAttack(Battle("E410", 'E', 250, 50), rules::Field{}, -300) == 0,
        "FN05: ATK never below zero");
  const rules::Outcome before = rules::Resolve(600, false, brute.attack, brute.defense);
  const rules::Outcome after = rules::Resolve(
      600, false, rules::EffectiveAttack(brute, rules::Field{}, -300), brute.defense);
  Check(before.attacker_destroyed && before.attacker_damage == 200 && after.defender_destroyed &&
            after.defender_damage == 100,
        "FN05: the weakened card loses the clash it used to win");

  ai::View v = EmptyView();
  v.hand = {&plow};
  Check(ai::ChooseFieldCard(v) == -1, "FN05 AI: holds it when the player side is empty");
  v.theirs = {Theirs(1, &kStrong, false)};
  Check(ai::ChooseFieldCard(v) == 0, "FN05 AI: weakens the player cards");
  v.theirs = {Theirs(1, &kStrong, false, -kStrong.attack)};
  Check(ai::ChooseFieldCard(v) == -1, "FN05 AI: holds it when the player cards have no ATK left");

  const CardInfo blade = Battle("N301", 'N', 600, 600);
  ai::View a = EmptyView();
  a.mine = {Mine(0, &blade)};
  a.theirs = {Theirs(1, &brute, false)};
  Check(!ai::ChooseAttack(a), "FN05 AI: does not attack ATK 800 with ATK 600");
  a.theirs = {Theirs(1, &brute, false, -300)};
  const std::optional<ai::Attack> at = ai::ChooseAttack(a);
  Check(at && at->target_slot == 1, "FN05 AI: attacks the card it weakened");
}

void General() {
  const CardInfo aura = Aura("FG01", "good");
  std::vector<rules::BoardCard> board = {InDefense()};
  Check(!aura.IsInstant() && !kGood.IsInstant(), "lasting field and battle cards are not instant");
  const rules::InstantOutcome out = rules::ApplyInstant(aura, true, board);
  Check(!rules::HasTarget(aura, true, board) && !out.clear_field && out.damage == 0 &&
            out.changed.empty() && board[0].defense,
        "a lasting field card has no instant effect");

  const CardInfo storm = Instant("FN01", FieldEffect::kClearField);
  const CardInfo walls = Instant("FN04", FieldEffect::kWeakenDefense, 300);
  const CardInfo sling = Instant("FN03", FieldEffect::kDamage, 100);
  ai::View v = EmptyView();
  v.hand = {&storm, &walls, &sling};
  Check(ai::ChooseFieldCard(v) == 2, "AI skips cards without a target and plays the one that works");
  v.hand = {&storm, &walls};
  Check(ai::ChooseFieldCard(v) == -1, "AI keeps every instant without a target in hand");

  Check(std::string(cards::EffectName(FieldEffect::kAura)).empty() &&
            std::string(cards::EffectName(FieldEffect::kWeakenAttack)) == "weakenAttack",
        "effect names match the JSON");
}

void Catalog() {
  const std::vector<CardInfo> all = cards::LoadAll(PONTIFICE_SOURCE_DIR);
  struct Expect {
    const char* code;
    FieldEffect effect;
    int value;
  };
  const Expect expected[] = {
      {"FN01", FieldEffect::kClearField, 0},    {"FN02", FieldEffect::kAttackMode, 0},
      {"FN03", FieldEffect::kDamage, 100},      {"FN04", FieldEffect::kWeakenDefense, 300},
      {"FN05", FieldEffect::kWeakenAttack, 300},
  };
  for (const Expect& e : expected) {
    const CardInfo* c = cards::Find(all, e.code);
    const std::string what = std::string("catalog: ") + e.code + " is " +
                             cards::EffectName(e.effect) + " " + std::to_string(e.value);
    Check(c != nullptr && c->IsInstant() && c->effect == e.effect && c->value == e.value &&
              c->hierarchy == 0 && c->odds > 0 && c->favored.empty(),
          what.c_str());
  }
  for (const char* code : {"FE01", "FE02", "FE03", "FE04", "FG01", "FG02", "FG03", "FG04"}) {
    const CardInfo* c = cards::Find(all, code);
    const std::string what = std::string("catalog: ") + code + " stays a lasting field card";
    Check(c != nullptr && c->IsField() && !c->IsInstant() && c->buff > 0, what.c_str());
  }
}

}

int main() {
  CalmingOfTheStorm();
  GoliathsChallenge();
  DavidsSling();
  WallsOfJericho();
  SwordsIntoPlowshares();
  General();
  Catalog();
  if (failures == 0) LOG("instantTest: ok");
  return failures == 0 ? 0 : 1;
}
