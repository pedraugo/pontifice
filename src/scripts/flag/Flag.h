#pragma once

#include <SDL3/SDL.h>

#include <string>

// Bandeiras desenhadas com primitivas do renderer (sem sprite), para a cena de idioma
// (docs/idioma.md): Estados Unidos (inglês), Espanha (espanhol) e Brasil (português).
// Simplificadas para ler bem em ~48 px de altura: sem brasão na espanhola e sem a faixa
// "Ordem e Progresso" na brasileira; as estrelas da americana viram uma grade de pontos.
namespace flag {

enum class Country { kUnitedStates, kSpain, kBrazil };

// Código de translate.json ("us", "es", "br"); desconhecido vira Brasil.
Country FromCode(const std::string& code);

// Desenha a bandeira ocupando `rect` (proporção ideal 3:2), com borda escura fina.
// `saturation`: 1 = cores reais; 0 = preto e branco (cena de idioma sem hover/foco).
void Draw(SDL_Renderer* r, Country country, const SDL_FRect& rect, float saturation = 1.0f);

}  // namespace flag
