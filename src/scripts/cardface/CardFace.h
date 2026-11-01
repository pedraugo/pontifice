#pragma once

#include "scripts/cards/Cards.h"
#include "scripts/image/Image.h"

// Frente de uma carta pronta para virar textura: moldura da categoria com os textos da carta
// desenhados por cima (fonte do jogo, via text::DrawOnImage). Layout em CardFace.cpp, medido
// na moldura de 1600x2230 e reduzido para kWidth x kHeight (a carta na mão é pequena na tela).
namespace cardface {

constexpr int kWidth = 800;
constexpr int kHeight = 1115;

// `frame` já em kWidth x kHeight (moldura reduzida 2x). CPU só: pode rodar em outra thread.
Image Compose(const Image& frame, const CardInfo& card);

}  // namespace cardface
