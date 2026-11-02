#pragma once

#include <functional>
#include <random>
#include <string>
#include <vector>

// Dados das cartas (assets/Cards*.json, ver docs/cartas.md) e montagem do monte.
struct CardInfo {
  std::string code;         // "G101", "E203", "N301", "FE01"
  std::string name;
  std::string description;
  char category = 'N';      // 'G' bondosa, 'E' maligna, 'N' neutra, 'F' campo
  std::string category_name;   // "good", "evil", "neutral", "field"
  int hierarchy = 0;        // 1 (mais rara) a 4; 0 para cartas de campo
  std::string hierarchy_name;  // ex.: "Arcanjos" (traduzido); vazio em cartas de campo
  int attack = 0;
  int defense = 0;
  int odds = 1;             // peso no sorteio do monte (maior = mais comum)
  bool combinable = false;
  // Ilustração (imageUrl, relativo ao binário, ex.: "sprites/neutral/N301.png"); o arquivo
  // pode ainda não existir, e aí a janela da imagem fica em branco.
  std::string image_url;
  // Só cartas de campo: categoria favorecida ("good"/"evil"), buff e nerf.
  std::string favored;
  int buff = 0;
  int nerf = 0;

  bool IsField() const { return category == 'F'; }
  // Nome da categoria no idioma atual (category.good, .evil, .neutral, .field), para a interface.
  std::string CategoryLabel() const;
  // Moldura da frente em sprites/: CardFrontGood, CardFrontEvil, CardFrontNeutral ou
  // CardFrontField.
  const char* FrameSprite() const;
};

namespace cards {

// Lê CardsGood, CardsEvil, CardsNeutral e CardsField de `base`/assets. Vazio se nenhum abrir.
// Nome, descrição e nome da hierarquia já saem no idioma atual (assets/translate.json:
// card.<code>.name, card.<code>.description, hierarchy.<categoryName>.<hierarchy>); sem a
// chave, fica o texto do JSON da carta (em inglês).
std::vector<CardInfo> LoadAll(const std::string& base);

const CardInfo* Find(const std::vector<CardInfo>& all, const std::string& code);

// Monta um monte de `size` códigos sorteando por peso (`odds`, ou `weight` se dado; peso 0 não
// entra), com no máximo 2 cópias de cada carta e 1 das de hierarquia 1. A ordem já sai
// embaralhada (deck::Shuffle).
using Weight = std::function<int(const CardInfo&)>;
std::vector<std::string> BuildDeck(const std::vector<CardInfo>& all, int size, std::mt19937& rng,
                                   const Weight& weight = Weight());

}  // namespace cards
