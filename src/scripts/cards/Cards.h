#pragma once

#include <random>
#include <string>
#include <vector>

// Dados das cartas (assets/Cards*.json, ver docs/cartas.md) e montagem do monte.
struct CardInfo {
  std::string code;         // "G101", "E203", "N301", "FE01"
  std::string name;
  std::string description;
  char category = 'N';      // 'G' bondosa, 'E' maligna, 'N' neutra, 'F' campo
  int hierarchy = 0;        // 1 (mais rara) a 4; 0 para cartas de campo
  int attack = 0;
  int defense = 0;
  int odds = 1;             // peso no sorteio do monte (maior = mais comum)
  bool combinable = false;
  // Só cartas de campo: categoria favorecida ("good"/"evil"), buff e nerf.
  std::string favored;
  int buff = 0;
  int nerf = 0;

  bool IsField() const { return category == 'F'; }
  // Moldura da frente em sprites/: Good, Evil ou Field (neutras e de campo usam a prateada).
  const char* FrameSprite() const;
};

namespace cards {

// Lê CardsGood, CardsEvil, CardsNeutral e CardsField de `base`/assets. Vazio se nenhum abrir.
std::vector<CardInfo> LoadAll(const std::string& base);

const CardInfo* Find(const std::vector<CardInfo>& all, const std::string& code);

// Monta um monte de `size` códigos sorteando por peso `odds`, com no máximo 2 cópias de cada
// carta e 1 das de hierarquia 1. A ordem já sai embaralhada (deck::Shuffle).
std::vector<std::string> BuildDeck(const std::vector<CardInfo>& all, int size, std::mt19937& rng);

}  // namespace cards
