#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "scripts/json/Json.h"
#include "scripts/math/Math.h"

// Ajustes do jogo lidos de JSON em assets/ (docs/detalhes_tecnicos.md, "Ajustes em JSON"):
// Light.json (iluminação), Fog.json (névoa, nuvens, clima) e Shadow.json (sombras). Cada
// arquivo é registrado por um nome ("light", "fog", "shadow") em main antes da primeira cena
// e consultado por caminho com pontos ("battle.arena.ambient"). Valor ausente ou de tipo
// errado devolve o padrão passado e avisa no log uma vez, então o jogo roda mesmo com um
// arquivo incompleto. Só leitura depois de Load: seguro em outras threads (Preload).
namespace config {

// Lê e registra o arquivo sob `name`. false (e log) se não abrir ou não for JSON válido.
bool Load(const std::string& name, const std::string& path);

// Nó em `path` do arquivo `name`, ou nullptr se o arquivo ou o caminho não existem.
const json::Value* Node(const std::string& name, const std::string& path);

float Float(const std::string& name, const std::string& path, float fallback);
int Int(const std::string& name, const std::string& path, int fallback);
// Array [x, y, z].
math::Vec3 Vec3(const std::string& name, const std::string& path, const math::Vec3& fallback);
// "#rrggbb"/"#rrggbbaa", nome de papel de assets/colors.json ("background.fog") ou array
// [r, g, b] / [r, g, b, a] em 0..1.
SDL_FColor Color(const std::string& name, const std::string& path, const SDL_FColor& fallback);

// Mesmas leituras a partir de um objeto já em mãos (itens de arrays, ex.: camadas de névoa).
float Float(const json::Value& object, const std::string& key, float fallback);
std::string String(const json::Value& object, const std::string& key, const std::string& fallback);
SDL_FColor Color(const json::Value& object, const std::string& key, const SDL_FColor& fallback);

}  // namespace config
