#pragma once

#include "scripts/cards/Cards.h"
#include "scripts/image/Image.h"

// Frente de uma carta pronta para virar textura: moldura da categoria com os textos da carta
// desenhados por cima (fonte do jogo, via text::DrawOnImage). Layout em CardFace.cpp, medido
// na moldura de 1600x2230 e reduzido para kWidth x kHeight (a carta na mão é pequena na tela).
namespace cardface {

constexpr int kWidth = 800;
constexpr int kHeight = 1115;

// Janela da ilustração na moldura (px em kWidth x kHeight): a área branca entre o nome e a
// faixa de ATK/DEF, x 66..733 e y 129..576 (668 x 448).
constexpr int kArtLeft = 66;
constexpr int kArtTop = 129;
constexpr int kArtWidth = 668;
constexpr int kArtHeight = 448;

// Ilustração ajustada à janela: preenche kArtWidth x kArtHeight cortando o excesso, centrada
// (as de sprites/ têm 1340x900, o dobro da janela). Inválida se `source` for.
Image FitArt(const Image& source);

// `frame` já em kWidth x kHeight (moldura reduzida 2x); `art` (opcional) já de FitArt, vai
// para a janela da imagem antes dos textos. CPU só: pode rodar em outra thread.
Image Compose(const Image& frame, const CardInfo& card, const Image* art = nullptr);

// Frente oculta das cartas na mão do oponente (`frame` = CardFrontEnemy reduzida 2x): nome
// "?", "ATK ?" e "DEF ?", descrição "?" e sem hierarquia, para não revelar a carta real.
Image ComposeHidden(const Image& frame);

}  // namespace cardface
