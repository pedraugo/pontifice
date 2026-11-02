#pragma once

#include <optional>
#include <random>
#include <vector>

#include "scripts/cards/Cards.h"
#include "scripts/rules/Rules.h"

// Inteligência do oponente (Vs PC). Recebe uma fotografia do jogo do ponto de vista do
// computador e decide uma ação por vez; a cena executa e pergunta de novo. Todas as cartas
// em campo ficam viradas para cima, então a IA vê o que o jogador vê. Heurísticas, sem busca:
// - Carta de campo: joga se favorece a categoria que ele tem mais em jogo e na mão.
// - Invocação: a carta de maior ataque efetivo, em ataque se consegue destruir alguma carta
//   do jogador (ou se o campo dele está vazio); senão a de maior defesa, em modo de defesa.
// - Posição: carta em defesa que consegue destruir algo (ou com o campo do jogador vazio)
//   passa a ataque; em ataque que não destrói nada e perderia do atacante mais forte do
//   jogador passa a defesa (nunca no turno em que entrou).
// - Ataques: primeiro os que destroem sem risco e com dano, depois os seguros; ataque direto
//   com todas quando o campo do jogador está vazio.
namespace ai {

// Nível do oponente: muda a força do monte dele em relação ao seu (peso de sorteio por
// hierarquia, ver DeckWeight). A tomada de decisão é a mesma em todos os níveis.
// - Fácil: cartas inferiores às suas (quase só hierarquia 4, nenhuma 1 ou 2).
// - Médio: mesma distribuição do seu monte.
// - Difícil: na maioria das vezes mais fortes (hierarquias 2 e 3 reforçadas).
// - Impossível: nitidamente mais fortes (todas as de hierarquia 1 e 2, resto 3).
enum class Difficulty { kEasy, kMedium, kHard, kImpossible };
// Chave do nome em assets/translate.json ("difficulty.easy", ...); traduzir com i18n::Get.
const char* DifficultyKey(Difficulty d);
Difficulty NextDifficulty(Difficulty d);   // cicla (seletor do menu)
// Peso de sorteio de `card` no monte do computador para o nível (0 = não entra).
int DeckWeight(const CardInfo& card, Difficulty d);

struct FieldCard {
  int slot = -1;
  const CardInfo* info = nullptr;
  bool defense = false;
  bool can_attack = false;        // em ataque, ainda não atacou neste turno
  bool summoned_this_turn = false;
};

struct View {
  std::vector<const CardInfo*> hand;  // mão do computador
  std::vector<FieldCard> mine;        // cartas do computador no campo
  std::vector<FieldCard> theirs;      // cartas do jogador no campo
  rules::Field field;                 // carta de campo ativa
  int my_life = 0;
  int their_life = 0;
  bool can_summon = true;   // invocação normal ainda disponível
  bool can_attack = true;   // falso no primeiro turno de quem começou
  int free_slots = 0;       // slots vazios do computador
};

struct Summon {
  int hand_index = -1;
  bool defense = false;    // entra em modo de defesa em vez de ataque
};
// Carta de campo da mão que vale a pena jogar agora, ou -1.
int ChooseFieldCard(const View& view);
// Invocação do turno, ou nada se não compensa (mão vazia, sem slot, etc.).
std::optional<Summon> ChooseSummon(const View& view);
// Slot para a carta: prefere a linha de trás (B, junto ao dono) e colunas centrais.
int ChooseSlot(const std::vector<int>& free_slots, int cols);
// Trocas de posição (slots) a fazer neste turno.
std::vector<int> ChoosePositionChanges(const View& view);

struct Attack {
  int slot = -1;         // atacante (do computador)
  int target_slot = -1;  // carta do jogador; -1 = ataque direto
};
// Próximo ataque, ou nada para encerrar a fase de batalha.
std::optional<Attack> ChooseAttack(const View& view);

}  // namespace ai
