#include "scripts/cardface/CardFace.h"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "scripts/text/Text.h"

namespace {

// Áreas da moldura (px em kWidth x kHeight; ver docs/detalhes_tecnicos.md, "Frente da carta").
constexpr float kNameTop = 58.0f, kNameBottom = 114.0f;      // faixa do topo
constexpr float kStatsTop = 630.0f, kStatsBottom = 690.0f;   // faixa abaixo da imagem
constexpr float kDescTop = 712.0f, kDescBottom = 1031.0f;    // área branca de baixo
constexpr float kLeft = 70.0f, kRight = 730.0f;
constexpr float kNamePx = 56.0f;
constexpr float kStatsPx = 48.0f;
constexpr float kDescPx = 40.0f;
constexpr float kDescLine = 50.0f;

constexpr SDL_Color kLight{241, 247, 253, 255};
constexpr SDL_Color kShadow{10, 12, 18, 200};
constexpr SDL_Color kDark{27, 27, 30, 255};

// Centraliza na caixa vertical [top, bottom] com `px` de em.
float CenteredTop(float top, float bottom, float px) { return (top + bottom) / 2.0f - px / 2.0f; }

void Shadowed(Image& img, const std::string& s, float x, float y, float px) {
  text::DrawOnImage(img, s.c_str(), x + 2.0f, y + 2.0f, px, kShadow);
  text::DrawOnImage(img, s.c_str(), x, y, px, kLight);
}

// Quebra em linhas que cabem em `max_width` (por palavras; palavra maior que a linha fica só).
std::vector<std::string> Wrap(const std::string& textstr, float px, float max_width) {
  std::vector<std::string> lines;
  std::string line, word;
  auto flush_word = [&]() {
    if (word.empty()) return;
    const std::string candidate = line.empty() ? word : line + " " + word;
    if (text::ImageWidth(candidate.c_str(), px) <= max_width || line.empty()) {
      line = candidate;
    } else {
      lines.push_back(line);
      line = word;
    }
    word.clear();
  };
  for (char c : textstr) {
    if (c == ' ' || c == '\n') {
      flush_word();
      if (c == '\n') {
        lines.push_back(line);
        line.clear();
      }
    } else {
      word.push_back(c);
    }
  }
  flush_word();
  if (!line.empty()) lines.push_back(line);
  return lines;
}

}  // namespace

namespace cardface {

Image Compose(const Image& frame, const CardInfo& card) {
  Image img = frame;
  if (!img.Valid()) return img;
  const float sx = static_cast<float>(img.width) / kWidth;  // tolera moldura em outra escala
  const float sy = static_cast<float>(img.height) / kHeight;
  const float cx = img.width / 2.0f;

  // Nome, centralizado na faixa do topo, claro com sombra (a faixa é colorida).
  {
    const float px = kNamePx * sy;
    const float w = text::ImageWidth(card.name.c_str(), px);
    Shadowed(img, card.name, cx - w / 2.0f, CenteredTop(kNameTop * sy, kNameBottom * sy, px), px);
  }

  // Abaixo da imagem: ataque à esquerda e defesa à direita; carta de campo mostra buff/nerf.
  {
    const float px = kStatsPx * sy;
    const float y = CenteredTop(kStatsTop * sy, kStatsBottom * sy, px);
    std::string left, right;
    if (card.IsField()) {
      left = "BUFF +" + std::to_string(card.buff);
      right = "NERF -" + std::to_string(card.nerf);
    } else {
      left = "ATK " + std::to_string(card.attack);
      right = "DEF " + std::to_string(card.defense);
    }
    Shadowed(img, left, kLeft * sx, y, px);
    Shadowed(img, right, kRight * sx - text::ImageWidth(right.c_str(), px), y, px);
  }

  // Descrição na área branca de baixo, com quebra de linha e centralizada.
  {
    const float px = kDescPx * sy;
    const float line_h = kDescLine * sy;
    std::string body = card.description;
    if (card.IsField() && !card.favored.empty()) {
      body += "\nFavorece: " + std::string(card.favored == "evil" ? "Malignas" : "Bondosas");
    }
    const std::vector<std::string> lines = Wrap(body, px, (kRight - kLeft) * sx);
    const float block_h = static_cast<float>(lines.size()) * line_h;
    float y = CenteredTop(kDescTop * sy, kDescBottom * sy, block_h);
    for (const std::string& line : lines) {
      const float w = text::ImageWidth(line.c_str(), px);
      text::DrawOnImage(img, line.c_str(), cx - w / 2.0f, y, px, kDark);
      y += line_h;
    }
  }
  return img;
}

}  // namespace cardface
