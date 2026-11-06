#pragma once

#include "scripts/cards/Cards.h"
#include "scripts/image/Image.h"

namespace cardface {

// Frame sprites (1600x2230) downscaled 2x; the layout below is in these units.
constexpr int kWidth = 800;
constexpr int kHeight = 1115;

// Art window: the white area between the name band and the ATK/DEF band.
constexpr int kArtLeft = 66;
constexpr int kArtTop = 129;
constexpr int kArtWidth = 668;
constexpr int kArtHeight = 448;

Image FitArt(const Image& source);

// `art` must already come from FitArt. CPU only: safe to run on another thread.
Image Compose(const Image& frame, const CardInfo& card, const Image* art = nullptr);

// Face for the opponent's hand: every text is "?" so the real card is not revealed.
Image ComposeHidden(const Image& frame);

}
