// Testes das regras de combate (scripts/rules) e da IA (scripts/ai), sem SDL.
#include <cstdio>
#include <vector>

#include "scripts/ai/Ai.h"
#include "scripts/rules/Rules.h"

namespace {

int failures = 0;

void Check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FALHOU: %s\n", what);
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

}  // namespace

int main() {
  // Dano contra carta em modo de ataque.
  rules::Outcome o = rules::Resolve(1000, false, 700, 900);
  Check(o.defender_destroyed && !o.attacker_destroyed && o.defender_damage == 300 &&
            o.attacker_damage == 0,
        "ataque maior destrói defensor e dá a diferença");
  o = rules::Resolve(500, false, 700, 900);
  Check(o.attacker_destroyed && !o.defender_destroyed && o.attacker_damage == 200,
        "ataque menor destrói atacante e dá a diferença");
  o = rules::Resolve(700, false, 700, 900);
  Check(o.attacker_destroyed && o.defender_destroyed && o.attacker_damage == 0 &&
            o.defender_damage == 0,
        "empate destrói as duas sem dano");
  // Contra defesa.
  o = rules::Resolve(1000, true, 700, 900);
  Check(o.defender_destroyed && o.defender_damage == 0, "ataque maior que defesa destrói sem dano");
  o = rules::Resolve(500, true, 700, 900);
  Check(!o.defender_destroyed && !o.attacker_destroyed && o.attacker_damage == 400,
        "ataque menor que defesa: atacante perde a diferença");
  o = rules::Resolve(900, true, 700, 900);
  Check(!o.defender_destroyed && o.attacker_damage == 0, "igual à defesa: nada acontece");
  o = rules::ResolveDirect(1200);
  Check(o.defender_damage == 1200 && !o.defender_destroyed, "ataque direto desconta tudo");

  // Carta de campo: buff/nerf, neutras intactas.
  CardInfo field = Make("FE01", 'F', 0, 0);
  field.favored = "evil";
  field.buff = 300;
  field.nerf = 300;
  const rules::Field f = rules::Field::From(field);
  const CardInfo good = Make("G401", 'G', 500, 500);
  const CardInfo evil = Make("E401", 'E', 450, 200);
  const CardInfo neutral = Make("N401", 'N', 500, 500);
  Check(rules::EffectiveAttack(evil, f) == 750 && rules::EffectiveDefense(evil, f) == 500,
        "favorecida ganha buff");
  Check(rules::EffectiveAttack(good, f) == 200 && rules::EffectiveDefense(good, f) == 200,
        "prejudicada perde nerf");
  Check(rules::EffectiveAttack(neutral, f) == 500, "neutra não muda");
  Check(rules::EffectiveAttack(Make("E410", 'E', 250, 50), rules::Field{true, 'G', 300, 300}) == 0,
        "nunca fica negativo");

  // IA: invoca a mais forte em ataque quando bate o campo; em defesa a de maior defesa quando
  // não bate.
  const CardInfo strong = Make("G201", 'G', 1800, 1900);
  const CardInfo weak = Make("N409", 'N', 100, 100);
  const CardInfo wall = Make("G305", 'G', 500, 700);
  ai::View v;
  v.hand = {&weak, &strong, &wall};
  v.free_slots = 10;
  v.theirs.push_back(ai::FieldCard{0, &good, false, true, false});
  std::optional<ai::Summon> s = ai::ChooseSummon(v);
  Check(s && s->hand_index == 1 && !s->defense, "invoca a mais forte em ataque");
  v.hand = {&weak, &wall};
  s = ai::ChooseSummon(v);
  Check(s && s->hand_index == 1 && s->defense, "sem bater o campo, a de maior defesa em defesa");
  v.can_summon = false;
  Check(!ai::ChooseSummon(v), "sem invocação disponível não invoca");

  // IA: ataque direto com campo vazio; não ataca quem é mais forte; respeita can_attack.
  ai::View a;
  a.mine.push_back(ai::FieldCard{7, &strong, false, true, false});
  std::optional<ai::Attack> at = ai::ChooseAttack(a);
  Check(at && at->slot == 7 && at->target_slot == -1, "campo vazio: ataque direto");
  a.theirs.push_back(ai::FieldCard{2, &good, false, true, false});
  at = ai::ChooseAttack(a);
  Check(at && at->target_slot == 2, "ataca carta mais fraca em modo de ataque");
  a.mine[0].info = &weak;
  Check(!ai::ChooseAttack(a), "não ataca carta mais forte");
  a.mine[0].info = &strong;
  a.can_attack = false;
  Check(!ai::ChooseAttack(a), "primeiro turno de quem começou não ataca");

  // IA: slot preferido é a linha de trás, centro.
  Check(ai::ChooseSlot({0, 1, 2, 5, 6, 7, 9}, 5) == 7, "slot B3 preferido");
  // IA: carta de campo que favorece sua categoria dominante.
  ai::View fc;
  fc.hand = {&field, &evil};
  fc.mine.push_back(ai::FieldCard{5, &evil, false, true, false});
  Check(ai::ChooseFieldCard(fc) == 0, "joga carta de campo que favorece malignas");
  fc.mine[0].info = &good;
  fc.hand = {&field, &good};
  Check(ai::ChooseFieldCard(fc) == -1, "não joga campo que prejudica suas cartas");

  // Níveis: peso do monte do oponente por hierarquia.
  auto card_h = [](int h, int odds) {
    CardInfo c = Make("X", 'G', 0, 0);
    c.hierarchy = h;
    c.odds = odds;
    return c;
  };
  Check(ai::DeckWeight(card_h(1, 1), ai::Difficulty::kEasy) == 0 &&
            ai::DeckWeight(card_h(2, 8), ai::Difficulty::kEasy) == 0 &&
            ai::DeckWeight(card_h(4, 71), ai::Difficulty::kEasy) == 71,
        "fácil: sem hierarquia 1 e 2, comuns normais");
  for (int h = 1; h <= 4; ++h) {
    Check(ai::DeckWeight(card_h(h, 10), ai::Difficulty::kMedium) == 10, "médio: odds originais");
  }
  Check(ai::DeckWeight(card_h(2, 8), ai::Difficulty::kHard) > 8 &&
            ai::DeckWeight(card_h(4, 71), ai::Difficulty::kHard) < 71,
        "difícil: fortes reforçadas, fracas reduzidas");
  Check(ai::DeckWeight(card_h(4, 71), ai::Difficulty::kImpossible) == 0 &&
            ai::DeckWeight(card_h(1, 1), ai::Difficulty::kImpossible) >
                ai::DeckWeight(card_h(3, 15), ai::Difficulty::kImpossible),
        "impossível: sem hierarquia 4, 1 e 2 dominam");
  Check(ai::DeckWeight(field, ai::Difficulty::kEasy) == field.odds &&
            ai::DeckWeight(field, ai::Difficulty::kImpossible) == field.odds,
        "carta de campo igual em todos os níveis");
  Check(ai::NextDifficulty(ai::Difficulty::kImpossible) == ai::Difficulty::kEasy, "ciclo do nível");

  if (failures == 0) std::printf("rules_test: ok\n");
  return failures == 0 ? 0 : 1;
}
