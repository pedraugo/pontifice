#include "scripts/ai/Ai.h"

#include <algorithm>
#include <cmath>

namespace ai {

const char* DifficultyKey(Difficulty d) {
  switch (d) {
    case Difficulty::kEasy:
      return "difficulty.easy";
    case Difficulty::kMedium:
      return "difficulty.medium";
    case Difficulty::kHard:
      return "difficulty.hard";
    case Difficulty::kImpossible:
      return "difficulty.impossible";
  }
  return "difficulty.medium";
}

Difficulty NextDifficulty(Difficulty d) {
  switch (d) {
    case Difficulty::kEasy:
      return Difficulty::kMedium;
    case Difficulty::kMedium:
      return Difficulty::kHard;
    case Difficulty::kHard:
      return Difficulty::kImpossible;
    case Difficulty::kImpossible:
      return Difficulty::kEasy;
  }
  return Difficulty::kMedium;
}

int DeckWeight(const CardInfo& card, Difficulty d) {
  if (card.IsField()) return card.odds;  // cartas de campo iguais em todos os níveis
  // Multiplicadores por hierarquia (1 = mais rara e forte ... 4 = comum e fraca), em %.
  // Médio mantém os odds do baralho. Os limites de cópias (2, e 1 para hierarquia 1) do
  // sorteio continuam valendo, então "Impossível" enche com 1 e 2 e completa com 3.
  static constexpr int kPercent[4][4] = {
      {0, 0, 40, 100},        // fácil: quase só hierarquia 4
      {100, 100, 100, 100},   // médio: igual ao seu
      {600, 1500, 400, 30},   // difícil: 2 e 3 dominam, algumas 1, poucas 4
      {100000, 100000, 2000, 0},  // impossível: todas as 1 e 2 primeiro, resto 3
  };
  const int h = card.hierarchy < 1 ? 1 : (card.hierarchy > 4 ? 4 : card.hierarchy);
  const long weight = static_cast<long>(card.odds) * kPercent[static_cast<int>(d)][h - 1] / 100;
  return weight > 0 ? static_cast<int>(weight) : (kPercent[static_cast<int>(d)][h - 1] > 0 ? 1 : 0);
}

namespace {

int Atk(const CardInfo* c, const rules::Field& f) { return rules::EffectiveAttack(*c, f); }
int Def(const CardInfo* c, const rules::Field& f) { return rules::EffectiveDefense(*c, f); }

// O que a carta do jogador "vale" como alvo: ataque (modo ataque) ou defesa (modo defesa).
int TheirGuard(const FieldCard& c, const View& v) {
  if (c.info == nullptr) return 0;
  return c.defense ? Def(c.info, v.field) : Atk(c.info, v.field);
}

// Alvo mais fraco do jogador: um ataque acima disso destrói alguma carta. Campo vazio: 0.
int TheirMinGuard(const View& v) {
  int best = 1 << 30;
  for (const FieldCard& c : v.theirs) best = std::min(best, TheirGuard(c, v));
  return v.theirs.empty() ? 0 : best;
}

}  // namespace

int ChooseFieldCard(const View& view) {
  // Conta categorias do computador (campo + mão) e do jogador (campo visível).
  int mine_g = 0, mine_e = 0, theirs_g = 0, theirs_e = 0;
  for (const FieldCard& c : view.mine) {
    if (c.info == nullptr) continue;
    mine_g += c.info->category == 'G';
    mine_e += c.info->category == 'E';
  }
  for (const CardInfo* c : view.hand) {
    if (c == nullptr || c->IsField()) continue;
    mine_g += c->category == 'G';
    mine_e += c->category == 'E';
  }
  for (const FieldCard& c : view.theirs) {
    if (c.info == nullptr) continue;
    theirs_g += c.info->category == 'G';
    theirs_e += c.info->category == 'E';
  }
  int best = -1;
  int best_gain = 0;
  for (size_t i = 0; i < view.hand.size(); ++i) {
    const CardInfo* c = view.hand[i];
    if (c == nullptr || !c->IsField()) continue;
    const char favored = c->favored == "evil" ? 'E' : 'G';
    if (view.field.active && view.field.favored == favored) continue;  // já vale isso
    // Ganho: minhas cartas favorecidas + cartas do jogador prejudicadas, menos o contrário.
    const int gain = (favored == 'G' ? mine_g - mine_e + theirs_e - theirs_g
                                     : mine_e - mine_g + theirs_g - theirs_e);
    if (gain > best_gain) {
      best_gain = gain;
      best = static_cast<int>(i);
    }
  }
  return best;
}

std::optional<Summon> ChooseSummon(const View& view) {
  if (!view.can_summon || view.hand.empty()) return std::nullopt;
  const bool full = view.free_slots <= 0;
  int best_atk = -1, best_def = -1;
  int atk_index = -1, def_index = -1;
  for (size_t i = 0; i < view.hand.size(); ++i) {
    const CardInfo* c = view.hand[i];
    if (c == nullptr || c->IsField()) continue;
    const int a = Atk(c, view.field);
    const int d = Def(c, view.field);
    if (a > best_atk) {
      best_atk = a;
      atk_index = static_cast<int>(i);
    }
    if (d > best_def) {
      best_def = d;
      def_index = static_cast<int>(i);
    }
  }
  if (atk_index < 0) return std::nullopt;
  // Campo cheio: só vale substituir se a nova carta é melhor que a pior em campo.
  if (full) {
    int worst = 1 << 30;
    for (const FieldCard& c : view.mine) {
      if (c.info != nullptr) worst = std::min(worst, Atk(c.info, view.field));
    }
    if (best_atk <= worst) return std::nullopt;
  }
  // Em ataque se consegue destruir alguma carta do jogador (ou o campo dele está vazio);
  // senão entra a de maior defesa, em modo de defesa.
  if (view.theirs.empty() || best_atk > TheirMinGuard(view)) return Summon{atk_index, false};
  return Summon{def_index, true};
}

int ChooseSlot(const std::vector<int>& free_slots, int cols) {
  if (free_slots.empty()) return -1;
  // Linha de trás (índices maiores) e colunas centrais primeiro.
  int best = free_slots.front();
  float best_score = -1e9f;
  for (int slot : free_slots) {
    const int row = slot / cols;
    const int col = slot % cols;
    const float center = std::fabs(static_cast<float>(col) - (cols - 1) / 2.0f);
    const float score = row * 10.0f - center;
    if (score > best_score) {
      best_score = score;
      best = slot;
    }
  }
  return best;
}

std::vector<int> ChoosePositionChanges(const View& view) {
  std::vector<int> out;
  int their_best_atk = 0;  // maior ataque do jogador em modo de ataque (visível)
  for (const FieldCard& c : view.theirs) {
    if (c.info != nullptr && !c.defense) {
      their_best_atk = std::max(their_best_atk, Atk(c.info, view.field));
    }
  }
  const int min_guard = TheirMinGuard(view);
  for (const FieldCard& c : view.mine) {
    if (c.info == nullptr || c.summoned_this_turn) continue;
    const int a = Atk(c.info, view.field);
    const int d = Def(c.info, view.field);
    if (c.defense) {
      // Levanta se consegue destruir alguma carta do jogador ou se o campo dele está vazio
      // (ataque direto).
      if (view.theirs.empty() || a > min_guard) out.push_back(c.slot);
    } else if (!view.theirs.empty() && a <= min_guard && their_best_atk > a && d > a) {
      // Não destrói nada, perderia para o atacante mais forte do jogador e defende melhor
      // do que ataca: abaixa.
      out.push_back(c.slot);
    }
  }
  return out;
}

std::optional<Attack> ChooseAttack(const View& view) {
  if (!view.can_attack) return std::nullopt;
  std::optional<Attack> best;
  int best_score = 0;  // só ataques com pontuação positiva
  for (const FieldCard& me : view.mine) {
    if (me.info == nullptr || !me.can_attack || me.defense) continue;
    const int a = Atk(me.info, view.field);
    if (view.theirs.empty()) {
      // Ataque direto: o mais forte primeiro.
      if (a > best_score) {
        best_score = a;
        best = Attack{me.slot, -1};
      }
      continue;
    }
    for (const FieldCard& t : view.theirs) {
      const int guard = TheirGuard(t, view);
      int score = 0;
      if (t.info == nullptr) {
        continue;
      } else if (t.defense) {
        if (a > guard) score = 150 + guard;  // tira um escudo; sem dano
      } else {
        if (a > guard) {
          score = 300 + (a - guard) + guard;  // destrói e causa dano
        } else if (a == guard) {
          score = 50;  // troca igual: as duas saem, sem dano
        }
      }
      if (score > best_score) {
        best_score = score;
        best = Attack{me.slot, t.slot};
      }
    }
  }
  return best;
}

}  // namespace ai
