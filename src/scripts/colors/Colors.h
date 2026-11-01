#pragma once

#include <SDL3/SDL.h>

#include <string>

// Paleta do jogo, lida de assets/colors.json (cores extraídas do ícone). Cada cor é pedida
// pelo papel que cumpre ("panel.fill", "text.primary", "status.buff"), nunca pelo tom.
// Sem o arquivo, ou para um papel desconhecido, devolve magenta e avisa no log uma vez.
namespace colors {

// Carrega "roles.*.*" e "palette.*" do arquivo. Pode ser chamado de novo para recarregar.
bool Load(const std::string& path);

SDL_Color Get(const std::string& role);
SDL_FColor GetF(const std::string& role);

// "#rrggbb" ou "#rrggbbaa". Retorna false se o texto não tem esse formato.
bool ParseHex(const std::string& hex, SDL_Color& out);

}  // namespace colors
