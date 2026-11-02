#include "scenes/battle/BattleScene.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

#include "scripts/cardface/CardFace.h"
#include "scripts/colors/Colors.h"
#include "scripts/config/Config.h"
#include "scripts/frame/Frame.h"
#include "scripts/i18n/I18n.h"
#include "scripts/image/Image.h"
#include "scripts/music/Music.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

// A arena é um prisma trapezoidal: topo 1,9 x 1,9 (tabuleiro), base 2,5 x 2,5, altura 0,36 unidades no
// modelo (um ladrilho); escalada para caber na câmera.
constexpr float kArenaScale = 4.2f;
// Alturas da arena no mundo (bounds do glb y[-0.061, 0.3] vezes a escala).
constexpr float kArenaBaseY = -0.061f * kArenaScale;
constexpr float kArenaTopY = 0.3f * kArenaScale;

// Camadas de névoa (altura acima da base da arena e opacidade), de assets/Fog.json:
// battle.mist.layers (base, sempre) e battle.introMist.layers (manto da abertura).
std::vector<BattleScene::MistLayer> LoadMistLayers(const char* path) {
  std::vector<BattleScene::MistLayer> layers;
  const json::Value* list = config::Node("fog", path);
  if (list != nullptr && list->IsArray()) {
    for (const json::Value& item : list->array) {
      if (!item.IsObject()) continue;
      layers.push_back({kArenaBaseY + config::Float(item, "y", 0.0f),
                        config::Float(item, "alpha", 0.5f)});
    }
  }
  return layers;
}

// Tochas fincadas no chão de cada lado da arena; são a única luz da arena.
constexpr float kTorchX = 6.4f;
constexpr float kTorchZ = -1.0f;
constexpr float kTorchScale = 2.6f;
constexpr float kTorchTilt = 0.10f;   // rad, levemente inclinadas para fora

// Abertura da batalha: a arena sobe de dentro de uma névoa densa enquanto a câmera se
// aproxima. A névoa extra fica em alturas fixas do mundo, por isso a arena "fura" camada por
// camada ao subir, e vai rareando até restarem só as camadas da base.
constexpr float kIntroSeconds = 1.6f;
constexpr float kIntroDrop = 3.2f;                     // quanto a arena começa abaixo do lugar
// Tochas emergem depois da arena: começam a subir quando ela já está quase no lugar.
constexpr float kTorchIntroStart = 1.1f;   // s depois da entrada
constexpr float kTorchIntroSeconds = 1.2f;
constexpr float kTorchIntroDrop = 4.5f;    // começam bem abaixo do chão
constexpr float kIgniteSeconds = 0.6f;     // o fogo acende depois de a tocha ter surgido
constexpr float kMatchStart = 3.2f;        // s: sorteio e primeiro turno, com as tochas acesas
constexpr math::Vec3 kIntroEyeStart{0.0f, 16.0f, 22.0f};  // câmera longe e alta
constexpr math::Vec3 kArenaTarget{0.0f, -0.1f, -0.3f};

// Virada de campo (docs/detalhes_tecnicos.md): a câmera orbita 180° em torno do centro da
// arena no turno do computador e volta no seu.
constexpr float kOrbitSeconds = 1.0f;

// Layout 2D na grade de 16 px (ui::Grid). Botão "Passar turno": centro superior, logo abaixo
// da dica (kHintY), entre os painéis de campo e de vida e acima da arena.
constexpr float kButtonWidth = ui::Grid(21);   // 336 px
constexpr float kButtonHeight = ui::Grid(5);   // 80 px
constexpr float kButtonMargin = ui::Grid(1);   // botão de som no canto inferior esquerdo
constexpr float kEndTurnTop = ui::Grid(6);     // 96 px: dica centrada em 64 px, 24 px de linha
constexpr float kHintScale = 2.0f * ui::kScale;
// Botão de som: 3 x 3 células no canto inferior esquerdo; o painel do monte fica à direita.
constexpr float kSoundButtonSize = ui::Grid(3);
constexpr float kSoundButtonGap = ui::Grid(1);
// Tooltip da carta no campo: imagem da carta à esquerda, textos à direita.
constexpr float kTipScale = 2.0f * ui::kScale;        // 24 px
constexpr float kTipDetailScale = 1.5f * ui::kScale;  // 18 px
constexpr float kTipPadding = ui::Grid(1);
constexpr float kTipGap = ui::Grid(0.5f);
constexpr float kTipOffset = ui::Grid(1);     // distância do cursor
constexpr float kTipImageH = ui::Grid(14);    // altura da frente da carta (224 px)
constexpr float kTipTextW = ui::Grid(24);     // largura da coluna de texto (quebra da descrição)
// Topo: linha de situação e dica (centros em y).
constexpr float kStatusY = ui::Grid(2);
constexpr float kHintY = ui::Grid(4);
constexpr float kCardAspect = 1600.0f / 2230.0f;

// Quebra `str` em linhas de até `max_w` px na escala dada (por palavras).
std::vector<std::string> WrapText(const std::string& str, float max_w, float scale) {
  std::vector<std::string> lines;
  std::string line;
  size_t pos = 0;
  while (pos < str.size()) {
    size_t end = str.find(' ', pos);
    if (end == std::string::npos) end = str.size();
    const std::string word = str.substr(pos, end - pos);
    const std::string candidate = line.empty() ? word : line + " " + word;
    if (!line.empty() && text::Width(candidate.c_str(), scale) > max_w) {
      lines.push_back(line);
      line = word;
    } else {
      line = candidate;
    }
    pos = end + 1;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}
constexpr float kStatusScale = 2.0f * ui::kScale;
constexpr float kClickSlop = 6.0f;      // px lógicos: até aqui um arrasto conta como clique
constexpr float kAttackDragPx = 24.0f;  // px lógicos de arrasto para virar ataque
constexpr int kHandLimit = 6;
// Pausas do turno do computador (s), para o jogador acompanhar cada jogada.
constexpr float kEnemyPause = 0.7f;
// Revelação da carta jogada pelo oponente: tempo total no centro da tela (com entrada e saída
// em fade), altura da carta (perto dos 557 px nativos da frente do HUD) e escala de entrada.
constexpr float kRevealSeconds = 2.0f;
constexpr float kRevealTravel = 0.5f;    // voo da mão do oponente até o centro, virando
constexpr float kHandHideSeconds = 0.5f; // mão do oponente se recolhe antes da virada
constexpr float kRevealFadeIn = 0.2f;
constexpr float kRevealFadeOut = 0.25f;
constexpr float kRevealCardH = ui::Grid(35);  // 560 px
constexpr float kRevealCaptionScale = 3.0f * ui::kScale;
constexpr float kRevealDetailScale = 2.5f * ui::kScale;
constexpr float kAttackHit = 0.25f;  // momento do impacto dentro da investida
// Tremor da arena ao abater uma carta: duração e deslocamento máximo da câmera (unidades do
// mundo), que cai com o quadrado do tempo restante.
constexpr float kShakeSeconds = 0.45f;
constexpr float kShakeAmplitude = 0.16f;

// Normal map da carta pronto para a GPU: pixels transparentes (fora dos cantos arredondados)
// viram normal plana (128, 128, 255), na proporção da transparência, para o filtro linear
// não puxar a borda da carta para uma normal inválida; depois reduz 2x, para a mesma
// resolução da frente composta (cardface::kWidth x kHeight). Reduções e normalização no
// shader mantêm o relevo.
Image CardNormalMap(Image image, int downscale) {
  if (!image.Valid()) return image;
  constexpr int kFlat[3] = {128, 128, 255};
  for (size_t i = 0; i + 3 < image.rgba.size(); i += 4) {
    const int a = image.rgba[i + 3];
    if (a == 255) continue;
    for (int c = 0; c < 3; ++c) {
      image.rgba[i + c] = static_cast<uint8_t>((image.rgba[i + c] * a + kFlat[c] * (255 - a)) / 255);
    }
    image.rgba[i + 3] = 255;
  }
  return downscale > 1 ? image.Downscaled(downscale) : image;
}

// Aura 2D (revelação de carta foil): anel em volta de `inner`, da cor de cada canto (sentido
// horário a partir do superior esquerdo) para transparente a `margin` px. Quatro trapézios
// com cores por vértice: a cor gira em volta da carta conforme os cantos mudam.
void DrawGlowRing(SDL_Renderer* r, const SDL_FRect& inner, float margin, const SDL_FColor c[4]) {
  const SDL_FPoint in[4] = {{inner.x, inner.y},
                            {inner.x + inner.w, inner.y},
                            {inner.x + inner.w, inner.y + inner.h},
                            {inner.x, inner.y + inner.h}};
  const SDL_FPoint out[4] = {{inner.x - margin, inner.y - margin},
                             {inner.x + inner.w + margin, inner.y - margin},
                             {inner.x + inner.w + margin, inner.y + inner.h + margin},
                             {inner.x - margin, inner.y + inner.h + margin}};
  SDL_Vertex v[16];
  int indices[24];
  for (int side = 0; side < 4; ++side) {
    const int a = side, b = (side + 1) % 4;
    SDL_FColor ca = c[a], cb = c[b];
    SDL_FColor oa = ca, ob = cb;
    oa.a = ob.a = 0.0f;
    v[side * 4 + 0] = SDL_Vertex{in[a], ca, {0, 0}};
    v[side * 4 + 1] = SDL_Vertex{in[b], cb, {0, 0}};
    v[side * 4 + 2] = SDL_Vertex{out[b], ob, {0, 0}};
    v[side * 4 + 3] = SDL_Vertex{out[a], oa, {0, 0}};
    const int base = side * 4;
    const int quad[6] = {base, base + 1, base + 2, base, base + 2, base + 3};
    for (int k = 0; k < 6; ++k) indices[side * 6 + k] = quad[k];
  }
  SDL_RenderGeometry(r, nullptr, v, 16, indices, 24);
}

SDL_FColor Hue(float x, float alpha) {
  constexpr float kTau = 6.2831853f;
  return SDL_FColor{0.5f + 0.5f * std::cos(kTau * x), 0.5f + 0.5f * std::cos(kTau * (x + 0.33f)),
                    0.5f + 0.5f * std::cos(kTau * (x + 0.67f)), alpha};
}

// Faixa grossa ao longo de uma polilinha (SDL_RenderGeometry): largura e cor por ponto.
void DrawStrip(SDL_Renderer* r, const std::vector<SDL_FPoint>& pts,
               const std::vector<float>& widths, const std::vector<SDL_FColor>& colors_) {
  const size_t n = pts.size();
  if (n < 2) return;
  std::vector<SDL_Vertex> v;
  v.reserve(n * 2);
  for (size_t i = 0; i < n; ++i) {
    const SDL_FPoint a = pts[i == 0 ? 0 : i - 1];
    const SDL_FPoint b = pts[i + 1 < n ? i + 1 : n - 1];
    float tx = b.x - a.x, ty = b.y - a.y;
    const float len = SDL_sqrtf(tx * tx + ty * ty);
    if (len > 1e-4f) {
      tx /= len;
      ty /= len;
    }
    const float half = widths[i] * 0.5f;
    const SDL_FPoint p = pts[i];
    v.push_back(SDL_Vertex{{p.x - ty * half, p.y + tx * half}, colors_[i], {0, 0}});
    v.push_back(SDL_Vertex{{p.x + ty * half, p.y - tx * half}, colors_[i], {0, 0}});
  }
  std::vector<int> idx;
  for (int i = 0; i + 1 < static_cast<int>(n); ++i) {
    const int a = i * 2;
    idx.insert(idx.end(), {a, a + 1, a + 2, a + 1, a + 3, a + 2});
  }
  SDL_RenderGeometry(r, nullptr, v.data(), static_cast<int>(v.size()), idx.data(),
                     static_cast<int>(idx.size()));
}

// Linha de ataque: arco (Bézier quadrática) da carta atacante até o mouse, curvado para cima
// com altura proporcional à distância; engrossa até a ponta, com contorno escuro, brilho em
// volta, um pulso de luz que corre da carta para o alvo e ponta de seta na direção da curva.
void DrawAttackArc(SDL_Renderer* r, SDL_FPoint from, SDL_FPoint to, SDL_Color color, float time) {
  const float dx = to.x - from.x, dy = to.y - from.y;
  const float dist = SDL_sqrtf(dx * dx + dy * dy);
  if (dist < 8.0f) return;
  // Normal do segmento voltada para cima na tela (y menor); linha vertical curva para a direita.
  float nx = -dy / dist, ny = dx / dist;
  if (ny > 0.0f || (ny == 0.0f && nx < 0.0f)) {
    nx = -nx;
    ny = -ny;
  }
  const float bend = SDL_clamp(dist * 0.35f, 40.0f, 320.0f);
  const SDL_FPoint ctrl{(from.x + to.x) * 0.5f + nx * bend, (from.y + to.y) * 0.5f + ny * bend};
  constexpr int kSegments = 48;
  constexpr float kHeadLength = 34.0f;
  constexpr float kHeadWidth = 34.0f;
  // Pontos da curva; a faixa para na base da seta (recuo de kHeadLength ao longo da ponta).
  auto at = [&](float t) {
    const float u = 1.0f - t;
    return SDL_FPoint{u * u * from.x + 2 * u * t * ctrl.x + t * t * to.x,
                      u * u * from.y + 2 * u * t * ctrl.y + t * t * to.y};
  };
  // Tangente no fim (direção de ctrl para to): orienta a seta.
  float ex = to.x - ctrl.x, ey = to.y - ctrl.y;
  const float elen = SDL_sqrtf(ex * ex + ey * ey);
  ex /= SDL_max(elen, 1e-4f);
  ey /= SDL_max(elen, 1e-4f);
  const SDL_FPoint base{to.x - ex * kHeadLength, to.y - ey * kHeadLength};
  std::vector<SDL_FPoint> pts;
  std::vector<float> core_w, glow_w, outline_w;
  std::vector<SDL_FColor> core_c, glow_c, outline_c;
  const float head_t = SDL_max(0.0f, 1.0f - kHeadLength / SDL_max(dist * 1.15f, 1.0f));
  const SDL_FColor c{color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, 1.0f};
  for (int i = 0; i <= kSegments; ++i) {
    const float s = static_cast<float>(i) / kSegments;
    pts.push_back(i == kSegments ? base : at(s * head_t));
    const float w = 4.0f + 6.0f * s;  // engrossa até a ponta
    // Pulso: faixa clara que corre ao longo do arco (ciclo de 0,8 s).
    const float phase = s - SDL_fmodf(time * 1.25f, 1.0f);
    const float pulse = SDL_expf(-phase * phase * 120.0f);
    core_w.push_back(w);
    glow_w.push_back(w * 3.2f);
    outline_w.push_back(w + 4.0f);
    core_c.push_back(SDL_FColor{SDL_min(1.0f, c.r + 0.5f * pulse), SDL_min(1.0f, c.g + 0.5f * pulse),
                                SDL_min(1.0f, c.b + 0.5f * pulse), 0.55f + 0.45f * s});
    glow_c.push_back(SDL_FColor{c.r, c.g, c.b, (0.12f + 0.25f * pulse) * (0.5f + 0.5f * s)});
    outline_c.push_back(SDL_FColor{0.0f, 0.0f, 0.0f, 0.45f * (0.5f + 0.5f * s)});
  }
  DrawStrip(r, pts, glow_w, glow_c);
  DrawStrip(r, pts, outline_w, outline_c);
  DrawStrip(r, pts, core_w, core_c);
  // Ponta de seta: contorno escuro um pouco maior e a seta na cor.
  auto head = [&](float grow, SDL_FColor fill) {
    const float px = -ey, py = ex;  // perpendicular à ponta
    const float half = kHeadWidth * 0.5f + grow;
    const SDL_FPoint tip{to.x + ex * grow, to.y + ey * grow};
    const SDL_FPoint b0{base.x - ex * grow + px * half, base.y - ey * grow + py * half};
    const SDL_FPoint b1{base.x - ex * grow - px * half, base.y - ey * grow - py * half};
    const SDL_Vertex tri[3] = {{tip, fill, {0, 0}}, {b0, fill, {0, 0}}, {b1, fill, {0, 0}}};
    SDL_RenderGeometry(r, nullptr, tri, 3, nullptr, 0);
  };
  head(3.0f, SDL_FColor{0.0f, 0.0f, 0.0f, 0.5f});
  head(0.0f, SDL_FColor{c.r, c.g, c.b, 1.0f});
}

float SmoothStep(float t) {
  t = math::Clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

const char* SlotName(int slot, char* buffer) {
  buffer[0] = slot < Board::kCols ? 'A' : 'B';
  buffer[1] = static_cast<char>('1' + slot % Board::kCols);
  buffer[2] = '\0';
  return buffer;
}

}  // namespace

BattleScene::~BattleScene() {
  if (field_front_2d_ != nullptr) SDL_DestroyTexture(field_front_2d_);
  for (auto& [code, texture] : hud_faces_) SDL_DestroyTexture(texture);
  for (auto& [code, texture] : hud_arts_) SDL_DestroyTexture(texture);
  if (gpu_ == nullptr) return;
  hand_.Clear();
  enemy_hand_view_.Clear();
  board_.Destroy(*gpu_);
  enemy_board_.Destroy(*gpu_);
  sword_.Destroy(*gpu_);
  shield_.Destroy(*gpu_);
  card_aura_.Destroy(*gpu_);
  gpu_->DestroyMesh(veil_quad_);
  card_model_.reset();
  arena_model_.reset();
  torch_.reset();
  gpu_->DestroyMesh(mist_quad_);
  for (SDL_GPUTexture* t : textures_) gpu_->DestroyTexture(t);
  textures_.clear();
}

void BattleScene::Preload(const PreloadInfo& info) {
  preloaded_ = true;
  const std::string base = BasePath();
  // Frente da carta de campo para o HUD: aparece com ~120 px de altura, então vai reduzida 4x
  // (400x557); vira SDL_Texture em OnEnter, na thread principal.
  hud_field_ = Image::LoadFile(base + "sprites/CardFrontField.png").Downscaled(4);
  if (info.gpu_device == nullptr) return;

  gpu_ = std::make_unique<Gpu>();
  if (!gpu_->InitDevice(info.gpu_device, info.logical_width, info.logical_height,
                        base + "shaders")) {
    gpu_.reset();
    return;
  }
  card_model_ = std::make_unique<Model>();
  if (!card_model_->Upload(*gpu_, Model::Parse(base + "models/Card.glb"))) {
    SDL_Log("Falha ao carregar models/Card.glb");
    card_model_.reset();
    return;
  }
  card_aura_.Init(*gpu_, *card_model_);
  veil_quad_ = gpu_->CreateMesh({{-0.5f, -0.5f, 0, 0, 1}, {0.5f, -0.5f, 0, 1, 1},
                                 {0.5f, 0.5f, 0, 1, 0}, {-0.5f, 0.5f, 0, 0, 0}},
                                {0, 1, 2, 0, 2, 3});
  back_ = UploadSprite(Image::LoadFile(base + "sprites/CardBack.png"));
  // Frente: 1600x2230 com cantos transparentes, reduzido para 800x1115 (CardNormalMap).
  front_normal_ = UploadSprite(
      CardNormalMap(Image::LoadFile(base + "sprites/CardFrontNormalMap.png"), 2));
  back_normal_ = UploadSprite(Image::LoadFile(base + "sprites/CardBackNormalMap.png"));
  for (Board* board : {&board_, &enemy_board_}) {
    board->GetLayout().arena_scale = kArenaScale;
    board->SetModel(card_model_.get());
    board->SetAura(&card_aura_);
    board->SetBack(back_);
    board->SetNormalMaps(front_normal_, back_normal_);
    board->Init(*gpu_);
  }
  enemy_board_.SetSide(-1.0f);
  sword_.Init(*gpu_);
  shield_.Init(*gpu_);
  // Molduras reduzidas 2x (800x1115): a carta na mão é pequena e a frente de cada carta é
  // composta a partir delas com os textos (cardface).
  frames_[0] = Image::LoadFile(base + "sprites/CardFrontGood.png").Downscaled(2);
  frames_[1] = Image::LoadFile(base + "sprites/CardFrontEvil.png").Downscaled(2);
  frames_[2] = Image::LoadFile(base + "sprites/CardFrontNeutral.png").Downscaled(2);
  frames_[3] = Image::LoadFile(base + "sprites/CardFrontField.png").Downscaled(2);
  // Frente oculta da mão do oponente: uma só textura para todas as cartas dele.
  hidden_face_ =
      UploadSprite(cardface::ComposeHidden(Image::LoadFile(base + "sprites/CardFrontEnemy.png").Downscaled(2)));

  // Montes: 40 cartas sorteadas por peso e embaralhadas (deck::Deck) para cada lado, ANTES de
  // qualquer carta aparecer; as 5 da sua mão inicial são compradas e compostas já aqui. O
  // computador também compra 5 (só os dados; a frente é composta quando a carta entra).
  cards_ = cards::LoadAll(base);
  // Ilustrações (imageUrl de cada carta), ajustadas uma vez à janela da imagem. Cartas sem o
  // arquivo ficam com a janela em branco. Decodificar ~65 PNGs grandes é a parte mais lenta
  // da carga: roda em várias threads (stb_image e FitArt não têm estado compartilhado).
  arts_.clear();
  {
    const Uint64 started = SDL_GetTicksNS();
    std::vector<const CardInfo*> todo;
    for (const CardInfo& card : cards_) {
      if (card.image_url.empty()) continue;
      if (SDL_GetPathInfo((base + card.image_url).c_str(), nullptr)) todo.push_back(&card);
    }
    std::vector<Image> results(todo.size());
    std::atomic<size_t> next{0};
    const unsigned workers =
        SDL_clamp(std::thread::hardware_concurrency(), 1u, 8u);
    std::vector<std::thread> pool;
    for (unsigned k = 0; k < workers; ++k) {
      pool.emplace_back([&] {
        for (size_t i = next++; i < todo.size(); i = next++) {
          results[i] = cardface::FitArt(Image::LoadFile(base + todo[i]->image_url));
        }
      });
    }
    for (std::thread& t : pool) t.join();
    for (size_t i = 0; i < todo.size(); ++i) {
      if (results[i].Valid()) arts_[todo[i]->code] = std::move(results[i]);
    }
    SDL_Log("Ilustracoes de cartas: %zu (%.0f ms, %u threads)", arts_.size(),
            static_cast<double>(SDL_GetTicksNS() - started) / 1e6, workers);
  }
  // Depuração: PONTIFICE_SEED=n fixa o sorteio (montes, compras e quem começa) para repetir
  // uma partida igual.
  if (const char* env = SDL_getenv("PONTIFICE_SEED")) {
    rng_.seed(static_cast<unsigned>(SDL_atoi(env)));
  } else {
    rng_.seed(std::random_device{}());
  }
  deck_ = deck::Deck(cards::BuildDeck(cards_, Hud::kDeckSize, rng_));
  deck_.Shuffle();
  // Monte do computador conforme o nível (menu; PONTIFICE_DIFFICULTY=easy|medium|hard|impossible
  // força, para depuração).
  int level = info.difficulty;
  if (const char* env = SDL_getenv("PONTIFICE_DIFFICULTY")) {
    if (std::strcmp(env, "easy") == 0) level = 0;
    if (std::strcmp(env, "medium") == 0) level = 1;
    if (std::strcmp(env, "hard") == 0) level = 2;
    if (std::strcmp(env, "impossible") == 0) level = 3;
  }
  difficulty_ = static_cast<ai::Difficulty>(std::clamp(level, 0, 3));
  enemy_deck_ = deck::Deck(cards::BuildDeck(cards_, Hud::kDeckSize, rng_,
                                            [this](const CardInfo& card) {
                                              return ai::DeckWeight(card, difficulty_);
                                            }));
  enemy_deck_.Shuffle();
  SDL_Log("Nivel do oponente: %s", ai::DifficultyKey(difficulty_));
  // Depuração: PONTIFICE_HAND=G101,E101,... força os códigos da mão inicial. O monte continua
  // embaralhado e perde uma carta do topo por carta forçada, para o contador do HUD ficar
  // como numa partida normal (35/40).
  std::vector<std::string> forced;
  if (const char* env = SDL_getenv("PONTIFICE_HAND")) {
    std::string spec = env;
    size_t pos = 0;
    while (pos <= spec.size()) {
      size_t end = spec.find(',', pos);
      if (end == std::string::npos) end = spec.size();
      if (end > pos) forced.push_back(spec.substr(pos, end - pos));
      pos = end + 1;
    }
  }
  for (int i = 0; i < 5; ++i) {
    std::optional<std::string> code;
    if (!forced.empty()) {
      if (i < static_cast<int>(forced.size())) {
        code = forced[i];
        deck_.Draw();
      }
    } else {
      code = deck_.Draw();
    }
    if (!code) break;
    const Face face = MakeCardFace(*code);
    if (face.texture != nullptr) initial_hand_.push_back(face);
  }
  // Depuração: PONTIFICE_ENEMY_HAND=G101,E101,... força a mão inicial do computador (como
  // PONTIFICE_HAND; o monte dele perde uma carta do topo por carta forçada).
  if (const char* env = SDL_getenv("PONTIFICE_ENEMY_HAND")) {
    std::string spec = env;
    size_t pos = 0;
    while (pos < spec.size() && enemy_hand_.size() < 5) {
      size_t end = spec.find(',', pos);
      if (end == std::string::npos) end = spec.size();
      if (const CardInfo* info = cards::Find(cards_, spec.substr(pos, end - pos))) {
        enemy_hand_.push_back(info);
        enemy_deck_.Draw();
      }
      pos = end + 1;
    }
  }
  while (enemy_hand_.size() < 5 && enemy_deck_.Size() > 0) EnemyDraw();
  arena_model_ = std::make_unique<Model>();
  if (!arena_model_->Upload(*gpu_, Model::Parse(base + "models/Arena.glb"))) {
    SDL_Log("Arena.glb nao carregado; gere com tools/texture_arena.py");
    arena_model_.reset();
  }
  torch_ = std::make_unique<Torch>();
  if (!torch_->Upload(*gpu_, Model::Parse(base + "models/Torch.glb"))) {
    SDL_Log("Torch.glb nao carregado; gere com tools/make_torch.py");
    torch_.reset();
  }
  // Quad horizontal em y = 0; cada camada de névoa o desenha transladado para sua altura.
  const float h = config::Float("fog", "battle.mist.halfSize", 40.0f);
  mist_quad_ = gpu_->CreateMesh(
      {{-h, 0, -h, 0, 0}, {h, 0, -h, 1, 0}, {h, 0, h, 1, 1}, {-h, 0, h, 0, 1}},
      {0, 1, 2, 0, 2, 3});
  mist_layers_ = LoadMistLayers("battle.mist.layers");
  intro_mist_layers_ = LoadMistLayers("battle.introMist.layers");
  if (mist_layers_.empty()) mist_layers_ = {{kArenaBaseY + 0.05f, 0.8f}, {kArenaBaseY + 0.45f, 0.5f}};
}

SDL_GPUTexture* BattleScene::UploadSprite(const Image& image) {
  SDL_GPUTexture* texture = gpu_->CreateTexture(image);
  if (texture != nullptr) textures_.push_back(texture);
  return texture;
}

SDL_Texture* BattleScene::UploadHudSprite(SDL_Renderer* renderer, const Image& image) {
  if (!image.Valid()) return nullptr;
  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STATIC, image.width, image.height);
  if (texture == nullptr) return nullptr;
  SDL_UpdateTexture(texture, nullptr, image.rgba.data(), image.width * 4);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
  return texture;
}

void BattleScene::ToggleSampleFieldCard() {
  if (hud_.HasFieldCard()) {
    hud_.ClearFieldCard();
    field_ = rules::Field{};
    return;
  }
  // Primeira carta de assets/CardsField.json.
  for (const CardInfo& card : cards_) {
    if (card.IsField()) {
      PlayField(Side::kPlayer, card);
      return;
    }
  }
}

BattleScene::Face BattleScene::MakeCardFace(const std::string& code) {
  const CardInfo* card = cards::Find(cards_, code);
  if (card == nullptr || gpu_ == nullptr) return {};
  const Image& frame = FrameFor(*card);
  // Hierarquia 1 (Divindades, Soberanos): carta foil.
  return {UploadSprite(cardface::Compose(frame, *card, ArtFor(*card))), card->hierarchy == 1,
          card};
}

SDL_Texture* BattleScene::HudArt(const CardInfo& card, bool large) {
  if (renderer_ == nullptr) return nullptr;
  const std::string key = large ? card.code + "#large" : card.code;
  if (const auto it = hud_arts_.find(key); it != hud_arts_.end()) return it->second;
  const Image* art = ArtFor(card);
  if (art == nullptr) return nullptr;
  // Miniatura: 668x448 reduzida 4x (167x112), no painel tem 64 px de altura. Ampliação:
  // resolução cheia, mostrada no tamanho nativo.
  SDL_Texture* texture = UploadHudSprite(renderer_, large ? *art : art->Downscaled(4));
  if (texture != nullptr) hud_arts_[key] = texture;
  return texture;
}

const Image* BattleScene::ArtFor(const CardInfo& card) const {
  const auto it = arts_.find(card.code);
  return it != arts_.end() ? &it->second : nullptr;
}

// Moldura da categoria (CardInfo::FrameSprite): Good, Evil, Neutral ou Field. Sem a neutra,
// neutras usam a de campo.
const Image& BattleScene::FrameFor(const CardInfo& card) const {
  switch (card.category) {
    case 'G':
      return frames_[0];
    case 'E':
      return frames_[1];
    case 'N':
      return frames_[2].Valid() ? frames_[2] : frames_[3];
    default:
      return frames_[3];
  }
}

SDL_Texture* BattleScene::HudFace(const CardInfo& card) {
  if (renderer_ == nullptr) return field_front_2d_;
  auto it = hud_faces_.find(card.code);
  if (it != hud_faces_.end()) return it->second;
  // Composta na moldura da categoria reduzida 2x e reduzida mais 2x (400x557): o HUD e o
  // tooltip mostram de 120 a 300 px de altura.
  const Image& frame = FrameFor(card);
  if (!frame.Valid()) return field_front_2d_;
  SDL_Texture* texture =
      UploadHudSprite(renderer_, cardface::Compose(frame, card, ArtFor(card)).Downscaled(2));
  if (texture == nullptr) return field_front_2d_;
  hud_faces_[card.code] = texture;
  return texture;
}

void BattleScene::DrawCard() {
  const std::optional<std::string> code = deck_.Draw();
  if (!code) return;
  hud_.SetDeck(Hud::kDeckSize - static_cast<int>(deck_.Size()), static_cast<int>(deck_.Size()));
  const Face face = MakeCardFace(*code);
  if (face.texture != nullptr) hand_.Add(face.texture, face.foil, face.info);
}

void BattleScene::EnemyHandAdd(const CardInfo* info) {
  enemy_hand_.push_back(info);
  if (hidden_face_ != nullptr) enemy_hand_view_.Add(hidden_face_, false, nullptr);
}

void BattleScene::EnemyHandRemove(size_t index, math::Mat4* world) {
  if (index >= enemy_hand_.size()) return;
  enemy_hand_.erase(enemy_hand_.begin() + static_cast<long>(index));
  if (index < enemy_hand_view_.Size()) {
    const Hand::Card removed = enemy_hand_view_.Remove(index);
    if (world != nullptr) *world = removed.world;
  }
}

void BattleScene::EnemyDraw() {
  const std::optional<std::string> code = enemy_deck_.Draw();
  if (!code) return;
  if (const CardInfo* info = cards::Find(cards_, *code)) EnemyHandAdd(info);
}

// ---------------------------------------------------------------------------------------------
// Fluxo da partida

void BattleScene::Schedule(float delay, std::function<void()> fn) {
  steps_.emplace_back(delay, std::move(fn));
}

void BattleScene::ScheduleNext(float delay, std::function<void()> fn) {
  steps_.emplace_front(delay, std::move(fn));
}

float BattleScene::RevealDuration() const {
  return (reveal_.from_hand ? kRevealTravel : 0.0f) + kRevealSeconds;
}

void BattleScene::StartReveal(const CardInfo& card, std::string caption, std::string detail,
                              const math::Mat4* from_world) {
  reveal_ = Reveal{};
  reveal_.card = &card;
  reveal_.caption = std::move(caption);
  reveal_.detail = std::move(detail);
  // Pose de saída na mão: translação, escala (coluna X) e giro em Z do leque.
  if (from_world != nullptr && ready_ && hidden_face_ != nullptr) {
    const math::Mat4& m = *from_world;
    reveal_.from_hand = true;
    reveal_.from_pos = {m.m[3][0], m.m[3][1], m.m[3][2]};
    reveal_.from_scale = std::sqrt(m.m[0][0] * m.m[0][0] + m.m[0][1] * m.m[0][1] +
                                   m.m[0][2] * m.m[0][2]);
    reveal_.from_roll = std::atan2(m.m[0][1], m.m[0][0]);
  }
  if (ready_) {
    const Face face = MakeCardFace(card.code);
    reveal_.front3d = face.texture;
    reveal_.foil = face.foil;
  }
  if (reveal_.front3d == nullptr) reveal_.face = HudFace(card);  // sem 3D: imagem 2D
}

bool BattleScene::Busy() const {
  return !steps_.empty() || board_.Animating() || enemy_board_.Animating() || sword_.Active() ||
         shield_.Active() ||
         std::fabs(orbit_ - orbit_target_) > 0.001f;
}

bool BattleScene::AttacksAllowed(Side side) const {
  return !(starter_ == side && turns_played_[Index(side)] == 0);
}

void BattleScene::StartMatch() {
  started_ = true;
  // Quem começa já foi sorteado na entrada (OnEnter), para a sua mão nem aparecer quando é o
  // oponente: aqui o turno dele começa direto.
  SDL_Log("Sorteio: %s", starter_ == Side::kPlayer ? "voce comeca" : "o oponente comeca");
  if (starter_ == Side::kPlayer) {
    BeginPlayerTurn();
  } else {
    hand_.SetAway(true);
    enemy_hand_view_.SetAway(false);  // a mão dele aparece no turno dele
    orbit_target_ = 1.0f;
    Schedule(kOrbitSeconds + 0.2f, [this] { RunEnemyTurn(); });
  }
}

void BattleScene::BeginPlayerTurn() {
  if (hud_.Over()) return;
  turn_side_ = Side::kPlayer;
  player_turn_ = true;
  summoned_ = false;
  attack_from_ = -1;
  attack_drag_ = false;
  board_.ResetAttacks();
  hand_.SetAway(false);
  enemy_hand_view_.SetAway(true);
  orbit_target_ = 0.0f;
  // Fase de compra: quem começou não compra no primeiro turno.
  if (!(starter_ == Side::kPlayer && turns_played_[0] == 0)) DrawCard();
  SDL_Log("Turno %d: sua vez", turn_);
}

void BattleScene::EndTurn() {
  if (!player_turn_ || Busy() || hud_.Over()) return;
  player_turn_ = false;
  ++turns_played_[0];
  hand_.Deselect();
  attack_from_ = -1;
  attack_drag_ = false;
  DiscardExtraCards();
  // Virada de campo e turno do computador: a sua mão se recolhe e a dele aparece.
  hand_.SetAway(true);
  enemy_hand_view_.SetAway(false);
  orbit_target_ = 1.0f;
  Schedule(kOrbitSeconds + 0.2f, [this] { RunEnemyTurn(); });
}

void BattleScene::DiscardExtraCards() {
  // Fase final: com mais de 6 cartas, descarta até 6 (aqui, as de menor ataque).
  while (hand_.Size() > static_cast<size_t>(kHandLimit)) {
    size_t worst = 0;
    int worst_value = 1 << 30;
    for (size_t i = 0; i < hand_.Size(); ++i) {
      const CardInfo* info = hand_.CardAt(i).info;
      const int value = info == nullptr ? 0 : (info->IsField() ? 0 : info->attack);
      if (value < worst_value) {
        worst_value = value;
        worst = i;
      }
    }
    hand_.Remove(worst);
    hud_.SetGraveyard(hud_.Graveyard() + 1);
  }
}

void BattleScene::RunEnemyTurn() {
  if (hud_.Over()) return;
  turn_side_ = Side::kEnemy;
  enemy_board_.ResetAttacks();
  if (!(starter_ == Side::kEnemy && turns_played_[1] == 0)) EnemyDraw();
  SDL_Log("Turno %d: oponente (%zu na mao, %zu no monte)", turn_, enemy_hand_.size(),
          enemy_deck_.Size());
  // Fase principal: trocas de posição, carta de campo e invocação, com pausas.
  Schedule(kEnemyPause, [this] {
    for (int slot : ai::ChoosePositionChanges(BuildEnemyView())) {
      enemy_board_.TogglePosition(slot);
    }
  });
  // Carta de campo e invocação: a carta sai da mão dele, aparece no centro da tela por
  // kRevealSeconds e só então vai para a arena (passo agendado logo em seguida).
  Schedule(kEnemyPause, [this] {
    const int index = ai::ChooseFieldCard(BuildEnemyView());
    if (index < 0) return;
    const CardInfo* card = enemy_hand_[static_cast<size_t>(index)];
    math::Mat4 from;
    const bool had_view = static_cast<size_t>(index) < enemy_hand_view_.Size();
    EnemyHandRemove(static_cast<size_t>(index), &from);
    StartReveal(*card, i18n::Get("battle.reveal.field"), "", had_view ? &from : nullptr);
    ScheduleNext(RevealDuration(), [this, card] {
      reveal_ = Reveal{};
      PlayField(Side::kEnemy, *card);
    });
  });
  Schedule(kEnemyPause, [this] {
    const std::optional<ai::Summon> summon = ai::ChooseSummon(BuildEnemyView());
    if (!summon || summon->hand_index < 0 ||
        summon->hand_index >= static_cast<int>(enemy_hand_.size())) {
      return;
    }
    const CardInfo* card = enemy_hand_[static_cast<size_t>(summon->hand_index)];
    const bool defense = summon->defense;
    math::Mat4 from;
    const bool had_view = static_cast<size_t>(summon->hand_index) < enemy_hand_view_.Size();
    EnemyHandRemove(static_cast<size_t>(summon->hand_index), &from);
    StartReveal(*card, i18n::Get("battle.reveal.summon"),
                i18n::Get(defense ? "battle.reveal.defense" : "battle.reveal.attack"),
                had_view ? &from : nullptr);
    ScheduleNext(RevealDuration(), [this, card, defense] {
      const Face face{reveal_.front3d, reveal_.foil, card};
      reveal_ = Reveal{};
      EnemySummon(card, defense, face.texture != nullptr ? &face : nullptr);
    });
  });
  // Fase de batalha: um ataque por passo, reavaliando o campo a cada vez.
  Schedule(kEnemyPause + 0.3f, [this] { EnemyAttackStep(); });
}

void BattleScene::EnemyAttackStep() {
  if (hud_.Over()) return;
  const std::optional<ai::Attack> attack = ai::ChooseAttack(BuildEnemyView());
  if (!attack) {
    Schedule(kEnemyPause, [this] { FinishEnemyTurn(); });
    return;
  }
  PerformAttack(Side::kEnemy, attack->slot, attack->target_slot);
  Schedule(0.9f, [this] { EnemyAttackStep(); });
}

void BattleScene::FinishEnemyTurn() {
  ++turns_played_[1];
  // Fase final do computador: mão com mais de 6 cartas descarta as de menor ataque.
  while (enemy_hand_.size() > static_cast<size_t>(kHandLimit)) {
    auto worst = std::min_element(enemy_hand_.begin(), enemy_hand_.end(),
                                  [](const CardInfo* a, const CardInfo* b) {
                                    return a->attack < b->attack;
                                  });
    EnemyHandRemove(static_cast<size_t>(worst - enemy_hand_.begin()));
  }
  ++turn_;
  // A mão do oponente se recolhe primeiro; só depois a arena vira para o seu lado.
  enemy_hand_view_.SetAway(true);
  ScheduleNext(kHandHideSeconds, [this] {
    orbit_target_ = 0.0f;
    ScheduleNext(kOrbitSeconds + 0.2f, [this] { BeginPlayerTurn(); });
  });
}

ai::View BattleScene::BuildEnemyView() const {
  ai::View view;
  view.hand = enemy_hand_;
  for (int slot : enemy_board_.Occupied()) {
    const Board::Card* c = enemy_board_.At(slot);
    view.mine.push_back(ai::FieldCard{slot, c->info, c->defense,
                                      !c->defense && !c->attacked, c->summoned_turn == turn_});
  }
  for (int slot : board_.Occupied()) {
    const Board::Card* c = board_.At(slot);
    view.theirs.push_back(
        ai::FieldCard{slot, c->info, c->defense, false, c->summoned_turn == turn_});
  }
  view.field = field_;
  view.my_life = hud_.OpponentLife();
  view.their_life = hud_.PlayerLife();
  view.can_summon = true;  // uma invocação por turno: ChooseSummon é chamado uma vez
  view.can_attack = AttacksAllowed(Side::kEnemy);
  view.free_slots = static_cast<int>(enemy_board_.Free().size());
  return view;
}

void BattleScene::EnemySummon(const CardInfo* info, bool defense, const Face* ready_face) {
  if (info == nullptr) return;
  std::vector<int> free = enemy_board_.Free();
  int slot;
  if (free.empty()) {
    // Campo cheio: substitui a carta de menor ataque.
    slot = -1;
    int worst = 1 << 30;
    for (int s : enemy_board_.Occupied()) {
      const Board::Card* c = enemy_board_.At(s);
      const int value = c->info != nullptr ? c->info->attack : 0;
      if (value < worst) {
        worst = value;
        slot = s;
      }
    }
    if (slot < 0) return;
    enemy_board_.Discard(slot);
  } else {
    slot = ai::ChooseSlot(free, Board::kCols);
  }
  const Face face = ready_face != nullptr ? *ready_face : MakeCardFace(info->code);
  if (face.texture == nullptr) return;
  enemy_board_.Place(slot, face.texture, face.foil, defense, info, turn_);
  char name[3];
  SDL_Log("Oponente invocou %s em %s (%s)", info->name.c_str(), SlotName(slot, name),
          defense ? "defesa" : "ataque");
}

// ---------------------------------------------------------------------------------------------
// Ações do jogador

bool BattleScene::CanAct() const {
  return ready_ && started_ && player_turn_ && !hud_.Over() && !hand_.Inspecting() && !Busy();
}

bool BattleScene::CanSummon() const {
  if (!CanAct() || hand_.Candidate() < 0) return false;
  // Carta de campo não gasta a invocação normal do turno.
  return CandidateIsField() || !summoned_;
}

bool BattleScene::CandidateIsField() const {
  const int index = hand_.Candidate();
  if (index < 0) return false;
  const CardInfo* info = hand_.CardAt(static_cast<size_t>(index)).info;
  return info != nullptr && info->IsField();
}

void BattleScene::PlayField(Side owner, const CardInfo& card) {
  // A carta de campo não entra no campo: é consumida ao ser jogada e vai direto para o
  // cemitério do dono (o HUD conta só o seu). O efeito (buff/nerf) fica ativo até outra
  // carta de campo substituí-lo, e o painel do HUD mostra o efeito em vigor.
  if (owner == Side::kPlayer) hud_.SetGraveyard(hud_.Graveyard() + 1);
  field_ = rules::Field::From(card);
  field_owner_ = owner;
  FieldCardInfo info;
  info.name = card.name;
  info.favored = field_.favored == 'E' ? Category::kEvil : Category::kGood;
  info.buff = card.buff;
  info.nerf = card.nerf;
  info.image = HudFace(card);
  info.art = HudArt(card);  // o painel mostra só a ilustração, se houver
  info.art_large = HudArt(card, /*large=*/true);  // ampliação ao passar o mouse
  hud_.SetFieldCard(info);
  SDL_Log("%s ativou a carta de campo %s", owner == Side::kPlayer ? "Voce" : "Oponente",
          card.name.c_str());
}

void BattleScene::Summon(int slot) {
  const int index = hand_.Candidate();
  if (index < 0 || slot < 0 || slot >= Board::kSlots) return;
  const Hand::Card card = hand_.Remove(static_cast<size_t>(index));
  if (card.info != nullptr && card.info->IsField()) {
    PlayField(Side::kPlayer, *card.info);
    return;
  }
  // Slot ocupado: a carta anterior é descartada para o cemitério (docs/campo.md).
  if (!board_.Empty(slot) && board_.Discard(slot)) hud_.SetGraveyard(hud_.Graveyard() + 1);
  // Entra virada para cima, em modo de ataque (docs/batalha.md).
  if (!board_.Place(slot, card.front, card.foil, false, card.info, turn_)) return;
  summoned_ = true;
  board_.SetHighlight(false);
  board_.SetHoverSlot(-1);
  char name[3];
  SDL_Log("Turno %d: carta invocada em %s", turn_, SlotName(slot, name));
}

// ---------------------------------------------------------------------------------------------
// Combate

void BattleScene::PerformAttack(Side attacker, int slot, int target_slot) {
  Board& mine = BoardOf(attacker);
  Board& theirs = BoardOf(Other(attacker));
  Board::Card* card = mine.At(slot);
  if (card == nullptr || card->attacked || card->defense) return;
  if (target_slot >= 0 && theirs.Empty(target_slot)) return;
  if (target_slot < 0 && theirs.Count() > 0) return;  // direto só com o campo vazio
  card->attacked = true;
  // Investida até a carta alvo ou, no ataque direto, até o meio do lado do oponente.
  const math::Vec3 target = target_slot >= 0
                                ? theirs.CardPosition(target_slot)
                                : theirs.SlotPosition(Board::kCols / 2 + Board::kCols);
  mine.Lunge(slot, target);
  Schedule(kAttackHit, [this, attacker, slot, target_slot] {
    ResolveAttack(attacker, slot, target_slot);
  });
}

void BattleScene::ResolveAttack(Side attacker, int slot, int target_slot) {
  Board& mine = BoardOf(attacker);
  Board& theirs = BoardOf(Other(attacker));
  const Board::Card* card = mine.At(slot);
  if (card == nullptr || card->info == nullptr) return;
  const int attack = rules::EffectiveAttack(*card->info, field_);
  rules::Outcome outcome;
  char a_name[3], d_name[3];
  // Golpe de espada no alvo (carta ou, no ataque direto, o meio do lado do oponente).
  const math::Vec3 hit = target_slot >= 0
                             ? theirs.CardPosition(target_slot)
                             : theirs.SlotPosition(Board::kCols / 2 + Board::kCols);
  sword_.Play(hit, mine.CardPosition(slot));
  if (target_slot >= 0) theirs.Flash(target_slot);
  if (target_slot < 0) {
    outcome = rules::ResolveDirect(attack);
    SDL_Log("%s: ataque direto de %s (%d)", attacker == Side::kPlayer ? "Voce" : "Oponente",
            card->info->name.c_str(), attack);
  } else {
    Board::Card* target = theirs.At(target_slot);
    if (target == nullptr || target->info == nullptr) return;
    outcome = rules::Resolve(attack, target->defense,
                             rules::EffectiveAttack(*target->info, field_),
                             rules::EffectiveDefense(*target->info, field_));
    // Carta em defesa: o escudo surge sobre ela e segura o golpe da espada (ou quebra, se ela
    // não aguenta).
    if (target->defense) {
      shield_.Play(hit, mine.CardPosition(slot), sword_.HitDelay(), outcome.defender_destroyed);
    }
    SDL_Log("%s: %s (%s, %d) ataca %s (%s, %s)", attacker == Side::kPlayer ? "Voce" : "Oponente",
            card->info->name.c_str(), SlotName(slot, a_name), attack,
            target->info->name.c_str(), SlotName(target_slot, d_name),
            target->defense ? "defesa" : "ataque");
  }
  if (outcome.defender_destroyed) Destroy(Other(attacker), target_slot);
  if (outcome.attacker_destroyed) Destroy(attacker, slot);
  if (outcome.defender_damage > 0) ApplyDamage(Other(attacker), outcome.defender_damage);
  if (outcome.attacker_damage > 0) ApplyDamage(attacker, outcome.attacker_damage);
  if (hud_.Over()) {
    // Fim da batalha: nada mais acontece; o HUD mostra o resultado.
    steps_.clear();
    reveal_ = Reveal{};
    player_turn_ = false;
    hand_.SetAway(true);
    SDL_Log("Fim: %s", hud_.PlayerLife() > 0 ? "vitoria" : "derrota");
  }
}

void BattleScene::Destroy(Side owner, int slot) {
  if (!BoardOf(owner).Discard(slot)) return;
  if (owner == Side::kPlayer) hud_.SetGraveyard(hud_.Graveyard() + 1);
  // Carta abatida: a arena treme (as duas destruídas no mesmo golpe não somam).
  shake_left_ = kShakeSeconds;
}

void BattleScene::ApplyDamage(Side side, int damage) {
  if (side == Side::kPlayer) {
    hud_.SetLife(hud_.OpponentLife(), hud_.PlayerLife() - damage);
  } else {
    hud_.SetLife(hud_.OpponentLife() - damage, hud_.PlayerLife());
  }
}

math::Vec2 BattleScene::ScreenOf(const math::Vec3& world, int width, int height) const {
  const math::Vec4 clip = arena_view_proj_ * math::Vec4{world.x, world.y, world.z, 1.0f};
  const float w = clip.w > 1e-6f ? clip.w : 1e-6f;
  return {(clip.x / w * 0.5f + 0.5f) * width, (1.0f - (clip.y / w * 0.5f + 0.5f)) * height};
}

// ---------------------------------------------------------------------------------------------
// Cena

void BattleScene::OnEnter(SceneContext& ctx) {
  // Sem Preload antes (entrada direta na batalha), carrega tudo aqui mesmo.
  if (!preloaded_) Preload(ctx.Preload());
  renderer_ = ctx.renderer;

  end_turn_ = std::make_unique<ButtonAction>(i18n::Get("battle.endTurn"),
                                             [this](SceneContext&) { EndTurn(); });
  SDL_FRect rect{(ctx.logical_width - kButtonWidth) / 2.0f, kEndTurnTop, kButtonWidth,
                 kButtonHeight};
  end_turn_->SetRect(rect);
  sound_button_.SetRect(SDL_FRect{kButtonMargin, ctx.logical_height - kButtonMargin - kSoundButtonSize,
                                  kSoundButtonSize, kSoundButtonSize});
  hud_.SetDeckPanelLeft(kButtonMargin + kSoundButtonSize + kSoundButtonGap);
  started_ = false;
  player_turn_ = false;
  summoned_ = false;
  turn_ = 1;
  turns_played_[0] = turns_played_[1] = 0;
  steps_.clear();
  reveal_ = Reveal{};
  field_ = rules::Field{};

  // HUD (docs/hud.md): vida cheia, sem carta de campo, cemitério vazio e monte com a mão
  // inicial já comprada. Não depende do render 3D.
  hud_.SetLife(Hud::kMaxLife, Hud::kMaxLife);
  hud_.ClearFieldCard();
  hud_.SetGraveyard(0);
  // Fundo = cor da névoa: o que some na distância se funde com ele (assets/Fog.json, que
  // por padrão aponta para os papéis background.fog e background.mist de colors.json).
  fog_color_ = config::Color("fog", "battle.fog.color", colors::GetF("background.fog"));
  mist_color_ = config::Color("fog", "battle.mist.color", colors::GetF("background.mist"));
  if (field_front_2d_ == nullptr) {
    field_front_2d_ = UploadHudSprite(ctx.renderer, hud_field_);
    hud_field_ = Image{};
  }

  if (gpu_ == nullptr || card_model_ == nullptr) {
    SDL_Log("Falha ao iniciar o render 3D");
    return;
  }
  gpu_->SetRenderer(ctx.renderer);

  hand_.SetModel(card_model_.get());
  hand_.SetAura(&card_aura_);
  hand_.SetBack(back_);
  hand_.SetNormalMaps(front_normal_, back_normal_);
  // Mão inicial: as 5 cartas compradas do monte embaralhado no Preload (ver docs/batalha.md).
  // Entram pela esquerda, uma a uma, depois de o fade in da entrada terminar.
  hand_.DelayDeal(0.8f);
  for (const Face& face : initial_hand_) hand_.Add(face.texture, face.foil, face.info);
  initial_hand_.clear();
  // Sorteio de quem começa (PONTIFICE_STARTER força, para depuração). Feito já na entrada: se
  // o oponente começa, a sua mão fica recolhida desde o início e só aparece no seu turno.
  starter_ = std::uniform_int_distribution<int>(0, 1)(rng_) == 0 ? Side::kPlayer : Side::kEnemy;
  if (const char* env = SDL_getenv("PONTIFICE_STARTER")) {
    if (std::strcmp(env, "player") == 0) starter_ = Side::kPlayer;
    if (std::strcmp(env, "enemy") == 0) starter_ = Side::kEnemy;
  }
  if (starter_ == Side::kEnemy) hand_.SetAwayNow(true);
  // Mão do oponente: mesmas cartas 3D, frente oculta, um pouco mais à vista (sem o mouse ela
  // não sobe); começa recolhida e aparece no turno dele.
  enemy_hand_view_.SetModel(card_model_.get());
  enemy_hand_view_.SetBack(back_);
  enemy_hand_view_.SetNormalMaps(front_normal_, back_normal_);
  enemy_hand_view_.GetLayout().hidden_drop = 0.35f;
  enemy_hand_view_.Clear();
  for (size_t i = 0; i < enemy_hand_.size() && hidden_face_ != nullptr; ++i) {
    enemy_hand_view_.Add(hidden_face_, false, nullptr);
  }
  enemy_hand_view_.SetAwayNow(true);
  hud_.SetDeck(Hud::kDeckSize - static_cast<int>(deck_.Size()), static_cast<int>(deck_.Size()));
  arena_world_ = math::Mat4::Scale(kArenaScale);

  const float aspect = static_cast<float>(ctx.logical_width) / ctx.logical_height;
  proj_ = math::Mat4::Perspective(math::Radians(35.0f), aspect, 0.1f, 100.0f);
  hand_view_proj_ = proj_ * math::Mat4::LookAt({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
  // Vista do jogador: acima e à frente do seu lado da arena, olhando para o centro.
  math::Vec3 eye{0, 10.5f, 12.2f};
  if (const char* env = SDL_getenv("PONTIFICE_ARENA_EYE")) {  // depuração: "x,y,z"
    SDL_sscanf(env, "%f,%f,%f", &eye.x, &eye.y, &eye.z);
  }
  eye_final_ = eye;
  arena_view_proj_ = proj_ * math::Mat4::LookAt(eye, kArenaTarget, {0, 1, 0});

  // Clima medieval: a luz da arena vem só das duas tochas (luzes pontuais nas chamas, cor
  // amarelo-branca de Torch::LightColor, com tremular); sem luz direcional, ambiente frio e
  // fraco, névoa na base e vinheta. Valores em assets/Light.json (battle.arena, torch),
  // Fog.json (battle.fog, battle.mist) e Shadow.json (battle).
  arena_atmosphere_.light_dir = config::Vec3("light", "battle.arena.lightDir", {-0.35f, 1.0f, 0.45f});
  arena_atmosphere_.light_intensity = config::Float("light", "battle.arena.lightIntensity", 0.0f);
  arena_atmosphere_.light_color = Torch::LightColor();
  arena_atmosphere_.ambient = config::Color("light", "battle.arena.ambient", {0.07f, 0.08f, 0.13f, 1.0f});
  arena_atmosphere_.point_intensity = 0.0f;  // acendem quando o fogo surge nas tochas
  arena_atmosphere_.point2_intensity = 0.0f;
  arena_atmosphere_.fog_color = fog_color_;
  arena_atmosphere_.fog_density = config::Float("fog", "battle.fog.density", 0.05f);
  arena_atmosphere_.fog_start = config::Float("fog", "battle.fog.start", 15.0f);
  arena_atmosphere_.fog_full_y = kArenaBaseY + config::Float("fog", "battle.fog.fullYOffset", 0.0f);
  arena_atmosphere_.fog_clear_y = kArenaTopY + config::Float("fog", "battle.fog.clearYOffset", -0.2f);
  arena_atmosphere_.fog_height_strength = config::Float("fog", "battle.fog.heightStrength", 0.9f);
  arena_atmosphere_.camera_pos = eye;
  arena_atmosphere_.vignette = config::Float("light", "battle.arena.vignette", 0.55f);
  arena_atmosphere_.shadow_strength = config::Float("shadow", "battle.strength", 0.85f);
  arena_atmosphere_.shadow_bias = config::Float("shadow", "bias", 0.0015f);
  arena_atmosphere_.mist_fade_start = config::Float("fog", "battle.mist.fadeStart", 8.0f);
  arena_atmosphere_.mist_fade_end = config::Float("fog", "battle.mist.fadeEnd", 28.0f);
  arena_atmosphere_.mist_noise_scale = config::Float("fog", "battle.mist.noiseScale", 1.0f);
  torch_light_ = config::Float("light", "torch.battle.intensity", 2.6f);
  shadow_proj_ = math::Mat4::Perspective(
      math::Radians(config::Float("shadow", "battle.fov", 130.0f)), 1.0f,
      config::Float("shadow", "battle.near", 0.3f), config::Float("shadow", "battle.far", 60.0f));
  shadow_target_ = config::Vec3("shadow", "battle.target", {0.0f, 0.3f, 0.0f});

  // Mão: mesma luz da tocha (principal e pontual, com o mesmo tremular), ambiente um
  // pouco mais claro para as cartas ficarem legíveis. Sem névoa nem vinheta.
  hand_atmosphere_.light_dir = config::Vec3("light", "battle.hand.lightDir", {0.3f, 0.6f, 1.0f});
  hand_atmosphere_.light_intensity = config::Float("light", "battle.hand.lightIntensity", 0.5f);
  hand_atmosphere_.light_color = arena_atmosphere_.light_color;
  hand_atmosphere_.ambient = config::Color("light", "battle.hand.ambient", {0.34f, 0.36f, 0.46f, 1.0f});
  hand_atmosphere_.point_pos = config::Vec3("light", "battle.hand.pointPos", {0.0f, 2.5f, 4.0f});
  hand_atmosphere_.point_intensity = config::Float("light", "battle.hand.pointIntensity", 1.0f);
  hand_atmosphere_.camera_pos = {0, 0, 10};
  ready_ = true;
  UpdateIntro(0.0f);  // primeiro quadro já com a arena afundada e a câmera longe
}

void BattleScene::UpdateIntro(float dt_seconds) {
  intro_time_ += dt_seconds;
  if (intro_ >= 1.0f && torch_intro_ >= 1.0f && ignite_ >= 1.0f) return;
  intro_ = SmoothStep(intro_time_ / kIntroSeconds);
  torch_intro_ = SmoothStep((intro_time_ - kTorchIntroStart) / kTorchIntroSeconds);
  ignite_ = SmoothStep((intro_time_ - kTorchIntroStart - kTorchIntroSeconds) / kIgniteSeconds);
  // Depuração: PONTIFICE_ARENA_EYE fixa a câmera, então pula a abertura.
  if (SDL_getenv("PONTIFICE_ARENA_EYE") != nullptr) intro_ = torch_intro_ = ignite_ = 1.0f;
  const float drop = kIntroDrop * (1.0f - intro_);
  arena_world_ = math::Mat4::Translate({0.0f, -drop, 0.0f}) * math::Mat4::Scale(kArenaScale);
  // Tochas emergem do chão depois da arena; o fogo acende depois, e a luz vem com ele.
  const float torch_drop = kTorchIntroDrop * (1.0f - torch_intro_);
  torch_world_[0] = math::Mat4::Translate({-kTorchX, kArenaBaseY - torch_drop, kTorchZ}) *
                    math::Mat4::RotateZ(kTorchTilt) * math::Mat4::Scale(kTorchScale);
  torch_world_[1] = math::Mat4::Translate({kTorchX, kArenaBaseY - torch_drop, kTorchZ}) *
                    math::Mat4::RotateZ(-kTorchTilt) * math::Mat4::Scale(kTorchScale);
  arena_atmosphere_.point_pos = Torch::FlamePosition(torch_world_[0]);
  arena_atmosphere_.point2_pos = Torch::FlamePosition(torch_world_[1]);
  // Sombras: cada chama olha para o centro da arena com campo bem aberto.
  for (int k = 0; k < 2; ++k) {
    const math::Vec3 flame = k == 0 ? arena_atmosphere_.point_pos : arena_atmosphere_.point2_pos;
    torch_light_vp_[k] = shadow_proj_ * math::Mat4::LookAt(flame, shadow_target_, {0, 1, 0});
  }
  arena_atmosphere_.point_intensity = torch_light_ * ignite_;
  arena_atmosphere_.point2_intensity = torch_light_ * ignite_;
  const math::Vec3 eye = kIntroEyeStart + (eye_final_ - kIntroEyeStart) * intro_;
  arena_view_proj_ = proj_ * math::Mat4::LookAt(eye, kArenaTarget, {0, 1, 0});
  arena_atmosphere_.camera_pos = eye;
}

void BattleScene::UpdateCamera() {
  // Virada de campo: a câmera gira em torno do eixo vertical pelo centro da arena.
  if (intro_ < 1.0f) return;
  const float angle = math::kPi * SmoothStep(orbit_);
  const float c = std::cos(angle), s = std::sin(angle);
  const math::Vec3 e = eye_final_;
  math::Vec3 eye{e.x * c + e.z * s, e.y, -e.x * s + e.z * c};
  math::Vec3 target = kArenaTarget;
  // Tremor (carta abatida): câmera e alvo sacodem juntos em três senos de frequências
  // diferentes, forte no início e sumindo. Só a arena treme: mão e HUD ficam parados.
  if (shake_left_ > 0.0f) {
    const float k = shake_left_ / kShakeSeconds;
    const float a = kShakeAmplitude * k * k;
    const float t = shake_time_;
    const math::Vec3 offset{a * std::sin(t * 57.0f), a * 0.8f * std::sin(t * 43.0f + 1.3f),
                            a * 0.6f * std::sin(t * 67.0f + 2.1f)};
    eye = eye + offset;
    target = target + offset * 0.6f;
  }
  arena_view_proj_ = proj_ * math::Mat4::LookAt(eye, target, {0, 1, 0});
  arena_atmosphere_.camera_pos = eye;
}

void BattleScene::Pause(SceneContext& ctx) {
  paused_ = true;
  // Solta o que estava em andamento com o mouse (arrasto de ataque).
  attack_from_ = -1;
  attack_drag_ = false;
  press_ = {-1.0f, -1.0f};
  if (ctx.music != nullptr) {
    ctx.music->SetScene("menu");
    ctx.music->SetCategory("ost");
  }
  // Sim = abandonar: ação destrutiva, placa vermelha.
  abandon_dialog_.Open(i18n::Get("battle.abandon.question"), i18n::Get("battle.abandon.no"),
                       i18n::Get("battle.abandon.yes"), ctx.logical_width, ctx.logical_height,
                       /*yes_danger=*/true);
}

void BattleScene::Resume(SceneContext& ctx) {
  paused_ = false;
  abandon_dialog_.Close();
  if (ctx.music != nullptr) {
    ctx.music->SetScene("battle");
    ctx.music->SetCategory("battle", /*resume=*/true);  // continua a faixa de onde parou
  }
}

void BattleScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  // Pausado: só a janela de confirmação recebe eventos.
  if (paused_) {
    switch (abandon_dialog_.HandleEvent(event)) {
      case ConfirmDialog::Result::kNo:
        Resume(ctx);
        break;
      case ConfirmDialog::Result::kYes:
        abandon_dialog_.Close();
        ctx.next_scene = SceneId::kMenu;  // o menu já toca a trilha dele
        break;
      case ConfirmDialog::Result::kNone:
        break;
    }
    return;
  }
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
        if (hand_.Inspecting()) {  // primeiro sai da inspeção; só depois pausa
          hand_.StopInspect();
          return;
        }
        // Batalha encerrada (vitória/derrota): Esc volta direto ao menu. Em andamento: pausa
        // e pergunta se abandona.
        if (hud_.Over()) {
          ctx.next_scene = SceneId::kMenu;
        } else {
          Pause(ctx);
        }
        return;
      case SDLK_SPACE:  // vira a carta inspecionada
        hand_.Flip();
        return;
      case SDLK_F:  // depuração: carta de campo de exemplo
        ToggleSampleFieldCard();
        return;
      case SDLK_1:  // depuração: dano no oponente
        if (!hud_.Over()) hud_.SetLife(hud_.OpponentLife() - 700, hud_.PlayerLife());
        return;
      case SDLK_2:  // depuração: dano em você
        if (!hud_.Over()) hud_.SetLife(hud_.OpponentLife(), hud_.PlayerLife() - 700);
        return;
      case SDLK_D:  // depuração: compra do monte
        DrawCard();
        return;
      case SDLK_G:  // depuração: carta para o cemitério
        hud_.SetGraveyard(hud_.Graveyard() + 1);
        return;
      default:
        break;
    }
  }
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    mouse_ = {event.motion.x, event.motion.y};
    if (attack_from_ >= 0 && !attack_drag_ && press_.x >= 0) {
      const float moved = std::fabs(mouse_.x - press_.x) + std::fabs(mouse_.y - press_.y);
      if (moved >= kAttackDragPx) attack_drag_ = true;
    }
  }
  // Botão de som: sempre disponível; fica com os eventos de mouse sobre ele.
  if (sound_button_.HandleEvent(event)) {
    if (ctx.music != nullptr) ctx.music->ToggleMuted();
    return;
  }
  if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
      sound_button_.Contains(event.button.x, event.button.y)) {
    return;
  }
  // "Passar turno" (visível no seu turno) fica com os eventos de mouse sobre ele.
  if (CanAct()) {
    if (end_turn_->HandleEvent(event)) {
      end_turn_->OnClick(ctx);
      return;
    }
    if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
        end_turn_->Contains(event.button.x, event.button.y)) {
      return;
    }
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    const math::Vec2 pos{event.button.x, event.button.y};
    if (event.button.button == SDL_BUTTON_LEFT) {
      press_ = pos;
      attack_from_ = -1;
      attack_drag_ = false;
      // Pressionar uma carta sua em ataque que ainda não atacou: começa um arrasto de ataque.
      if (CanAct() && AttacksAllowed(Side::kPlayer) && hand_.Hovered() < 0) {
        const int slot = board_.PickSlot(pos, arena_view_proj_, ctx.logical_width,
                                         ctx.logical_height);
        const Board::Card* card = board_.At(slot);
        if (card != nullptr && !card->defense && !card->attacked) {
          attack_from_ = slot;
        }
      }
    }
    hand_.MouseDown(pos, event.button.button == SDL_BUTTON_LEFT);
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
    const math::Vec2 pos{event.button.x, event.button.y};
    const bool inspecting = hand_.Inspecting();
    hand_.MouseUp(pos);
    const float moved = std::fabs(pos.x - press_.x) + std::fabs(pos.y - press_.y);
    if (attack_from_ >= 0 && attack_drag_) {
      // Soltar sobre uma carta do oponente a ataca; sobre o lado vazio dele, ataque direto.
      if (CanAct()) {
        const int target = enemy_board_.PickSlot(pos, arena_view_proj_, ctx.logical_width,
                                                 ctx.logical_height);
        if (target >= 0 && !enemy_board_.Empty(target)) {
          PerformAttack(Side::kPlayer, attack_from_, target);
        } else if (target >= 0 && enemy_board_.Count() == 0) {
          PerformAttack(Side::kPlayer, attack_from_, -1);
        }
      }
    } else if (!inspecting && press_.x >= 0 && moved < kClickSlop && hand_.Hovered() < 0 &&
               CanAct()) {
      // Clique (sem arrasto) num slot do seu lado: com carta selecionada, invoca (num slot
      // ocupado a carta anterior é descartada); sem seleção, numa carta sua, alterna ataque e
      // defesa. Com o mouse sobre uma carta da mão o clique é dela, não do slot atrás.
      const int slot =
          board_.PickSlot(pos, arena_view_proj_, ctx.logical_width, ctx.logical_height);
      if (CanSummon()) {
        const bool field = CandidateIsField();
        // Carta de campo: qualquer slot da arena serve (o slot de campo não está modelado).
        const int enemy_slot = enemy_board_.PickSlot(pos, arena_view_proj_, ctx.logical_width,
                                                     ctx.logical_height);
        if (slot >= 0 || (field && enemy_slot >= 0)) Summon(field ? 0 : slot);
      } else if (slot >= 0 && board_.TogglePosition(slot)) {
        char name[3];
        SDL_Log("Turno %d: carta de %s em modo de %s", turn_, SlotName(slot, name),
                board_.At(slot)->defense ? "defesa" : "ataque");
      }
    }
    press_ = {-1, -1};
    attack_from_ = -1;
    attack_drag_ = false;
  }
  if (event.type == SDL_EVENT_MOUSE_WHEEL) {
    hand_.Wheel(event.wheel.y);
  }
  if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
    mouse_ = {-1, -1};
  }
}

void BattleScene::Update(SceneContext& ctx, float dt_seconds) {
  if (!ready_ || paused_) return;  // pausa: abertura, câmera, IA e animações congelam
  hud_.Update(dt_seconds, mouse_.x, mouse_.y);  // ampliação da ilustração do painel de campo
  if (reveal_.card != nullptr) reveal_.time += dt_seconds;
  UpdateIntro(dt_seconds);
  // Virada de campo.
  if (shake_left_ > 0.0f) {
    shake_left_ = std::max(0.0f, shake_left_ - dt_seconds);
    shake_time_ += dt_seconds;
  }
  if (orbit_ < orbit_target_) orbit_ = std::min(orbit_target_, orbit_ + dt_seconds / kOrbitSeconds);
  if (orbit_ > orbit_target_) orbit_ = std::max(orbit_target_, orbit_ - dt_seconds / kOrbitSeconds);
  UpdateCamera();
  // Partida começa quando as tochas já estão acesas.
  if (!started_ && intro_time_ >= kMatchStart) StartMatch();
  // Passos agendados (turno do computador, impacto dos ataques): um por vez, com pausa.
  if (!steps_.empty()) {
    step_timer_ += dt_seconds;
    if (step_timer_ >= steps_.front().first) {
      step_timer_ = 0.0f;
      auto fn = std::move(steps_.front().second);
      steps_.pop_front();
      fn();
    }
  } else {
    step_timer_ = 0.0f;
  }

  hand_.Update(dt_seconds, mouse_, hand_view_proj_, ctx.logical_width, ctx.logical_height);
  enemy_hand_view_.Update(dt_seconds, math::Vec2{-1.0f, -1.0f}, hand_view_proj_,
                          ctx.logical_width, ctx.logical_height);
  // Destaques do campo: slots vazios em modo de invocação; moldura amarela no slot sob o
  // mouse quando dá para invocar nele, trocar a posição da carta ou, num arrasto de ataque,
  // no alvo do oponente.
  const bool summoning = CanSummon();
  board_.SetHighlight(summoning);
  int hover = -1;
  int enemy_hover = -1;
  if (attack_from_ >= 0 && attack_drag_) {
    hover = attack_from_;
    const int target = enemy_board_.PickSlot(mouse_, arena_view_proj_, ctx.logical_width,
                                             ctx.logical_height);
    if (target >= 0 && (!enemy_board_.Empty(target) || enemy_board_.Count() == 0)) {
      enemy_hover = target;
    }
  } else if (CanAct() && hand_.Hovered() < 0) {
    hover = board_.PickSlot(mouse_, arena_view_proj_, ctx.logical_width, ctx.logical_height);
    if (!summoning && board_.Empty(hover)) hover = -1;
  }
  board_.SetHoverSlot(hover);
  enemy_board_.SetHoverSlot(enemy_hover);
  board_.Update(dt_seconds);
  enemy_board_.Update(dt_seconds);
  sword_.Update(dt_seconds);
  shield_.Update(dt_seconds);
  // Tooltip e ampliação: carta de qualquer lado sob o mouse (fora da mão e sem inspeção).
  tooltip_slot_ = -1;
  if (intro_ >= 1.0f && hand_.Hovered() < 0 && !hand_.Inspecting() && mouse_.x >= 0) {
    for (Side side : {Side::kPlayer, Side::kEnemy}) {
      const Board& board = side == Side::kPlayer ? board_ : enemy_board_;
      const int slot =
          board.PickSlot(mouse_, arena_view_proj_, ctx.logical_width, ctx.logical_height);
      if (slot >= 0 && !board.Empty(slot)) {
        tooltip_slot_ = slot;
        tooltip_side_ = side;
        break;
      }
    }
  }
  board_.SetHoverCard(tooltip_slot_ >= 0 && tooltip_side_ == Side::kPlayer ? tooltip_slot_ : -1);
  enemy_board_.SetHoverCard(tooltip_slot_ >= 0 && tooltip_side_ == Side::kEnemy ? tooltip_slot_
                                                                                : -1);
}

// Entrada e saída da revelação: alpha e escala no instante t (s).
namespace {
void RevealCurve(float t, float& alpha, float& scale) {
  const float in = SmoothStep(t / kRevealFadeIn);
  const float out = 1.0f - SmoothStep((t - (kRevealSeconds - kRevealFadeOut)) / kRevealFadeOut);
  alpha = std::min(in, out);
  scale = (0.85f + 0.15f * in) * (0.92f + 0.08f * out);
}
}  // namespace

void BattleScene::RenderReveal3D(int width, int height) {
  if (reveal_.card == nullptr || reveal_.front3d == nullptr) return;
  const float travel = reveal_.from_hand ? kRevealTravel : 0.0f;
  const float hold = reveal_.time - travel;  // tempo no centro (negativo durante o voo)
  float alpha = 0.0f, scale = 1.0f;
  RevealCurve(hold, alpha, scale);
  float veil_alpha = alpha;
  if (reveal_.from_hand) {
    // Saindo da mão: a carta já chega opaca e no tamanho final (sem a entrada em fade); só a
    // saída no fim vale. O véu escurece durante o voo.
    RevealCurve(std::max(hold, kRevealFadeIn), alpha, scale);
    veil_alpha = std::min(SmoothStep(reveal_.time / travel), alpha);
  }
  const Hand::Layout& layout = hand_.GetLayout();
  // Véu: quad escuro atrás da carta cobrindo a vista (escurece a mesa e a sua mão).
  const float veil_z = layout.inspect_z - 1.0f;
  const float veil_h = 2.2f * (layout.camera_z - veil_z) * std::tan(layout.fov_y * 0.5f);
  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  SDL_FColor veil = colors::GetF("background.overlay");
  veil.a *= veil_alpha;
  gpu_->Draw(veil_quad_, nullptr,
             math::Mat4::Translate({0.0f, 0.0f, veil_z}) *
                 math::Mat4::Scale({veil_h * aspect, veil_h, 1.0f}),
             hand_view_proj_, veil, Gpu::DrawMode::kEmissive);
  math::Vec3 center;
  float center_scale = 1.0f;
  hand_.CenterPose(kRevealCardH * scale / static_cast<float>(height), center, center_scale);
  if (reveal_.from_hand && hold < 0.0f) {
    // Voo: da pose na mão até o centro, crescendo e endireitando o leque; vira em Y no
    // caminho, com a frente oculta até 90° e a carta real depois.
    const float u = SmoothStep(reveal_.time / travel);
    const math::Vec3 pos = reveal_.from_pos + (center - reveal_.from_pos) * u;
    const float s = math::Lerp(reveal_.from_scale, center_scale, u);
    const float roll = math::Lerp(reveal_.from_roll, 0.0f, u);
    const float flip = math::kPi * u;
    const bool hidden = flip < math::kPi * 0.5f;
    const math::Mat4 world =
        Hand::PoseMatrix(pos, s, hidden ? flip : flip - math::kPi, 0.0f, roll);
    hand_.DrawLoose(*gpu_, hand_view_proj_, world, hidden ? hidden_face_ : reveal_.front3d,
                    !hidden && reveal_.foil, 1.0f);
    return;
  }
  // No centro: balança em Y (e um pouco em X), para o arco-íris do foil correr pela frente; o
  // balanço entra suave depois do voo.
  const float t = std::max(hold, 0.0f);
  const float sway = reveal_.from_hand ? SmoothStep(t / 0.3f) : 1.0f;
  const float yaw = sway * 0.42f * std::sin(t * 3.1f);
  const float pitch = sway * 0.1f * std::sin(t * 2.3f);
  const math::Mat4 world = Hand::PoseMatrix(center, center_scale, yaw, pitch, 0.0f);
  hand_.DrawLoose(*gpu_, hand_view_proj_, world, reveal_.front3d, reveal_.foil, alpha);
}

void BattleScene::RenderReveal(SDL_Renderer* r, int width, int height) const {
  if (reveal_.card == nullptr) return;
  if (reveal_.front3d != nullptr) {
    // Carta e véu já foram desenhados em 3D (RenderReveal3D): aqui só as legendas, que
    // entram depois do voo da mão.
    float alpha = 0.0f, scale = 1.0f;
    const float hold = reveal_.time - (reveal_.from_hand ? kRevealTravel : 0.0f);
    RevealCurve(hold, alpha, scale);
    if (reveal_.from_hand) {
      float unused = 0.0f;
      RevealCurve(std::max(hold, kRevealFadeIn), unused, scale);  // mesma escala da carta 3D
    }
    const auto with_alpha = [alpha](SDL_Color c) {
      c.a = static_cast<Uint8>(c.a * alpha);
      return c;
    };
    const float h = kRevealCardH * scale;
    const float cx = width / 2.0f;
    const float top = height / 2.0f - h / 2.0f;
    text::DrawCentered(r, reveal_.caption.c_str(), cx, top - ui::Grid(3), kRevealCaptionScale,
                       with_alpha(colors::Get("panel.title")));
    if (!reveal_.detail.empty()) {
      text::DrawCentered(r, reveal_.detail.c_str(), cx, top + h + ui::Grid(3),
                         kRevealDetailScale, with_alpha(colors::Get("text.secondary")));
    }
    return;
  }
  if (reveal_.face == nullptr) return;
  // Entrada: cresce de 85% e clareia; saída: encolhe um pouco e some, logo antes de a carta
  // descer na arena.
  const float in = SmoothStep(reveal_.time / kRevealFadeIn);
  const float out =
      1.0f - SmoothStep((reveal_.time - (kRevealSeconds - kRevealFadeOut)) / kRevealFadeOut);
  const float alpha = std::min(in, out);
  const float scale = (0.85f + 0.15f * in) * (0.92f + 0.08f * out);
  const auto with_alpha = [alpha](SDL_Color c) {
    c.a = static_cast<Uint8>(c.a * alpha);
    return c;
  };

  SDL_BlendMode previous = SDL_BLENDMODE_NONE;
  SDL_GetRenderDrawBlendMode(r, &previous);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  // Véu leve: a arena continua visível atrás.
  const SDL_Color veil = with_alpha(colors::Get("background.overlay"));
  SDL_SetRenderDrawColor(r, veil.r, veil.g, veil.b, veil.a);
  const SDL_FRect full{0, 0, static_cast<float>(width), static_cast<float>(height)};
  SDL_RenderFillRect(r, &full);

  const float h = kRevealCardH * scale;
  const float w = h * kCardAspect;
  const float cx = width / 2.0f;
  const float cy = height / 2.0f + ui::Grid(1);
  const SDL_FRect card{cx - w / 2.0f, cy - h / 2.0f, w, h};
  // Carta foil (hierarquia 1): aura atrás, que pulsa e gira em arco-íris; um anel largo e
  // suave por fora e um mais estreito, quase branco, junto à borda.
  if (reveal_.card->hierarchy == 1) {
    const float t = static_cast<float>(SDL_GetTicks()) / 1000.0f;
    const float pulse = 0.8f + 0.2f * std::sin(t * 2.6f);
    SDL_FColor wide[4], near_edge[4];
    for (int k = 0; k < 4; ++k) {
      wide[k] = Hue(t * 0.12f + k * 0.25f, 0.55f * alpha * pulse);
      const SDL_FColor hue = Hue(t * 0.12f + k * 0.25f, 0.9f * alpha * pulse);
      near_edge[k] = SDL_FColor{0.55f + 0.45f * hue.r, 0.55f + 0.45f * hue.g,
                                0.5f + 0.45f * hue.b, hue.a};
    }
    DrawGlowRing(r, card, ui::Grid(6) * scale, wide);
    DrawGlowRing(r, card, ui::Grid(2) * scale, near_edge);
  }
  SDL_SetTextureAlphaMod(reveal_.face, static_cast<Uint8>(alpha * 255.0f));
  SDL_RenderTexture(r, reveal_.face, nullptr, &card);
  SDL_SetTextureAlphaMod(reveal_.face, 255);  // textura compartilhada com HUD e tooltip

  text::DrawCentered(r, reveal_.caption.c_str(), cx, card.y - ui::Grid(3), kRevealCaptionScale,
                     with_alpha(colors::Get("panel.title")));
  if (!reveal_.detail.empty()) {
    text::DrawCentered(r, reveal_.detail.c_str(), cx, card.y + card.h + ui::Grid(3),
                       kRevealDetailScale, with_alpha(colors::Get("text.secondary")));
  }
  SDL_SetRenderDrawBlendMode(r, previous);
}

void BattleScene::RenderTooltip(SDL_Renderer* r, int width, int height) const {
  if (tooltip_slot_ < 0) return;
  const Board& board = tooltip_side_ == Side::kPlayer ? board_ : enemy_board_;
  const Board::Card* card = board.At(tooltip_slot_);
  if (card == nullptr || card->info == nullptr) return;
  const CardInfo& info = *card->info;
  const int attack = rules::EffectiveAttack(info, field_);
  const int defense = rules::EffectiveDefense(info, field_);

  // Linhas de texto: nome; categoria e hierarquia; ATK/DEF efetivos (e base, se alterados);
  // descrição quebrada; dono e posição.
  struct Line {
    std::string text;
    float scale;
    SDL_Color color;
    float gap_after;
  };
  std::vector<Line> lines;
  const SDL_Color title = colors::Get("panel.title");
  const SDL_Color label = colors::Get("panel.label");
  const SDL_Color value = colors::Get("panel.value");
  lines.push_back({info.name, kTipScale, title, kTipGap});
  std::string kind = info.CategoryLabel();
  if (info.hierarchy > 0) {
    kind += " - " + i18n::Format("battle.tooltip.hierarchy", {std::to_string(info.hierarchy)});
    if (!info.hierarchy_name.empty()) kind += " (" + info.hierarchy_name + ")";
  }
  lines.push_back({kind, kTipDetailScale, label, kTipGap * 2});
  auto stat_color = [&](int effective, int base) {
    return effective > base ? colors::Get("status.buff")
           : effective < base ? colors::Get("status.nerf")
                              : value;
  };
  std::string atk = i18n::Get("cardface.atk") + " " + std::to_string(attack);
  if (attack != info.attack) {
    atk += " " + i18n::Format("battle.tooltip.base", {std::to_string(info.attack)});
  }
  std::string def = i18n::Get("cardface.def") + " " + std::to_string(defense);
  if (defense != info.defense) {
    def += " " + i18n::Format("battle.tooltip.base", {std::to_string(info.defense)});
  }
  lines.push_back({atk, kTipScale, stat_color(attack, info.attack), kTipGap});
  lines.push_back({def, kTipScale, stat_color(defense, info.defense), kTipGap * 2});
  for (const std::string& part : WrapText(info.description, kTipTextW, kTipDetailScale)) {
    lines.push_back({part, kTipDetailScale, colors::Get("text.secondary"), kTipGap * 0.6f});
  }
  if (!info.description.empty()) lines.back().gap_after = kTipGap * 2;
  std::string owner = i18n::Get(tooltip_side_ == Side::kPlayer ? "battle.tooltip.yourCard"
                                                                 : "battle.tooltip.opponentCard");
  owner += " - " + i18n::Get(card->defense ? "battle.tooltip.inDefense" : "battle.tooltip.inAttack");
  if (tooltip_side_ == Side::kPlayer && !card->defense && card->attacked) {
    owner += " " + i18n::Get("battle.tooltip.attacked");
  }
  lines.push_back({owner, kTipDetailScale, label, 0.0f});

  // Tamanho: imagem à esquerda (kTipImageH de altura), coluna de texto à direita.
  float text_w = 0.0f;
  float text_h = 0.0f;
  for (const Line& line : lines) {
    text_w = std::max(text_w, text::Width(line.text.c_str(), line.scale));
    text_h += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * line.scale + line.gap_after;
  }
  const float image_w = kTipImageH * kCardAspect;
  const float w = ui::SnapUp(3 * kTipPadding + image_w + std::max(text_w, kTipTextW * 0.6f));
  const float h = ui::SnapUp(2 * kTipPadding + std::max(kTipImageH, text_h));
  // Perto do cursor, sem sair da tela.
  float x = mouse_.x + kTipOffset;
  float y = mouse_.y + kTipOffset;
  if (x + w > width) x = mouse_.x - kTipOffset - w;
  if (y + h > height) y = std::max(0.0f, mouse_.y - kTipOffset - h);
  const SDL_FRect panel{x, y, w, h};
  SDL_Color c;
  if (frame::Ready()) {
    frame::Draw(r, panel, frame::Kind::kWindow);
  } else {
    c = colors::Get("panel.fill");
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &panel);
    c = colors::Get("panel.border");
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderRect(r, &panel);
  }

  // Imagem: frente composta da carta (mesma do HUD), em cache por código.
  const SDL_FRect image{x + kTipPadding, y + kTipPadding, image_w, kTipImageH};
  SDL_Texture* face = const_cast<BattleScene*>(this)->HudFace(info);
  if (face != nullptr) {
    SDL_RenderTexture(r, face, nullptr, &image);
  } else {
    c = colors::Get("border.subtle");
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &image);
  }
  c = colors::Get("panel.border");
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderRect(r, &image);

  const float tx = image.x + image.w + kTipPadding;
  float ty = y + kTipPadding;
  for (const Line& line : lines) {
    text::Draw(r, line.text.c_str(), tx, ty, line.scale, line.color);
    ty += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * line.scale + line.gap_after;
  }
}

void BattleScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color fg = colors::Get("text.primary");
  const SDL_Color background = colors::Get("background.fog");
  const float cx = ctx.logical_width / 2.0f;

  if (ready_) {
    // Passes de sombra (um por tocha): arena, corpos das tochas e cartas em campo são os
    // ocultadores.
    for (int k = 0; k < 2; ++k) {
      if (!gpu_->BeginShadowPass(k, torch_light_vp_[k])) break;
      if (arena_model_ != nullptr) arena_model_->DrawDepth(*gpu_, arena_world_);
      if (torch_ != nullptr) {
        for (const math::Mat4& world : torch_world_) torch_->DrawBodyDepth(*gpu_, world);
      }
      board_.RenderDepth(*gpu_);
      enemy_board_.RenderDepth(*gpu_);
      gpu_->EndShadowPass();
    }
  }
  if (ready_ && gpu_->BeginFrame(fog_color_)) {
    gpu_->SetAtmosphere(arena_atmosphere_);
    if (arena_model_ != nullptr) {
      arena_model_->Draw(*gpu_, arena_world_, arena_view_proj_, SDL_FColor{1, 1, 1, 1});
    }
    if (torch_ != nullptr) {
      for (const math::Mat4& world : torch_world_) {
        torch_->DrawBody(*gpu_, world, arena_view_proj_);
      }
    }
    // Cartas dos dois lados do campo e destaques dos slots.
    board_.Render(*gpu_, arena_view_proj_);
    enemy_board_.Render(*gpu_, arena_view_proj_);
    sword_.Render(*gpu_, arena_view_proj_);
    shield_.Render(*gpu_, arena_view_proj_);
    // Névoa depois da arena (transparente): de baixo para cima.
    for (const MistLayer& layer : mist_layers_) {
      gpu_->Draw(mist_quad_, nullptr, math::Mat4::Translate({0, layer.y, 0}), arena_view_proj_,
                 SDL_FColor{mist_color_.r, mist_color_.g, mist_color_.b, layer.alpha},
                 Gpu::DrawMode::kMist);
    }
    // Abertura: névoa densa acima da base, que se dissipa conforme a arena emerge.
    if (intro_ < 1.0f) {
      const float remain = 1.0f - intro_;
      Atmosphere dense = arena_atmosphere_;
      // Manto fechado no início, fiapos no fim (Fog.json, battle.introMist).
      dense.mist_density = config::Float("fog", "battle.introMist.density", 0.3f) * remain;
      dense.mist_fade_start = config::Float("fog", "battle.introMist.fadeStart", 10.0f);
      dense.mist_fade_end = config::Float("fog", "battle.introMist.fadeEnd", 30.0f);
      gpu_->SetAtmosphere(dense);
      for (const MistLayer& layer : intro_mist_layers_) {
        gpu_->Draw(mist_quad_, nullptr, math::Mat4::Translate({0, layer.y, 0}),
                   arena_view_proj_,
                   SDL_FColor{mist_color_.r, mist_color_.g, mist_color_.b,
                              layer.alpha * std::sqrt(remain)},
                   Gpu::DrawMode::kMist);
      }
    }
    // Chamas por cima da névoa (transparentes, escrevem profundidade).
    if (torch_ != nullptr) {
      gpu_->SetAtmosphere(arena_atmosphere_);
      for (const math::Mat4& world : torch_world_) {
        torch_->DrawFlame(*gpu_, world, arena_view_proj_, arena_atmosphere_.camera_pos,
                          ignite_);
      }
    }
    gpu_->NewLayer();  // a mão fica sempre por cima da arena
    gpu_->SetAtmosphere(hand_atmosphere_);
    hand_.Render(*gpu_, hand_view_proj_);
    enemy_hand_view_.Render(*gpu_, hand_view_proj_);
    RenderReveal3D(ctx.logical_width, ctx.logical_height);
    gpu_->EndFrame();

    SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
    SDL_RenderClear(r);
    SDL_RenderTexture(r, gpu_->Target(), nullptr, nullptr);
  } else {
    SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
    SDL_RenderClear(r);
    text::DrawCentered(r, i18n::Get("battle.noRender").c_str(), cx, ctx.logical_height * 0.5f,
                       2.0f * ui::kScale, fg);
  }

  // Topo: situação do turno e, abaixo, a dica do que fazer.
  if (started_ && !hud_.Over()) {
    const std::string status = i18n::Format(
        "battle.status",
        {std::to_string(turn_),
         i18n::Get(turn_side_ == Side::kPlayer && player_turn_ ? "battle.status.yourTurn"
                                                               : "battle.status.opponent"),
         i18n::Get(ai::DifficultyKey(difficulty_)), std::to_string(enemy_hand_.size())});
    text::DrawCentered(r, status.c_str(), cx, kStatusY, kStatusScale,
                       colors::Get("text.secondary"));
  }
  // Arrasto de ataque: arco curvo com seta da carta atacante até o mouse.
  if (attack_from_ >= 0 && attack_drag_ && mouse_.x >= 0) {
    const math::Vec2 from =
        ScreenOf(board_.CardPosition(attack_from_), ctx.logical_width, ctx.logical_height);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    DrawAttackArc(r, SDL_FPoint{from.x, from.y}, SDL_FPoint{mouse_.x, mouse_.y},
                  colors::Get("board.slotHover"),
                  static_cast<float>(SDL_GetTicks()) / 1000.0f);
  }
  // Dicas do seu turno.
  const char* hint = nullptr;  // chave em translate.json
  if (attack_from_ >= 0 && attack_drag_) {
    hint = enemy_board_.Count() == 0 ? "battle.hint.directAttack" : "battle.hint.attackCard";
  } else if (CanSummon()) {
    hint = CandidateIsField() ? "battle.hint.playField" : "battle.hint.summon";
  } else if (CanAct() && AttacksAllowed(Side::kPlayer)) {
    for (int slot : board_.Occupied()) {
      const Board::Card* card = board_.At(slot);
      if (!card->defense && !card->attacked) {
        hint = "battle.hint.attack";
        break;
      }
    }
  }
  if (hint != nullptr) {
    text::DrawCentered(r, i18n::Get(hint).c_str(), cx, kHintY, kHintScale,
                       colors::Get("board.hint"));
  }
  hud_.Render(r, ctx.logical_width, ctx.logical_height);
  if (CanAct()) end_turn_->Render(r);
  sound_button_.Render(r, ctx.music != nullptr && ctx.music->Muted());
  RenderReveal(r, ctx.logical_width, ctx.logical_height);
  if (!paused_ && reveal_.card == nullptr) RenderTooltip(r, ctx.logical_width, ctx.logical_height);
  abandon_dialog_.Render(r, ctx.logical_width, ctx.logical_height);
}
