#include "scripts/text/Text.h"

#include <cmath>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb_truetype.h>

namespace {

constexpr float kCellPx = 8.0f;  // altura da linha por unidade de scale (fonte de depuração)
// Em (altura ascent..descent) por altura de linha, por fonte. A fonte de depuração enchia a
// célula de 8 px; na PirataOne as maiúsculas ocupam bem menos do em, então o em é ampliado
// para o texto manter o tamanho visual com os mesmos `scale`. A Newsreader é ~1,8x mais larga
// que a PirataOne no mesmo em: sem ampliação, a linha de situação da batalha ainda cabe entre
// os painéis do topo.
constexpr float kTitleEmBoost = 1.6f;
constexpr float kBodyEmBoost = 1.0f;
constexpr int kAtlasSize = 2048;
// Dois intervalos de codepoints no atlas: ASCII imprimível e Latin-1 (sinais como ¿ ¡ º e
// letras acentuadas).
constexpr int kFirstChar = 32;
constexpr int kNumChars = 95;  // 32..126
constexpr int kFirstLatin = 0xA0;
constexpr int kNumLatin = 96;  // 0xA0..0xFF
constexpr int kNumGlyphs = kNumChars + kNumLatin;
// Tamanhos do atlas, escolhidos pelos PIXELS DA TELA (tamanho lógico x razão de pixels da
// janela, 2x no Retina), nunca ampliados: até 128 px vem do atlas; acima disso o texto é
// rasterizado no tamanho exato numa textura própria (cache, ver DrawBig).
constexpr float kSizes[] = {16.0f, 32.0f, 64.0f, 128.0f};
constexpr int kNumSizes = 4;
// Sobreamostragem por tamanho: 2x2 nos pequenos (nitidez), 2x1 em 64 e nenhuma em 128, para
// os quatro tamanhos caberem juntos no atlas de 2048.
constexpr unsigned kOversampleX[] = {2, 2, 2, 1};
constexpr unsigned kOversampleY[] = {2, 2, 1, 1};
// Textos maiores que o atlas, rasterizados no tamanho exato (título do menu, resultado da
// batalha, itens no Retina): cache por (fonte, tamanho em px, texto).
constexpr size_t kBigCacheMax = 64;

struct BigText {
  std::string key;
  SDL_Texture* texture = nullptr;
  int width = 0;   // px de dispositivo
  int height = 0;
  float ascent = 0.0f;  // px de dispositivo, do topo da textura (com margem) à linha de base
};

struct Font {
  SDL_Texture* atlas = nullptr;
  std::vector<unsigned char> file;
  stbtt_fontinfo info{};
  stbtt_packedchar chars[kNumSizes][kNumGlyphs];
  float ascent[kNumSizes];   // px acima da linha de base em cada tamanho
  float descent[kNumSizes];  // px abaixo (negativo)
  float em_boost = kTitleEmBoost;  // em = kCellPx * scale * em_boost
  bool ready = false;
};

Font& G(text::Face face) {
  static Font fonts[2];
  return fonts[face == text::Face::kBody ? 1 : 0];
}

std::deque<BigText>& BigCache() {
  static std::deque<BigText> cache;
  return cache;
}

// Razão pixels de dispositivo / pixels lógicos (ex.: 2,0 em Retina com 1920x1080 lógico).
// Atualizada a cada Draw (tem o renderer); Width usa o último valor.
float& PixelRatioRef() {
  static float ratio = 1.0f;
  return ratio;
}

void UpdatePixelRatio(SDL_Renderer* r) {
  int out_w = 0, out_h = 0, logical_w = 0, logical_h = 0;
  SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
  if (!SDL_GetRenderOutputSize(r, &out_w, &out_h)) return;
  if (!SDL_GetRenderLogicalPresentation(r, &logical_w, &logical_h, &mode)) return;
  if (mode == SDL_LOGICAL_PRESENTATION_DISABLED || logical_w <= 0 || logical_h <= 0) {
    float sx = 1.0f, sy = 1.0f;
    SDL_GetRenderScale(r, &sx, &sy);
    PixelRatioRef() = SDL_max(sx, 0.01f);
    return;
  }
  // Letterbox: mesma escala nos dois eixos, a menor que cabe.
  const float ratio = SDL_min(static_cast<float>(out_w) / static_cast<float>(logical_w),
                              static_cast<float>(out_h) / static_cast<float>(logical_h));
  PixelRatioRef() = SDL_max(ratio, 0.01f);
}

// Fonte a usar para `face`: a de corpo, se carregou; senão a de título (que pode não estar
// pronta, e aí o chamador cai na fonte de depuração).
const Font& Pick(text::Face face) {
  const Font& f = G(face);
  return f.ready ? f : G(text::Face::kTitle);
}

// Índice do codepoint no atlas, ou -1 se não está nos intervalos empacotados.
int GlyphSlot(Uint32 ch) {
  if (ch >= static_cast<Uint32>(kFirstChar) && ch < static_cast<Uint32>(kFirstChar + kNumChars)) {
    return static_cast<int>(ch) - kFirstChar;
  }
  if (ch >= static_cast<Uint32>(kFirstLatin) && ch < static_cast<Uint32>(kFirstLatin + kNumLatin)) {
    return kNumChars + static_cast<int>(ch) - kFirstLatin;
  }
  return -1;
}

// Fallback para codepoints fora do atlas (ou para a fonte de depuração, só ASCII): troca
// letras acentuadas pela base ("Dilúvio" vira "Diluvio"); o resto vira '?'.
char FoldChar(Uint32 ch) {
  if (ch < 0x80) return static_cast<char>(ch);
  if (ch == 0xA1) return '!';
  if (ch == 0xBF) return '?';
  if (ch >= 0xC0 && ch <= 0xC5) return 'A';
  if (ch == 0xC7) return 'C';
  if (ch >= 0xC8 && ch <= 0xCB) return 'E';
  if (ch >= 0xCC && ch <= 0xCF) return 'I';
  if (ch == 0xD1) return 'N';
  if ((ch >= 0xD2 && ch <= 0xD6) || ch == 0xD8) return 'O';
  if (ch >= 0xD9 && ch <= 0xDC) return 'U';
  if (ch >= 0xE0 && ch <= 0xE5) return 'a';
  if (ch == 0xE7) return 'c';
  if (ch >= 0xE8 && ch <= 0xEB) return 'e';
  if (ch >= 0xEC && ch <= 0xEF) return 'i';
  if (ch == 0xF1) return 'n';
  if ((ch >= 0xF2 && ch <= 0xF6) || ch == 0xF8) return 'o';
  if (ch >= 0xF9 && ch <= 0xFC) return 'u';
  return '?';
}

std::string FoldToAscii(const char* str) {
  std::string out;
  Uint32 ch = 0;
  while ((ch = SDL_StepUTF8(&str, nullptr)) != 0) out.push_back(FoldChar(ch));
  return out;
}

// Codepoints a desenhar: os que existem no atlas (e na fonte) ficam; os outros são
// dobrados para ASCII.
std::vector<Uint32> Codepoints(const char* str, const stbtt_fontinfo& info) {
  std::vector<Uint32> out;
  Uint32 ch = 0;
  while ((ch = SDL_StepUTF8(&str, nullptr)) != 0) {
    if (GlyphSlot(ch) >= 0 && stbtt_FindGlyphIndex(&info, static_cast<int>(ch)) != 0) {
      out.push_back(ch);
    } else {
      out.push_back(static_cast<Uint32>(static_cast<unsigned char>(FoldChar(ch))));
    }
  }
  return out;
}

// Índice do tamanho de atlas a usar para `device_px` (px na tela): o menor que não precise
// ampliar; senão o maior (acima dele, Draw usa DrawBig).
int SizeIndex(float device_px) {
  for (int i = 0; i < kNumSizes; ++i) {
    if (kSizes[i] >= device_px) return i;
  }
  return kNumSizes - 1;
}

bool IsBig(float device_px) { return device_px > kSizes[kNumSizes - 1]; }

// Largura em px no tamanho nativo do atlas `s`, somando avanços e kerning.
float NativeWidth(const Font& f, const std::vector<Uint32>& cps, int s) {
  const float scale = stbtt_ScaleForPixelHeight(&f.info, kSizes[s]);
  float w = 0.0f;
  for (size_t i = 0; i < cps.size(); ++i) {
    const int slot = GlyphSlot(cps[i]);
    if (slot < 0) continue;
    w += f.chars[s][slot].xadvance;
    if (i + 1 < cps.size()) {
      w += scale * stbtt_GetCodepointKernAdvance(&f.info, static_cast<int>(cps[i]),
                                                 static_cast<int>(cps[i + 1]));
    }
  }
  return w;
}

}  // namespace

namespace text {

bool Init(SDL_Renderer* r, const std::string& ttf_path, Face face) {
  Font& f = G(face);
  size_t size = 0;
  void* data = SDL_LoadFile(ttf_path.c_str(), &size);
  if (data == nullptr) {
    SDL_Log("Fonte nao carregada (%s): %s; usando a fonte de depuracao", ttf_path.c_str(),
            SDL_GetError());
    return false;
  }
  f.file.assign(static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + size);
  f.em_boost = face == Face::kBody ? kBodyEmBoost : kTitleEmBoost;
  SDL_free(data);
  if (!stbtt_InitFont(&f.info, f.file.data(), 0)) {
    SDL_Log("Fonte invalida: %s", ttf_path.c_str());
    return false;
  }

  // Atlas: os três tamanhos empacotados na mesma textura, com sobreamostragem horizontal
  // para o texto pequeno ficar mais nítido.
  std::vector<unsigned char> bitmap(static_cast<size_t>(kAtlasSize) * kAtlasSize);
  stbtt_pack_context pack{};
  if (!stbtt_PackBegin(&pack, bitmap.data(), kAtlasSize, kAtlasSize, 0, 1, nullptr)) {
    SDL_Log("stbtt_PackBegin falhou");
    return false;
  }
  for (int s = 0; s < kNumSizes; ++s) {
    stbtt_PackSetOversampling(&pack, kOversampleX[s], kOversampleY[s]);
    stbtt_pack_range ranges[2]{};
    ranges[0].font_size = kSizes[s];
    ranges[0].first_unicode_codepoint_in_range = kFirstChar;
    ranges[0].num_chars = kNumChars;
    ranges[0].chardata_for_range = f.chars[s];
    ranges[1].font_size = kSizes[s];
    ranges[1].first_unicode_codepoint_in_range = kFirstLatin;
    ranges[1].num_chars = kNumLatin;
    ranges[1].chardata_for_range = f.chars[s] + kNumChars;
    if (!stbtt_PackFontRanges(&pack, f.file.data(), 0, ranges, 2)) {
      SDL_Log("Atlas de fonte nao coube em %dx%d", kAtlasSize, kAtlasSize);
      stbtt_PackEnd(&pack);
      return false;
    }
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&f.info, &ascent, &descent, &line_gap);
    const float k = stbtt_ScaleForPixelHeight(&f.info, kSizes[s]);
    f.ascent[s] = static_cast<float>(ascent) * k;
    f.descent[s] = static_cast<float>(descent) * k;
  }
  stbtt_PackEnd(&pack);

  // Textura RGBA branca com o alpha do atlas: a cor vem do color mod.
  std::vector<unsigned char> rgba(bitmap.size() * 4);
  for (size_t i = 0; i < bitmap.size(); ++i) {
    rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
    rgba[i * 4 + 3] = bitmap[i];
  }
  f.atlas = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, kAtlasSize,
                              kAtlasSize);
  if (f.atlas == nullptr) {
    SDL_Log("SDL_CreateTexture (fonte): %s", SDL_GetError());
    return false;
  }
  SDL_UpdateTexture(f.atlas, nullptr, rgba.data(), kAtlasSize * 4);
  SDL_SetTextureBlendMode(f.atlas, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(f.atlas, SDL_SCALEMODE_LINEAR);
  f.ready = true;
  return true;
}

void Shutdown() {
  for (BigText& big : BigCache()) {
    if (big.texture != nullptr) SDL_DestroyTexture(big.texture);
  }
  BigCache().clear();
  for (Face face : {Face::kTitle, Face::kBody}) {
    Font& f = G(face);
    if (f.atlas != nullptr) SDL_DestroyTexture(f.atlas);
    f.atlas = nullptr;
    f.ready = false;
  }
}

float Width(const char* str, float scale, Face face) {
  const Font& f = Pick(face);
  if (!f.ready) {
    size_t count = 0;
    while (SDL_StepUTF8(&str, nullptr) != 0) ++count;
    return static_cast<float>(count) * SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * scale;
  }
  const float px = kCellPx * scale * f.em_boost;
  const float device_px = px * PixelRatioRef();
  if (IsBig(device_px)) return ImageWidth(str, px, face);
  const int s = SizeIndex(device_px);
  return NativeWidth(f, Codepoints(str, f.info), s) * (px / kSizes[s]);
}

namespace {

// Texto grande: rasteriza no tamanho exato em pixels de dispositivo (DrawOnImage) numa
// textura própria, guardada no cache; desenha a textura escalada para o retângulo lógico.
void DrawBig(SDL_Renderer* r, const Font& f, Face face, const char* str, float x, float y,
             float scale, SDL_Color color) {
  const float ratio = PixelRatioRef();
  const float px = kCellPx * scale * f.em_boost;
  const int device_px = static_cast<int>(px * ratio + 0.5f);
  std::string key = std::to_string(face == Face::kBody ? 1 : 0) + ':' +
                    std::to_string(device_px) + ':' + str;

  std::deque<BigText>& cache = BigCache();
  BigText* hit = nullptr;
  for (BigText& big : cache) {
    if (big.key == key) {
      hit = &big;
      break;
    }
  }
  if (hit == nullptr) {
    const float k = stbtt_ScaleForPixelHeight(&f.info, static_cast<float>(device_px));
    int ascent = 0, descent = 0, line_gap = 0;
    stbtt_GetFontVMetrics(&f.info, &ascent, &descent, &line_gap);
    constexpr int kPad = 2;  // margem para o filtro linear não cortar a borda
    Image img;
    img.width = static_cast<int>(std::ceil(ImageWidth(str, static_cast<float>(device_px), face))) +
                2 * kPad;
    img.height = static_cast<int>(std::ceil((ascent - descent) * k)) + 2 * kPad;
    if (img.width <= 2 * kPad || img.height <= 2 * kPad) return;
    img.rgba.assign(static_cast<size_t>(img.width) * img.height * 4, 0);
    // Branco com alpha do glifo: a cor vem do color mod, como no atlas.
    for (size_t i = 0; i < img.rgba.size(); i += 4) img.rgba[i] = img.rgba[i + 1] = img.rgba[i + 2] = 255;
    DrawOnImage(img, str, static_cast<float>(kPad), static_cast<float>(kPad),
                static_cast<float>(device_px), SDL_Color{255, 255, 255, 255}, face);
    SDL_Texture* texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                             img.width, img.height);
    if (texture == nullptr) return;
    SDL_UpdateTexture(texture, nullptr, img.rgba.data(), img.width * 4);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
    if (cache.size() >= kBigCacheMax) {
      if (cache.front().texture != nullptr) SDL_DestroyTexture(cache.front().texture);
      cache.pop_front();
    }
    cache.push_back(BigText{key, texture, img.width, img.height, kPad + ascent * k});
    hit = &cache.back();
  }

  // Mesmo alinhamento do caminho do atlas: caixa ascent..descent centrada na célula.
  const float k_logical = stbtt_ScaleForPixelHeight(&f.info, px);
  int ascent = 0, descent = 0, line_gap = 0;
  stbtt_GetFontVMetrics(&f.info, &ascent, &descent, &line_gap);
  const float cell = kCellPx * scale;
  const float font_box = (ascent - descent) * k_logical;
  const float baseline = y + (cell - font_box) / 2.0f + ascent * k_logical;
  const SDL_FRect dst{x - 2.0f / ratio, baseline - hit->ascent / ratio,
                      static_cast<float>(hit->width) / ratio,
                      static_cast<float>(hit->height) / ratio};
  SDL_SetTextureColorMod(hit->texture, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(hit->texture, color.a);
  SDL_RenderTexture(r, hit->texture, nullptr, &dst);
}

}  // namespace

void Draw(SDL_Renderer* r, const char* str, float x, float y, float scale, SDL_Color color,
          Face face) {
  const Font& f = Pick(face);
  if (!f.ready) {
    float prev_x = 1.0f, prev_y = 1.0f;
    SDL_GetRenderScale(r, &prev_x, &prev_y);
    SDL_SetRenderScale(r, scale, scale);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    SDL_RenderDebugText(r, x / scale, y / scale, FoldToAscii(str).c_str());
    SDL_SetRenderScale(r, prev_x, prev_y);
    return;
  }
  UpdatePixelRatio(r);
  const float px = kCellPx * scale * f.em_boost;
  if (IsBig(px * PixelRatioRef())) {
    DrawBig(r, f, face, str, x, y, scale, color);
    return;
  }
  const std::vector<Uint32> cps = Codepoints(str, f.info);

  const int s = SizeIndex(px * PixelRatioRef());
  const float k = px / kSizes[s];  // do tamanho do atlas para o tamanho pedido
  const float kern_scale = stbtt_ScaleForPixelHeight(&f.info, kSizes[s]);
  SDL_SetTextureColorMod(f.atlas, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(f.atlas, color.a);

  // `y` é o topo da célula de 8 px * scale; a caixa ascent..descent da fonte é centralizada
  // nela, então o texto fica alinhado com o layout feito para a fonte de depuração.
  float pen_x = 0.0f;
  const float cell = kCellPx * scale;
  const float font_box = (f.ascent[s] - f.descent[s]) * k;
  const float baseline = y + (cell - font_box) / 2.0f + f.ascent[s] * k;
  for (size_t i = 0; i < cps.size(); ++i) {
    const int slot = GlyphSlot(cps[i]);
    if (slot < 0) continue;
    const stbtt_packedchar& pc = f.chars[s][slot];
    const SDL_FRect src{static_cast<float>(pc.x0), static_cast<float>(pc.y0),
                        static_cast<float>(pc.x1 - pc.x0), static_cast<float>(pc.y1 - pc.y0)};
    const SDL_FRect dst{x + (pen_x + pc.xoff) * k, baseline + pc.yoff * k,
                        (pc.xoff2 - pc.xoff) * k, (pc.yoff2 - pc.yoff) * k};
    SDL_RenderTexture(r, f.atlas, &src, &dst);
    pen_x += pc.xadvance;
    if (i + 1 < cps.size()) {
      pen_x += kern_scale * stbtt_GetCodepointKernAdvance(&f.info, static_cast<int>(cps[i]),
                                                          static_cast<int>(cps[i + 1]));
    }
  }
}

float ImageWidth(const char* str, float px, Face face) {
  const Font& f = Pick(face);
  if (!f.ready) return 0.0f;
  const float k = stbtt_ScaleForPixelHeight(&f.info, px);
  const std::vector<Uint32> cps = Codepoints(str, f.info);
  float w = 0.0f;
  for (size_t i = 0; i < cps.size(); ++i) {
    int advance = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&f.info, static_cast<int>(cps[i]), &advance, &lsb);
    w += advance * k;
    if (i + 1 < cps.size()) {
      w += k * stbtt_GetCodepointKernAdvance(&f.info, static_cast<int>(cps[i]),
                                             static_cast<int>(cps[i + 1]));
    }
  }
  return w;
}

void DrawOnImage(Image& img, const char* str, float x, float y, float px, SDL_Color color,
                 Face face) {
  const Font& f = Pick(face);
  if (!f.ready || !img.Valid()) return;
  const float k = stbtt_ScaleForPixelHeight(&f.info, px);
  int ascent = 0, descent = 0, line_gap = 0;
  stbtt_GetFontVMetrics(&f.info, &ascent, &descent, &line_gap);
  const float baseline = y + ascent * k;
  const std::vector<Uint32> cps = Codepoints(str, f.info);
  std::vector<unsigned char> glyph;
  float pen = x;
  for (size_t i = 0; i < cps.size(); ++i) {
    const int cp = static_cast<int>(cps[i]);
    int advance = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(&f.info, cp, &advance, &lsb);
    // Subpixel em x: a posição fracionária da pena entra no raster do glifo.
    const float pen_floor = std::floor(pen);
    const float shift_x = pen - pen_floor;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetCodepointBitmapBoxSubpixel(&f.info, cp, k, k, shift_x, 0.0f, &x0, &y0, &x1, &y1);
    const int gw = x1 - x0, gh = y1 - y0;
    if (gw > 0 && gh > 0) {
      glyph.assign(static_cast<size_t>(gw) * gh, 0);
      stbtt_MakeCodepointBitmapSubpixel(&f.info, glyph.data(), gw, gh, gw, k, k, shift_x, 0.0f,
                                        cp);
      const int ox = static_cast<int>(pen_floor) + x0;
      const int oy = static_cast<int>(std::floor(baseline)) + y0;
      for (int gy = 0; gy < gh; ++gy) {
        const int py = oy + gy;
        if (py < 0 || py >= img.height) continue;
        for (int gx = 0; gx < gw; ++gx) {
          const int pxl = ox + gx;
          if (pxl < 0 || pxl >= img.width) continue;
          const float a = glyph[static_cast<size_t>(gy) * gw + gx] / 255.0f * (color.a / 255.0f);
          if (a <= 0.0f) continue;
          uint8_t* d = &img.rgba[(static_cast<size_t>(py) * img.width + pxl) * 4];
          d[0] = static_cast<uint8_t>(d[0] + (color.r - d[0]) * a);
          d[1] = static_cast<uint8_t>(d[1] + (color.g - d[1]) * a);
          d[2] = static_cast<uint8_t>(d[2] + (color.b - d[2]) * a);
          d[3] = static_cast<uint8_t>(SDL_max(d[3], static_cast<int>(a * 255.0f)));
        }
      }
    }
    pen += advance * k;
    if (i + 1 < cps.size()) {
      pen += k * stbtt_GetCodepointKernAdvance(&f.info, cp, static_cast<int>(cps[i + 1]));
    }
  }
}

void DrawCentered(SDL_Renderer* r, const char* str, float center_x, float center_y, float scale,
                  SDL_Color color, Face face) {
  const float w = Width(str, scale, face);
  const float h = kCellPx * scale;
  Draw(r, str, center_x - w / 2.0f, center_y - h / 2.0f, scale, color, face);
}

}  // namespace text
