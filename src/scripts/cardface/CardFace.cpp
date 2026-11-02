#include "scripts/cardface/CardFace.h"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "scripts/i18n/I18n.h"
#include "scripts/text/Text.h"

namespace {

// Áreas da moldura (px em kWidth x kHeight; ver docs/detalhes_tecnicos.md, "Frente da carta"),
// medidas nas molduras CardFront*.png: a faixa de ATK/DEF é alta (quase 100 px) para os
// números saírem grandes, e a área branca da descrição vai até perto da base.
constexpr float kNameTop = 58.0f, kNameBottom = 114.0f;      // faixa do topo
constexpr float kStatsTop = 592.0f, kStatsBottom = 688.0f;   // faixa abaixo da imagem
constexpr float kDescTop = 712.0f, kDescBottom = 1056.0f;    // área branca de baixo
constexpr float kLeft = 70.0f, kRight = 730.0f;
constexpr float kNamePx = 56.0f;
constexpr float kStatsPx = 76.0f;  // ATK/DEF (BUFF/NERF): ~80% da altura da faixa
constexpr float kDescPx = 40.0f;
constexpr float kDescLine = 50.0f;

// Nome, hierarquia e ATK/DEF: brancos com contorno escuro e sombra (ficam sobre faixas
// coloridas). Descrição: preta lisa, sobre a área branca.
constexpr SDL_Color kWhite{255, 255, 255, 255};
constexpr SDL_Color kOutline{12, 12, 16, 255};
constexpr SDL_Color kShadow{0, 0, 0, 150};
constexpr SDL_Color kInk{20, 20, 24, 255};

// Centraliza na caixa vertical [top, bottom] com `px` de em.
float CenteredTop(float top, float bottom, float px) { return (top + bottom) / 2.0f - px / 2.0f; }

void Label(Image& img, const std::string& s, float x, float y, float px) {
  const char* str = s.c_str();
  const float t = SDL_max(1.5f, px * 0.055f);   // espessura do contorno
  const float shadow = SDL_max(2.0f, px * 0.08f);
  text::DrawOnImage(img, str, x + shadow, y + shadow, px, kShadow);
  // Contorno: o texto em escuro nas 8 direções em volta.
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) continue;
      text::DrawOnImage(img, str, x + dx * t, y + dy * t, px, kOutline);
    }
  }
  text::DrawOnImage(img, str, x, y, px, kWhite);
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

Image FitArt(const Image& source) {
  if (!source.Valid()) return Image{};
  // Escala de "cobrir" a janela; reduções grandes passam antes por média de blocos (sem
  // serrilhado), e o resto é amostragem bilinear.
  Image src = source;
  float scale = SDL_max(static_cast<float>(kArtWidth) / src.width,
                        static_cast<float>(kArtHeight) / src.height);
  const int factor = static_cast<int>(1.0f / scale);
  if (factor >= 2) {
    src = src.Downscaled(factor);
    scale = SDL_max(static_cast<float>(kArtWidth) / src.width,
                    static_cast<float>(kArtHeight) / src.height);
  }
  Image out;
  out.width = kArtWidth;
  out.height = kArtHeight;
  out.rgba.resize(static_cast<size_t>(kArtWidth) * kArtHeight * 4);
  const float ox = (src.width - kArtWidth / scale) * 0.5f;  // corte centrado
  const float oy = (src.height - kArtHeight / scale) * 0.5f;
  auto at = [&src](int x, int y, int c) {
    x = SDL_clamp(x, 0, src.width - 1);
    y = SDL_clamp(y, 0, src.height - 1);
    return static_cast<float>(src.rgba[(static_cast<size_t>(y) * src.width + x) * 4 + c]);
  };
  for (int y = 0; y < kArtHeight; ++y) {
    const float fy = oy + (y + 0.5f) / scale - 0.5f;
    const int y0 = static_cast<int>(SDL_floorf(fy));
    const float ty = fy - y0;
    for (int x = 0; x < kArtWidth; ++x) {
      const float fx = ox + (x + 0.5f) / scale - 0.5f;
      const int x0 = static_cast<int>(SDL_floorf(fx));
      const float tx = fx - x0;
      uint8_t* o = &out.rgba[(static_cast<size_t>(y) * kArtWidth + x) * 4];
      for (int c = 0; c < 3; ++c) {
        const float top = at(x0, y0, c) * (1 - tx) + at(x0 + 1, y0, c) * tx;
        const float bottom = at(x0, y0 + 1, c) * (1 - tx) + at(x0 + 1, y0 + 1, c) * tx;
        o[c] = static_cast<uint8_t>(top * (1 - ty) + bottom * ty + 0.5f);
      }
      o[3] = 255;
    }
  }
  return out;
}

Image Compose(const Image& frame, const CardInfo& card, const Image* art) {
  Image img = frame;
  if (!img.Valid()) return img;
  const float sx = static_cast<float>(img.width) / kWidth;  // tolera moldura em outra escala
  const float sy = static_cast<float>(img.height) / kHeight;
  const float cx = img.width / 2.0f;

  // Ilustração na janela da imagem (opaca, substitui o branco da moldura). Com a moldura em
  // outra escala, amostra pelo vizinho mais próximo.
  if (art != nullptr && art->Valid()) {
    const int left = static_cast<int>(kArtLeft * sx), top = static_cast<int>(kArtTop * sy);
    const int w = static_cast<int>(kArtWidth * sx), h = static_cast<int>(kArtHeight * sy);
    for (int y = 0; y < h && top + y < img.height; ++y) {
      const int ay = SDL_min(art->height - 1, y * art->height / SDL_max(h, 1));
      for (int x = 0; x < w && left + x < img.width; ++x) {
        const int ax = SDL_min(art->width - 1, x * art->width / SDL_max(w, 1));
        const uint8_t* s = &art->rgba[(static_cast<size_t>(ay) * art->width + ax) * 4];
        uint8_t* d = &img.rgba[(static_cast<size_t>(top + y) * img.width + left + x) * 4];
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        d[3] = 255;
      }
    }
  }

  // Faixa do topo: nome alinhado à esquerda e número da hierarquia à direita (cartas de
  // campo não têm hierarquia).
  {
    const float px = kNamePx * sy;
    const float y = CenteredTop(kNameTop * sy, kNameBottom * sy, px);
    Label(img, card.name, kLeft * sx, y, px);
    if (card.hierarchy > 0) {
      const std::string tier = std::to_string(card.hierarchy);
      Label(img, tier, kRight * sx - text::ImageWidth(tier.c_str(), px), y, px);
    }
  }

  // Abaixo da imagem: ataque à esquerda e defesa à direita; carta de campo mostra buff/nerf.
  {
    const float px = kStatsPx * sy;
    const float y = CenteredTop(kStatsTop * sy, kStatsBottom * sy, px);
    std::string left, right;
    if (card.IsField()) {
      left = i18n::Get("cardface.buff") + " +" + std::to_string(card.buff);
      right = i18n::Get("cardface.nerf") + " -" + std::to_string(card.nerf);
    } else {
      left = i18n::Get("cardface.atk") + " " + std::to_string(card.attack);
      right = i18n::Get("cardface.def") + " " + std::to_string(card.defense);
    }
    Label(img, left, kLeft * sx, y, px);
    Label(img, right, kRight * sx - text::ImageWidth(right.c_str(), px), y, px);
  }

  // Descrição na área branca de baixo, com quebra de linha e centralizada.
  {
    const float px = kDescPx * sy;
    const float line_h = kDescLine * sy;
    std::string body = card.description;
    if (card.IsField() && !card.favored.empty()) {
      const std::string favored =
          i18n::Get(card.favored == "evil" ? "category.evil.plural" : "category.good.plural");
      body += "\n" + i18n::Format("cardface.favors", {favored});
    }
    const std::vector<std::string> lines = Wrap(body, px, (kRight - kLeft) * sx);
    const float block_h = static_cast<float>(lines.size()) * line_h;
    float y = CenteredTop(kDescTop * sy, kDescBottom * sy, block_h);
    for (const std::string& line : lines) {
      const float w = text::ImageWidth(line.c_str(), px);
      text::DrawOnImage(img, line.c_str(), cx - w / 2.0f, y, px, kInk);
      y += line_h;
    }
  }
  return img;
}

}  // namespace cardface
