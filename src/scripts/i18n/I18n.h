#pragma once

#include <initializer_list>
#include <string>
#include <vector>

// Textos do jogo em vários idiomas (assets/translate.json, ver docs/idioma.md). Cada chave
// ("menu.play", "card.G101.name") tem um texto por idioma ("pt", "en", "es"). Carregado uma
// vez em main antes da primeira cena; o idioma é escolhido na cena de idioma
// (scenes/language) e vale para todo o jogo. Sem o arquivo, ou sem a chave, o texto cai no
// português e depois na própria chave, para nunca sumir da tela.
//
// Só leitura depois de Load e SetLanguage: as consultas são seguras em outras threads (a
// frente das cartas é composta no Preload da batalha).
namespace i18n {

constexpr const char* kDefaultLanguage = "pt";

struct Language {
  std::string code;  // "en", "es", "pt": idioma nas chaves de translate.json
  std::string name;  // nome no próprio idioma, para o botão
  std::string flag;  // bandeira (scripts/flag): "us", "es", "br"
};

// Lê translate.json. false (e log) se o arquivo não abrir ou não for válido.
bool Load(const std::string& path);
// Idiomas na ordem do arquivo (ordem dos botões da cena de idioma).
const std::vector<Language>& Languages();

void SetLanguage(const std::string& code);
const std::string& CurrentLanguage();

// Texto da chave no idioma atual; cai no português, depois na própria chave.
std::string Get(const std::string& key);
// Igual a Get, mas com a chave ausente em todos os idiomas devolve `fallback` (cartas:
// mantém o texto do Cards*.json).
std::string GetOr(const std::string& key, const std::string& fallback);
// Get com "{0}", "{1}", ... trocados pelos argumentos, na ordem.
std::string Format(const std::string& key, std::initializer_list<std::string> args);

}  // namespace i18n
