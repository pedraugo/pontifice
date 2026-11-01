// Testes do monte (scripts/deck): embaralhar preserva as cartas, é determinístico por semente,
// muda a ordem e comprar tira do topo. Sem framework: falha encerra com código != 0.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "scripts/deck/Deck.h"

namespace {

int failures = 0;

void Check(bool ok, const char* what) {
  if (!ok) {
    std::fprintf(stderr, "FALHOU: %s\n", what);
    ++failures;
  }
}

std::vector<std::string> SampleDeck(int n) {
  std::vector<std::string> cards;
  for (int i = 0; i < n; ++i) cards.push_back("C" + std::to_string(100 + i));
  return cards;
}

}  // namespace

int main() {
  const std::vector<std::string> original = SampleDeck(40);

  // Mesmas cartas depois de embaralhar (nenhuma perdida ou duplicada).
  deck::Deck a(original);
  a.Shuffle(42u);
  std::vector<std::string> sorted = a.Cards();
  std::sort(sorted.begin(), sorted.end());
  Check(sorted == original, "embaralhar preserva o conjunto de cartas");
  Check(a.Size() == 40, "tamanho continua 40");

  // Determinístico por semente; sementes diferentes dão ordens diferentes.
  deck::Deck b(original);
  b.Shuffle(42u);
  Check(a.Cards() == b.Cards(), "mesma semente, mesma ordem");
  deck::Deck c(original);
  c.Shuffle(7u);
  Check(c.Cards() != a.Cards(), "sementes diferentes, ordens diferentes");

  // De fato embaralha (40 cartas na mesma ordem por acaso: probabilidade desprezível).
  Check(a.Cards() != original, "ordem muda em relacao ao original");

  // Semente aleatória também preserva o conjunto.
  deck::Deck d(original);
  d.Shuffle();
  sorted = d.Cards();
  std::sort(sorted.begin(), sorted.end());
  Check(sorted == original, "semente aleatoria preserva o conjunto");

  // Comprar tira do topo (fim do vetor) até esvaziar.
  deck::Deck e(SampleDeck(3));
  Check(e.Draw() == std::string("C102"), "compra tira o topo");
  Check(e.Draw() == std::string("C101"), "segunda compra");
  e.PutOnBottom("X");
  Check(e.Draw() == std::string("C100"), "carta posta embaixo nao e o topo");
  Check(e.Draw() == std::string("X"), "depois sai a de baixo");
  Check(!e.Draw().has_value(), "monte vazio devolve nada");
  Check(e.Empty(), "monte vazio");

  // Casos pequenos: 0 e 1 carta não quebram.
  deck::Deck empty;
  empty.Shuffle(1u);
  Check(empty.Empty(), "embaralhar vazio");
  deck::Deck one(SampleDeck(1));
  one.Shuffle(1u);
  Check(one.Cards() == SampleDeck(1), "embaralhar uma carta");

  if (failures == 0) std::printf("deck_test: ok\n");
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
