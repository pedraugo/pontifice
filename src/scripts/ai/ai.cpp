#include "scripts/ai/ai.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace ai {

const char* DifficultyKey(Difficulty d) {
  switch (d) {
    case Difficulty::kEasy:
      return "menu.difficulty.easy";
    case Difficulty::kMedium:
      return "menu.difficulty.medium";
    case Difficulty::kHard:
      return "menu.difficulty.hard";
    case Difficulty::kImpossible:
      return "menu.difficulty.impossible";
  }
  return "menu.difficulty.medium";
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

cards::Tiers OpponentTiers(const cards::Tiers& player, Difficulty d) {
  // Percent of each hierarchy moved one step: negative to weaker, positive to stronger.
  static constexpr int kShift[4] = {-100, 0, 50, 100};
  const int shift = kShift[static_cast<int>(d)];
  cards::Tiers out{};
  out[0] = player[0];
  for (int h = 1; h <= 4; ++h) {
    const int target = std::clamp(shift > 0 ? h - 1 : h + 1, 1, 4);
    const int moved = player[h] * std::abs(shift) / 100;
    out[h] += player[h] - moved;
    out[target] += moved;
  }
  return out;
}

namespace {

int Atk(const CardInfo* c, const rules::Field& f) { return rules::EffectiveAttack(*c, f); }
int Def(const CardInfo* c, const rules::Field& f) { return rules::EffectiveDefense(*c, f); }
int Atk(const FieldCard& c, const rules::Field& f) {
  return rules::EffectiveAttack(*c.info, f, c.attack_mod);
}
int Def(const FieldCard& c, const rules::Field& f) {
  return rules::EffectiveDefense(*c.info, f, c.defense_mod);
}

int TheirGuard(const FieldCard& c, const View& v) {
  if (c.info == nullptr) return 0;
  return c.defense ? Def(c, v.field) : Atk(c, v.field);
}

int TheirMinGuard(const View& v) {
  int best = 1 << 30;
  for (const FieldCard& c : v.theirs) best = std::min(best, TheirGuard(c, v));
  return v.theirs.empty() ? 0 : best;
}

// Cards a field favoring `favored` helps on my side and hurts on theirs, minus the reverse.
int FieldGain(const View& view, char favored) {
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
  return favored == 'G' ? mine_g - mine_e + theirs_e - theirs_g
                        : mine_e - mine_g + theirs_g - theirs_e;
}

int AuraScore(const View& view, const CardInfo& card) {
  const char favored = card.favored == "evil" ? 'E' : 'G';
  if (view.field.active && view.field.favored == favored) return 0;
  return FieldGain(view, favored) * 100;
}

int MyBestAttack(const View& view) {
  int best = 0;
  for (const FieldCard& c : view.mine) {
    if (c.info != nullptr && c.can_attack) best = std::max(best, Atk(c, view.field));
  }
  return best;
}

int InstantScore(const View& view, const CardInfo& card) {
  switch (card.effect) {
    case FieldEffect::kClearField: {
      if (!view.field.active || view.field_mine) return 0;
      const int gain = FieldGain(view, view.field.favored);
      return gain < 0 ? -gain * 100 : 0;
    }
    case FieldEffect::kAttackMode: {
      if (!view.can_attack) return 0;
      const int best = MyBestAttack(view);
      int exposed = 0;
      for (const FieldCard& c : view.theirs) {
        if (c.info != nullptr && c.defense && Atk(c, view.field) < best) ++exposed;
      }
      return exposed * 100;
    }
    case FieldEffect::kDamage:
      if (card.value <= 0) return 0;
      return view.their_life <= card.value ? 1 << 20 : card.value / 10;
    case FieldEffect::kWeakenAttack: {
      int lost = 0;
      for (const FieldCard& c : view.theirs) {
        if (c.info != nullptr) lost += std::min(card.value, Atk(c, view.field));
      }
      return lost / 10;
    }
    case FieldEffect::kWeakenDefense: {
      if (!view.can_attack) return 0;
      int lost = 0;
      for (const FieldCard& c : view.theirs) {
        if (c.info != nullptr && c.defense) lost += std::min(card.value, Def(c, view.field));
      }
      return lost / 10;
    }
    default:
      return 0;
  }
}

}

int ChooseFieldCard(const View& view) {
  int best = -1;
  int best_score = 0;
  for (size_t i = 0; i < view.hand.size(); ++i) {
    const CardInfo* c = view.hand[i];
    if (c == nullptr || !c->IsField()) continue;
    const int score = c->IsInstant() ? InstantScore(view, *c) : AuraScore(view, *c);
    if (score > best_score) {
      best_score = score;
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
  // Field full: only worth replacing if the new card beats the weakest one on the field.
  if (full) {
    int worst = 1 << 30;
    for (const FieldCard& c : view.mine) {
      if (c.info != nullptr) worst = std::min(worst, Atk(c, view.field));
    }
    if (best_atk <= worst) return std::nullopt;
  }
  if (view.theirs.empty() || best_atk > TheirMinGuard(view)) return Summon{atk_index, false};
  return Summon{def_index, true};
}

int ChooseSlot(const std::vector<int>& free_slots, int cols) {
  if (free_slots.empty()) return -1;
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
  int their_best_atk = 0;
  for (const FieldCard& c : view.theirs) {
    if (c.info != nullptr && !c.defense) {
      their_best_atk = std::max(their_best_atk, Atk(c, view.field));
    }
  }
  const int min_guard = TheirMinGuard(view);
  for (const FieldCard& c : view.mine) {
    if (c.info == nullptr || c.summoned_this_turn) continue;
    const int a = Atk(c, view.field);
    const int d = Def(c, view.field);
    if (c.defense) {
      if (view.theirs.empty() || a > min_guard) out.push_back(c.slot);
    } else if (!view.theirs.empty() && a <= min_guard && their_best_atk > a && d > a) {
      out.push_back(c.slot);
    }
  }
  return out;
}

std::optional<Attack> ChooseAttack(const View& view) {
  if (!view.can_attack) return std::nullopt;
  std::optional<Attack> best;
  int best_score = 0;
  for (const FieldCard& me : view.mine) {
    if (me.info == nullptr || !me.can_attack || me.defense) continue;
    const int a = Atk(me, view.field);
    if (view.theirs.empty()) {
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
        if (a > guard) score = 150 + guard;  // removes a shield, no damage
      } else {
        if (a > guard) {
          score = 300 + (a - guard) + guard;  // destroys and deals damage
        } else if (a == guard) {
          score = 50;  // even trade: both destroyed, no damage
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

}
