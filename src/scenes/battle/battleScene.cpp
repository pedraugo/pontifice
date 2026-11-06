#include "scenes/battle/battleScene.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

#include "scripts/cardface/cardFace.h"
#include "scripts/colors/colors.h"
#include "scripts/config/config.h"
#include "scripts/frame/frame.h"
#include "scripts/i18n/i18n.h"
#include "scripts/image/image.h"
#include "scripts/audios/music.h"
#include "scripts/text/text.h"
#include "scripts/ui/ui.h"
#include "scripts/log/log.h"

namespace {

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

constexpr float kArenaScale = 4.2f;
constexpr float kArenaBaseY = -0.061f * kArenaScale;
constexpr float kArenaTopY = 0.3f * kArenaScale;

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

constexpr float kTorchX = 6.4f;
constexpr float kTorchZ = -1.0f;
constexpr float kTorchScale = 2.6f;
constexpr float kTorchTilt = 0.10f;

constexpr float kIntroSeconds = 1.6f;
constexpr float kIntroDrop = 3.2f;
constexpr float kTorchIntroStart = 1.1f;
constexpr float kTorchIntroSeconds = 1.2f;
constexpr float kTorchIntroDrop = 4.5f;
constexpr float kIgniteSeconds = 0.6f;
constexpr float kMatchStart = 3.2f;
constexpr math::Vec3 kIntroEyeStart{0.0f, 16.0f, 22.0f};
constexpr math::Vec3 kArenaTarget{0.0f, -0.1f, -0.3f};

constexpr float kOrbitSeconds = 1.0f;

constexpr float kButtonWidth = ui::Grid(21);
constexpr float kButtonHeight = ui::Grid(5);
constexpr float kButtonMargin = ui::Grid(1);
constexpr float kEndTurnTop = ui::Grid(6);
constexpr float kHintScale = 1.5f * ui::kScale;
constexpr float kSoundButtonSize = ui::Grid(3);
constexpr float kTipScale = 2.0f * ui::kScale;
constexpr float kTipDetailScale = 1.5f * ui::kScale;
constexpr float kTipPadding = ui::Grid(1);
constexpr float kTipGap = ui::Grid(0.5f);
constexpr float kTipOffset = ui::Grid(1);
constexpr float kTipImageH = ui::Grid(14);
constexpr float kTipTextW = ui::Grid(24);
constexpr float kStatusY = ui::Grid(2);
constexpr float kHintY = ui::Grid(4);
constexpr float kCardAspect = 1600.0f / 2230.0f;

constexpr float kStatusScale = 2.0f * ui::kScale;
constexpr float kClickSlop = 6.0f;
constexpr float kAttackDragPx = 24.0f;
constexpr int kHandLimit = 6;
constexpr float kOpponentPause = 0.7f;
constexpr float kFieldGlowSpeed = 6.0f;
constexpr float kFieldArenaBoost = 1.5f;

SDL_FColor LerpColor(SDL_FColor a, SDL_FColor b, float t) {
  return SDL_FColor{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
                    a.a + (b.a - a.a) * t};
}
constexpr float kRevealSeconds = 2.0f;
constexpr float kRevealTravel = 0.5f;
constexpr float kHandHideSeconds = 0.5f;
constexpr float kRevealFadeIn = 0.2f;
constexpr float kRevealFadeOut = 0.25f;
constexpr float kRevealCardH = ui::Grid(35);
constexpr float kRevealCaptionScale = 3.0f * ui::kScale;
constexpr float kRevealDetailScale = 2.5f * ui::kScale;
constexpr float kAttackHit = 0.25f;

// Card stat (attack or defense) to effect tier 1 (weakest) to 5 (strongest).
int SfxTier(int value) {
  if (value < 300) return 1;
  if (value < 600) return 2;
  if (value < 1000) return 3;
  if (value < 1800) return 4;
  return 5;
}
constexpr float kShakeSeconds = 0.45f;
constexpr float kShakeAmplitude = 0.16f;

Image CardNormalMap(Image image, int downscale) {
  if (!image.Valid()) return image;
  // Transparent corners become a flat normal with full alpha.
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

void DrawAttackArc(SDL_Renderer* r, SDL_FPoint from, SDL_FPoint to, SDL_Color color, float time) {
  const float dx = to.x - from.x, dy = to.y - from.y;
  const float dist = SDL_sqrtf(dx * dx + dy * dy);
  if (dist < 8.0f) return;
  float nx = -dy / dist, ny = dx / dist;
  // The arc always bends upward on screen.
  if (ny > 0.0f || (ny == 0.0f && nx < 0.0f)) {
    nx = -nx;
    ny = -ny;
  }
  const float bend = SDL_clamp(dist * 0.35f, 40.0f, 320.0f);
  const SDL_FPoint ctrl{(from.x + to.x) * 0.5f + nx * bend, (from.y + to.y) * 0.5f + ny * bend};
  constexpr int kSegments = 48;
  constexpr float kHeadLength = 34.0f;
  constexpr float kHeadWidth = 34.0f;
  auto at = [&](float t) {
    const float u = 1.0f - t;
    return SDL_FPoint{u * u * from.x + 2 * u * t * ctrl.x + t * t * to.x,
                      u * u * from.y + 2 * u * t * ctrl.y + t * t * to.y};
  };
  float ex = to.x - ctrl.x, ey = to.y - ctrl.y;
  const float elen = SDL_sqrtf(ex * ex + ey * ey);
  ex /= SDL_max(elen, 1e-4f);
  ey /= SDL_max(elen, 1e-4f);
  const SDL_FPoint base{to.x - ex * kHeadLength, to.y - ey * kHeadLength};
  std::vector<SDL_FPoint> pts;
  std::vector<float> core_w, glow_w, outline_w;
  std::vector<SDL_FColor> core_c, glow_c, outline_c;
  // The curve stops before the arrowhead.
  const float head_t = SDL_max(0.0f, 1.0f - kHeadLength / SDL_max(dist * 1.15f, 1.0f));
  const SDL_FColor c{color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, 1.0f};
  for (int i = 0; i <= kSegments; ++i) {
    const float s = static_cast<float>(i) / kSegments;
    pts.push_back(i == kSegments ? base : at(s * head_t));
    const float w = 4.0f + 6.0f * s;
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
  auto head = [&](float grow, SDL_FColor fill) {
    const float px = -ey, py = ex;
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

}

BattleScene::~BattleScene() {
  if (field_front_2d_ != nullptr) SDL_DestroyTexture(field_front_2d_);
  for (auto& [code, texture] : hud_faces_) SDL_DestroyTexture(texture);
  for (auto& [code, texture] : hud_arts_) SDL_DestroyTexture(texture);
  if (gpu_ == nullptr) return;
  hand_.Clear();
  opponent_hand_view_.Clear();
  board_.Destroy(*gpu_);
  opponent_board_.Destroy(*gpu_);
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
  hud_field_ = Image::LoadFile(base + "sprites/cardFrontField.webp").Downscaled(4);
  if (info.gpu_device == nullptr) return;

  gpu_ = std::make_unique<Gpu>();
  if (!gpu_->InitDevice(info.gpu_device, info.logical_width, info.logical_height,
                        base + "shaders")) {
    gpu_.reset();
    return;
  }
  card_model_ = std::make_unique<Model>();
  if (!card_model_->Upload(*gpu_, Model::Parse(base + "models/card.glb"))) {
    LOG("Failed to load models/card.glb");
    card_model_.reset();
    return;
  }
  card_aura_.Init(*gpu_, *card_model_);
  veil_quad_ = gpu_->CreateMesh({{-0.5f, -0.5f, 0, 0, 1}, {0.5f, -0.5f, 0, 1, 1},
                                 {0.5f, 0.5f, 0, 1, 0}, {-0.5f, 0.5f, 0, 0, 0}},
                                {0, 1, 2, 0, 2, 3});
  back_ = UploadSprite(Image::LoadFile(base + "sprites/cardBack.webp"));
  front_normal_ = UploadSprite(
      CardNormalMap(Image::LoadFile(base + "sprites/cardFrontNormalMap.webp"), 2));
  back_normal_ = UploadSprite(Image::LoadFile(base + "sprites/cardBackNormalMap.webp"));
  for (Board* board : {&board_, &opponent_board_}) {
    board->GetLayout().arena_scale = kArenaScale;
    board->SetModel(card_model_.get());
    board->SetAura(&card_aura_);
    board->SetBack(back_);
    board->SetNormalMaps(front_normal_, back_normal_);
    board->Init(*gpu_);
  }
  opponent_board_.SetSide(-1.0f);
  sword_.Init(*gpu_);
  shield_.Init(*gpu_);
  for (int tier = 1; tier <= 5; ++tier) {
    const std::string n = std::to_string(tier);
    sfx_.Load("sword" + n, base + "audios/sfx/sword" + n + ".ogg");
    sfx_.Load("shield" + n, base + "audios/sfx/shield" + n + ".ogg");
  }
  sfx_.Load("open", base + "audios/sfx/openBattle.ogg");
  frames_[0] = Image::LoadFile(base + "sprites/cardFrontGood.webp").Downscaled(2);
  frames_[1] = Image::LoadFile(base + "sprites/cardFrontEvil.webp").Downscaled(2);
  frames_[2] = Image::LoadFile(base + "sprites/cardFrontNeutral.webp").Downscaled(2);
  frames_[3] = Image::LoadFile(base + "sprites/cardFrontField.webp").Downscaled(2);
  hidden_face_ = UploadSprite(cardface::ComposeHidden(
      Image::LoadFile(base + "sprites/cardFrontOpponent.webp").Downscaled(2)));

  cards_ = cards::LoadAll(base);
  arts_.clear();
  // Card art decoded in parallel: the slowest part of loading.
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
    LOG("Card illustrations: %zu (%.0f ms, %u threads)", arts_.size(),
            static_cast<double>(SDL_GetTicksNS() - started) / 1e6, workers);
  }
  // Debug: PONTIFICE_SEED fixes the deck shuffle.
  if (const char* env = SDL_getenv("PONTIFICE_SEED")) {
    rng_.seed(static_cast<unsigned>(SDL_atoi(env)));
  } else {
    rng_.seed(std::random_device{}());
  }
  deck_ = deck::Deck(cards::BuildDeck(cards_, Hud::kDeckSize, rng_));
  deck_.Shuffle();
  int level = info.difficulty;
  if (const char* env = SDL_getenv("PONTIFICE_DIFFICULTY")) {
    if (std::strcmp(env, "easy") == 0) level = 0;
    if (std::strcmp(env, "medium") == 0) level = 1;
    if (std::strcmp(env, "hard") == 0) level = 2;
    if (std::strcmp(env, "impossible") == 0) level = 3;
  }
  difficulty_ = static_cast<ai::Difficulty>(std::clamp(level, 0, 3));
  opponent_deck_ = deck::Deck(cards::BuildDeck(cards_, Hud::kDeckSize, rng_,
                                               [this](const CardInfo& card) {
                                                 return ai::DeckWeight(card, difficulty_);
                                               }));
  opponent_deck_.Shuffle();
  LOG("Opponent level: %s", ai::DifficultyKey(difficulty_));
  // Debug: PONTIFICE_HAND=N301,E101 forces the starting hand.
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
        deck_.Draw();  // still draws, to keep the deck count right
      }
    } else {
      code = deck_.Draw();
    }
    if (!code) break;
    const Face face = MakeCardFace(*code);
    if (face.texture != nullptr) initial_hand_.push_back(face);
  }
  if (const char* env = SDL_getenv("PONTIFICE_OPPONENT_HAND")) {
    std::string spec = env;
    size_t pos = 0;
    while (pos < spec.size() && opponent_hand_.size() < 5) {
      size_t end = spec.find(',', pos);
      if (end == std::string::npos) end = spec.size();
      if (const CardInfo* info = cards::Find(cards_, spec.substr(pos, end - pos))) {
        opponent_hand_.push_back(info);
        opponent_deck_.Draw();
      }
      pos = end + 1;
    }
  }
  while (opponent_hand_.size() < 5 && opponent_deck_.Size() > 0) OpponentDraw();
  arena_model_ = std::make_unique<Model>();
  if (!arena_model_->Upload(*gpu_, Model::Parse(base + "models/arena.glb"))) {
    LOG("arena.glb not loaded; generate it with tools/textureArena.py");
    arena_model_.reset();
  }
  torch_ = std::make_unique<Torch>();
  if (!torch_->Upload(*gpu_, Model::Parse(base + "models/torch.glb"))) {
    LOG("torch.glb not loaded; generate it with tools/makeTorch.py");
    torch_.reset();
  }
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
  return {UploadSprite(cardface::Compose(frame, *card, ArtFor(*card))), card->hierarchy == 1,
          card};
}

SDL_Texture* BattleScene::HudArt(const CardInfo& card, bool large) {
  if (renderer_ == nullptr) return nullptr;
  const std::string key = large ? card.code + "#large" : card.code;
  if (const auto it = hud_arts_.find(key); it != hud_arts_.end()) return it->second;
  const Image* art = ArtFor(card);
  if (art == nullptr) return nullptr;
  SDL_Texture* texture = UploadHudSprite(renderer_, large ? *art : art->Downscaled(4));
  if (texture != nullptr) hud_arts_[key] = texture;
  return texture;
}

const Image* BattleScene::ArtFor(const CardInfo& card) const {
  const auto it = arts_.find(card.code);
  return it != arts_.end() ? &it->second : nullptr;
}

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

// Without a renderer or frame, the field front is the fallback.
SDL_Texture* BattleScene::HudFace(const CardInfo& card) {
  if (renderer_ == nullptr) return field_front_2d_;
  auto it = hud_faces_.find(card.code);
  if (it != hud_faces_.end()) return it->second;
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

void BattleScene::OpponentHandAdd(const CardInfo* info) {
  opponent_hand_.push_back(info);
  if (hidden_face_ != nullptr) opponent_hand_view_.Add(hidden_face_, false, nullptr);
}

void BattleScene::OpponentHandRemove(size_t index, math::Mat4* world) {
  if (index >= opponent_hand_.size()) return;
  opponent_hand_.erase(opponent_hand_.begin() + static_cast<long>(index));
  if (index < opponent_hand_view_.Size()) {
    const Hand::Card removed = opponent_hand_view_.Remove(index);
    if (world != nullptr) *world = removed.world;
  }
}

void BattleScene::OpponentDraw() {
  const std::optional<std::string> code = opponent_deck_.Draw();
  if (!code) return;
  if (const CardInfo* info = cards::Find(cards_, *code)) OpponentHandAdd(info);
}

void BattleScene::Schedule(float delay, std::function<void()> fn) {
  steps_.emplace_back(delay, std::move(fn));
}

void BattleScene::ScheduleNext(float delay, std::function<void()> fn) {
  steps_.emplace_front(delay, std::move(fn));
}
void BattleScene::PlaySfxLater(float delay, std::string name) {
  if (delay <= 0.0f) {
    sfx_.Play(name);
    return;
  }
  sfx_queue_.emplace_back(delay, std::move(name));
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
  if (from_world != nullptr && ready_ && hidden_face_ != nullptr) {
    const math::Mat4& m = *from_world;
    // Position, scale and Z roll extracted from the hand card matrix.
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
  if (reveal_.front3d == nullptr) reveal_.face = HudFace(card);
}

bool BattleScene::Busy() const {
  return !steps_.empty() || board_.Animating() || opponent_board_.Animating() || sword_.Active() ||
         shield_.Active() ||
         std::fabs(orbit_ - orbit_target_) > 0.001f;
}

bool BattleScene::AttacksAllowed(Side side) const {
  // Whoever starts cannot attack on the first turn.
  return !(starter_ == side && turns_played_[Index(side)] == 0);
}

void BattleScene::StartMatch() {
  started_ = true;
  LOG("Coin toss: %s", starter_ == Side::kPlayer ? "you start" : "the opponent starts");
  if (starter_ == Side::kPlayer) {
    BeginPlayerTurn();
  } else {
    hand_.SetAway(true);
    opponent_hand_view_.SetAway(false);
    const bool turned = orbit_ >= 1.0f;
    orbit_target_ = 1.0f;
    Schedule(turned ? 0.2f : kOrbitSeconds + 0.2f, [this] { RunOpponentTurn(); });
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
  opponent_hand_view_.SetAway(true);
  orbit_target_ = 0.0f;
  // Whoever starts does not draw on the first turn.
  if (!(starter_ == Side::kPlayer && turns_played_[0] == 0)) DrawCard();
  LOG("Turn %d: your turn", turn_);
}

void BattleScene::EndTurn() {
  if (!player_turn_ || Busy() || hud_.Over()) return;
  player_turn_ = false;
  ++turns_played_[0];
  hand_.CancelDrag();
  attack_from_ = -1;
  attack_drag_ = false;
  DiscardExtraCards();
  hand_.SetAway(true);
  opponent_hand_view_.SetAway(false);
  orbit_target_ = 1.0f;
  Schedule(kOrbitSeconds + 0.2f, [this] { RunOpponentTurn(); });
}

void BattleScene::DiscardExtraCards() {
  while (hand_.Size() > static_cast<size_t>(kHandLimit)) {
    size_t worst = 0;
    int worst_value = 1 << 30;
    for (size_t i = 0; i < hand_.Size(); ++i) {
      const CardInfo* info = hand_.CardAt(i).info;
      // Field cards count as 0: discarded first.
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

void BattleScene::RunOpponentTurn() {
  if (hud_.Over()) return;
  turn_side_ = Side::kOpponent;
  opponent_board_.ResetAttacks();
  if (!(starter_ == Side::kOpponent && turns_played_[1] == 0)) OpponentDraw();  // same rule
  LOG("Turn %d: opponent (%zu in hand, %zu in deck)", turn_, opponent_hand_.size(),
          opponent_deck_.Size());
  Schedule(kOpponentPause, [this] {
    for (int slot : ai::ChoosePositionChanges(BuildOpponentView())) {
      opponent_board_.TogglePosition(slot);
    }
  });
  Schedule(kOpponentPause, [this] {
    const int index = ai::ChooseFieldCard(BuildOpponentView());
    if (index < 0) return;
    const CardInfo* card = opponent_hand_[static_cast<size_t>(index)];
    math::Mat4 from;
    const bool had_view = static_cast<size_t>(index) < opponent_hand_view_.Size();
    OpponentHandRemove(static_cast<size_t>(index), &from);
    StartReveal(*card, i18n::Get("battle.reveal.field"), "", had_view ? &from : nullptr);
    ScheduleNext(RevealDuration(), [this, card] {
      reveal_ = Reveal{};
      PlayField(Side::kOpponent, *card);
    });
  });
  Schedule(kOpponentPause, [this] {
    const std::optional<ai::Summon> summon = ai::ChooseSummon(BuildOpponentView());
    if (!summon || summon->hand_index < 0 ||
        summon->hand_index >= static_cast<int>(opponent_hand_.size())) {
      return;
    }
    const CardInfo* card = opponent_hand_[static_cast<size_t>(summon->hand_index)];
    const bool defense = summon->defense;
    math::Mat4 from;
    const bool had_view = static_cast<size_t>(summon->hand_index) < opponent_hand_view_.Size();
    OpponentHandRemove(static_cast<size_t>(summon->hand_index), &from);
    StartReveal(*card, i18n::Get("battle.reveal.summon"),
                i18n::Get(defense ? "battle.reveal.defense" : "battle.reveal.attack"),
                had_view ? &from : nullptr);
    ScheduleNext(RevealDuration(), [this, card, defense] {
      const Face face{reveal_.front3d, reveal_.foil, card};
      reveal_ = Reveal{};
      OpponentSummon(card, defense, face.texture != nullptr ? &face : nullptr);
    });
  });
  Schedule(kOpponentPause + 0.3f, [this] { OpponentAttackStep(); });
}

void BattleScene::OpponentAttackStep() {
  if (hud_.Over()) return;
  const std::optional<ai::Attack> attack = ai::ChooseAttack(BuildOpponentView());
  if (!attack) {
    Schedule(kOpponentPause, [this] { FinishOpponentTurn(); });
    return;
  }
  PerformAttack(Side::kOpponent, attack->slot, attack->target_slot);
  Schedule(0.9f, [this] { OpponentAttackStep(); });
}

void BattleScene::FinishOpponentTurn() {
  ++turns_played_[1];
  while (opponent_hand_.size() > static_cast<size_t>(kHandLimit)) {
    auto worst = std::min_element(opponent_hand_.begin(), opponent_hand_.end(),
                                  [](const CardInfo* a, const CardInfo* b) {
                                    return a->attack < b->attack;
                                  });
    OpponentHandRemove(static_cast<size_t>(worst - opponent_hand_.begin()));
  }
  ++turn_;
  opponent_hand_view_.SetAway(true);
  // The opponent hand retracts before the camera orbits back.
  ScheduleNext(kHandHideSeconds, [this] {
    orbit_target_ = 0.0f;
    ScheduleNext(kOrbitSeconds + 0.2f, [this] { BeginPlayerTurn(); });
  });
}

ai::View BattleScene::BuildOpponentView() const {
  ai::View view;
  view.hand = opponent_hand_;
  for (int slot : opponent_board_.Occupied()) {
    const Board::Card* c = opponent_board_.At(slot);
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
  view.can_summon = true;
  view.can_attack = AttacksAllowed(Side::kOpponent);
  view.free_slots = static_cast<int>(opponent_board_.Free().size());
  return view;
}

void BattleScene::OpponentSummon(const CardInfo* info, bool defense, const Face* ready_face) {
  if (info == nullptr) return;
  std::vector<int> free = opponent_board_.Free();
  int slot;
  if (free.empty()) {
    slot = -1;
    int worst = 1 << 30;
    for (int s : opponent_board_.Occupied()) {
      const Board::Card* c = opponent_board_.At(s);
      const int value = c->info != nullptr ? c->info->attack : 0;
      if (value < worst) {
        worst = value;
        slot = s;
      }
    }
    if (slot < 0) return;
    opponent_board_.Discard(slot);
  } else {
    slot = ai::ChooseSlot(free, Board::kCols);
  }
  const Face face = ready_face != nullptr ? *ready_face : MakeCardFace(info->code);
  if (face.texture == nullptr) return;
  opponent_board_.Place(slot, face.texture, face.foil, defense, info, turn_);
  char name[3];
  LOG("Opponent summoned %s at %s (%s)", info->name.c_str(), SlotName(slot, name),
          defense ? "defense" : "attack");
}

bool BattleScene::CanAct() const {
  return ready_ && started_ && player_turn_ && !hud_.Over() && !hand_.Inspecting() && !Busy();
}

bool BattleScene::CanSummon() const {
  if (!CanAct() || hand_.Dragging() < 0) return false;
  return DraggedIsField() || !summoned_;
}

const char* BattleScene::HoverRole() const {
  return field_.active || DraggedIsField() ? "board.slotHoverField" : "board.slotHover";
}

bool BattleScene::DraggedIsField() const {
  const int index = hand_.Dragging();
  if (index < 0) return false;
  const CardInfo* info = hand_.CardAt(static_cast<size_t>(index)).info;
  return info != nullptr && info->IsField();
}

void BattleScene::PlayField(Side owner, const CardInfo& card) {
  if (owner == Side::kPlayer) hud_.SetGraveyard(hud_.Graveyard() + 1);
  field_ = rules::Field::From(card);
  field_owner_ = owner;
  FieldCardInfo info;
  info.name = card.name;
  info.favored = field_.favored == 'E' ? Category::kEvil : Category::kGood;
  info.buff = card.buff;
  info.nerf = card.nerf;
  info.image = HudFace(card);
  info.art = HudArt(card);
  info.art_large = HudArt(card, true);
  hud_.SetFieldCard(info);
  LOG("%s activated the field card %s", owner == Side::kPlayer ? "You" : "Opponent",
          card.name.c_str());
}

void BattleScene::Summon(int slot) {
  const int index = hand_.Dragging();
  if (index < 0 || slot < 0 || slot >= Board::kSlots) return;
  const Hand::Card card = hand_.Remove(static_cast<size_t>(index));
  if (card.info != nullptr && card.info->IsField()) {
    PlayField(Side::kPlayer, *card.info);
    return;
  }
  if (!board_.Empty(slot) && board_.Discard(slot)) hud_.SetGraveyard(hud_.Graveyard() + 1);
  if (!board_.Place(slot, card.front, card.foil, false, card.info, turn_)) return;
  summoned_ = true;
  board_.SetHighlight(false);
  board_.SetHoverSlot(-1);
  char name[3];
  LOG("Turn %d: card summoned at %s", turn_, SlotName(slot, name));
}

void BattleScene::PerformAttack(Side attacker, int slot, int target_slot) {
  Board& mine = BoardOf(attacker);
  Board& theirs = BoardOf(Other(attacker));
  Board::Card* card = mine.At(slot);
  if (card == nullptr || card->attacked || card->defense) return;
  if (target_slot >= 0 && theirs.Empty(target_slot)) return;
  if (target_slot < 0 && theirs.Count() > 0) return;
  card->attacked = true;
  // Direct attack aims at the center slot of row B.
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
  const math::Vec3 hit = target_slot >= 0
                             ? theirs.CardPosition(target_slot)
                             : theirs.SlotPosition(Board::kCols / 2 + Board::kCols);
  sword_.Play(hit, mine.CardPosition(slot));
  sfx_.Play("sword" + std::to_string(SfxTier(attack)));
  if (target_slot >= 0) theirs.Flash(target_slot);
  if (target_slot < 0) {
    outcome = rules::ResolveDirect(attack);
    LOG("%s: direct attack by %s (%d)", attacker == Side::kPlayer ? "You" : "Opponent",
            card->info->name.c_str(), attack);
  } else {
    Board::Card* target = theirs.At(target_slot);
    if (target == nullptr || target->info == nullptr) return;
    outcome = rules::Resolve(attack, target->defense,
                             rules::EffectiveAttack(*target->info, field_),
                             rules::EffectiveDefense(*target->info, field_));
    if (target->defense) {
      shield_.Play(hit, mine.CardPosition(slot), sword_.HitDelay(), outcome.defender_destroyed);
      const int defense = rules::EffectiveDefense(*target->info, field_);
      PlaySfxLater(sword_.HitDelay(), "shield" + std::to_string(SfxTier(defense)));
    }
    LOG("%s: %s (%s, %d) attacks %s (%s, %s)", attacker == Side::kPlayer ? "You" : "Opponent",
            card->info->name.c_str(), SlotName(slot, a_name), attack,
            target->info->name.c_str(), SlotName(target_slot, d_name),
            target->defense ? "defense" : "attack");
  }
  if (outcome.defender_destroyed) Destroy(Other(attacker), target_slot);
  if (outcome.attacker_destroyed) Destroy(attacker, slot);
  if (outcome.defender_damage > 0) ApplyDamage(Other(attacker), outcome.defender_damage);
  if (outcome.attacker_damage > 0) ApplyDamage(attacker, outcome.attacker_damage);
  if (hud_.Over()) {
    steps_.clear();
    reveal_ = Reveal{};
    player_turn_ = false;
    hand_.SetAway(true);
    LOG("End: %s", hud_.PlayerLife() > 0 ? "victory" : "defeat");
  }
}

void BattleScene::Destroy(Side owner, int slot) {
  if (!BoardOf(owner).Discard(slot)) return;
  if (owner == Side::kPlayer) hud_.SetGraveyard(hud_.Graveyard() + 1);
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

void BattleScene::OnEnter(SceneContext& ctx) {
  if (!preloaded_) Preload(ctx.Preload());
  renderer_ = ctx.renderer;

  end_turn_ = std::make_unique<Button>(i18n::Get("battle.endTurn"),
                                       [this](SceneContext&) { EndTurn(); });
  SDL_FRect rect{(ctx.logical_width - kButtonWidth) / 2.0f, kEndTurnTop, kButtonWidth,
                 kButtonHeight};
  end_turn_->SetRect(rect);
  sound_button_.SetRect(SDL_FRect{kButtonMargin, ctx.logical_height - kButtonMargin - kSoundButtonSize,
                                  kSoundButtonSize, kSoundButtonSize});
  hint_button_.SetRect(SDL_FRect{kButtonMargin, sound_button_.Rect().y - kButtonMargin - kSoundButtonSize,
                                 kSoundButtonSize, kSoundButtonSize});
  started_ = false;
  player_turn_ = false;
  summoned_ = false;
  turn_ = 1;
  turns_played_[0] = turns_played_[1] = 0;
  steps_.clear();
  reveal_ = Reveal{};
  field_ = rules::Field{};

  hud_.SetLife(Hud::kMaxLife, Hud::kMaxLife);
  hud_.ClearFieldCard();
  hud_.SetGraveyard(0);
  fog_color_ = config::Color("fog", "battle.fog.color", colors::GetF("background.fog"));
  mist_color_ = config::Color("fog", "battle.mist.color", colors::GetF("background.mist"));
  if (field_front_2d_ == nullptr) {
    field_front_2d_ = UploadHudSprite(ctx.renderer, hud_field_);
    hud_field_ = Image{};  // free the image: already a texture
  }

  if (gpu_ == nullptr || card_model_ == nullptr) {
    LOG("Failed to start 3D rendering");
    return;
  }
  gpu_->SetRenderer(ctx.renderer);

  hand_.SetModel(card_model_.get());
  hand_.SetAura(&card_aura_);
  hand_.SetBack(back_);
  hand_.SetNormalMaps(front_normal_, back_normal_);
  hand_.DelayDeal(0.8f);
  for (const Face& face : initial_hand_) hand_.Add(face.texture, face.foil, face.info);
  initial_hand_.clear();
  starter_ = std::uniform_int_distribution<int>(0, 1)(rng_) == 0 ? Side::kPlayer : Side::kOpponent;
  if (const char* env = SDL_getenv("PONTIFICE_STARTER")) {
    if (std::strcmp(env, "player") == 0) starter_ = Side::kPlayer;
    if (std::strcmp(env, "opponent") == 0) starter_ = Side::kOpponent;
  }
  if (starter_ == Side::kOpponent) {
    hand_.SetAwayNow(true);
    orbit_ = orbit_target_ = 1.0f;
  } else {
    orbit_ = orbit_target_ = 0.0f;
  }
  opponent_hand_view_.SetModel(card_model_.get());
  opponent_hand_view_.SetBack(back_);
  opponent_hand_view_.SetNormalMaps(front_normal_, back_normal_);
  opponent_hand_view_.GetLayout().hidden_drop = 0.35f;
  opponent_hand_view_.Clear();
  const bool opponent_first = starter_ == Side::kOpponent;
  if (opponent_first) opponent_hand_view_.DelayDeal(0.8f);
  for (size_t i = 0; i < opponent_hand_.size() && hidden_face_ != nullptr; ++i) {
    opponent_hand_view_.Add(hidden_face_, false, nullptr);
  }
  opponent_hand_view_.SetAwayNow(!opponent_first);
  hud_.SetDeck(Hud::kDeckSize - static_cast<int>(deck_.Size()), static_cast<int>(deck_.Size()));
  arena_world_ = math::Mat4::Scale(kArenaScale);

  const float aspect = static_cast<float>(ctx.logical_width) / ctx.logical_height;
  proj_ = math::Mat4::Perspective(math::Radians(35.0f), aspect, 0.1f, 100.0f);
  hand_view_proj_ = proj_ * math::Mat4::LookAt({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
  math::Vec3 eye{0, 10.5f, 12.2f};
  if (const char* env = SDL_getenv("PONTIFICE_ARENA_EYE")) {
    SDL_sscanf(env, "%f,%f,%f", &eye.x, &eye.y, &eye.z);
  }
  eye_final_ = eye;
  arena_view_proj_ = proj_ * math::Mat4::LookAt(eye, kArenaTarget, {0, 1, 0});

  arena_atmosphere_.light_dir = config::Vec3("light", "battle.arena.lightDir", {-0.35f, 1.0f, 0.45f});
  arena_atmosphere_.light_intensity = config::Float("light", "battle.arena.lightIntensity", 0.0f);
  arena_atmosphere_.light_color = Torch::LightColor();
  arena_atmosphere_.ambient = config::Color("light", "battle.arena.ambient", {0.07f, 0.08f, 0.13f, 1.0f});
  arena_atmosphere_.point_intensity = 0.0f;
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

  hand_atmosphere_.light_dir = config::Vec3("light", "battle.hand.lightDir", {0.3f, 0.6f, 1.0f});
  hand_atmosphere_.light_intensity = config::Float("light", "battle.hand.lightIntensity", 0.5f);
  hand_atmosphere_.light_color = arena_atmosphere_.light_color;
  hand_atmosphere_.ambient = config::Color("light", "battle.hand.ambient", {0.34f, 0.36f, 0.46f, 1.0f});
  hand_atmosphere_.point_pos = config::Vec3("light", "battle.hand.pointPos", {0.0f, 2.5f, 4.0f});
  hand_atmosphere_.point_intensity = config::Float("light", "battle.hand.pointIntensity", 1.0f);
  hand_atmosphere_.camera_pos = {0, 0, 10};
  ready_ = true;
  sfx_.SetMuted(ctx.music != nullptr && ctx.music->Muted());
  sfx_.Play("open");
  UpdateIntro(0.0f);
}

void BattleScene::UpdateIntro(float dt_seconds) {
  intro_time_ += dt_seconds;
  if (intro_ >= 1.0f && torch_intro_ >= 1.0f && ignite_ >= 1.0f) return;
  intro_ = SmoothStep(intro_time_ / kIntroSeconds);
  torch_intro_ = SmoothStep((intro_time_ - kTorchIntroStart) / kTorchIntroSeconds);
  ignite_ = SmoothStep((intro_time_ - kTorchIntroStart - kTorchIntroSeconds) / kIgniteSeconds);
  // Debug: PONTIFICE_ARENA_EYE skips the intro.
  if (SDL_getenv("PONTIFICE_ARENA_EYE") != nullptr) intro_ = torch_intro_ = ignite_ = 1.0f;
  const float drop = kIntroDrop * (1.0f - intro_);
  arena_world_ = math::Mat4::Translate({0.0f, -drop, 0.0f}) * math::Mat4::Scale(kArenaScale);
  const float torch_drop = kTorchIntroDrop * (1.0f - torch_intro_);
  torch_world_[0] = math::Mat4::Translate({-kTorchX, kArenaBaseY - torch_drop, kTorchZ}) *
                    math::Mat4::RotateZ(kTorchTilt) * math::Mat4::Scale(kTorchScale);
  torch_world_[1] = math::Mat4::Translate({kTorchX, kArenaBaseY - torch_drop, kTorchZ}) *
                    math::Mat4::RotateZ(-kTorchTilt) * math::Mat4::Scale(kTorchScale);
  arena_atmosphere_.point_pos = Torch::FlamePosition(torch_world_[0]);
  arena_atmosphere_.point2_pos = Torch::FlamePosition(torch_world_[1]);
  for (int k = 0; k < 2; ++k) {
    const math::Vec3 flame = k == 0 ? arena_atmosphere_.point_pos : arena_atmosphere_.point2_pos;
    torch_light_vp_[k] = shadow_proj_ * math::Mat4::LookAt(flame, shadow_target_, {0, 1, 0});
  }
  arena_atmosphere_.point_intensity = torch_light_ * ignite_;
  arena_atmosphere_.point2_intensity = torch_light_ * ignite_;
  math::Vec3 eye = kIntroEyeStart + (eye_final_ - kIntroEyeStart) * intro_;
  if (orbit_ > 0.0f) {  // orbit 180° around Y to the opponent side
    const float angle = math::kPi * SmoothStep(orbit_);
    const float c = std::cos(angle), s = std::sin(angle);
    eye = math::Vec3{eye.x * c + eye.z * s, eye.y, -eye.x * s + eye.z * c};
  }
  arena_view_proj_ = proj_ * math::Mat4::LookAt(eye, kArenaTarget, {0, 1, 0});
  arena_atmosphere_.camera_pos = eye;
}

void BattleScene::UpdateCamera() {
  if (intro_ < 1.0f) return;
  const float angle = math::kPi * SmoothStep(orbit_);  // 180° around Y to the opponent side
  const float c = std::cos(angle), s = std::sin(angle);
  const math::Vec3 e = eye_final_;
  math::Vec3 eye{e.x * c + e.z * s, e.y, -e.x * s + e.z * c};
  math::Vec3 target = kArenaTarget;
  if (shake_left_ > 0.0f) {
    const float k = shake_left_ / kShakeSeconds;
    const float a = kShakeAmplitude * k * k;  // shake decays quadratically
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
  hand_.CancelDrag();
  attack_from_ = -1;
  attack_drag_ = false;
  press_ = {-1.0f, -1.0f};
  if (ctx.music != nullptr) {
    ctx.music->SetScene("menu");
    ctx.music->SetCategory("menu");
  }
  abandon_dialog_.Open(i18n::Get("battle.abandon.question"), i18n::Get("battle.abandon.no"),
                       i18n::Get("battle.abandon.yes"), ctx.logical_width, ctx.logical_height,
                       true);
}

void BattleScene::Resume(SceneContext& ctx) {
  paused_ = false;
  abandon_dialog_.Close();
  if (ctx.music != nullptr) {
    ctx.music->SetScene("battle");
    ctx.music->SetCategory("battle", true);
  }
}

void BattleScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  if (paused_) {
    switch (abandon_dialog_.HandleEvent(event)) {
      case ConfirmDialog::Result::kNo:
        Resume(ctx);
        break;
      case ConfirmDialog::Result::kYes:
        abandon_dialog_.Close();
        ctx.next_scene = SceneId::kMenu;
        break;
      case ConfirmDialog::Result::kNone:
        break;
    }
    return;
  }
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
        if (hand_.Inspecting()) {
          hand_.StopInspect();
          return;
        }
        if (hud_.Over()) {
          ctx.next_scene = SceneId::kMenu;
        } else {
          Pause(ctx);
        }
        return;
      case SDLK_SPACE:
        hand_.Flip();
        return;
      // Debug keys: F field card, 1/2 damage, D draw, G graveyard.
      case SDLK_F:
        ToggleSampleFieldCard();
        return;
      case SDLK_1:
        if (!hud_.Over()) hud_.SetLife(hud_.OpponentLife() - 700, hud_.PlayerLife());
        return;
      case SDLK_2:
        if (!hud_.Over()) hud_.SetLife(hud_.OpponentLife(), hud_.PlayerLife() - 700);
        return;
      case SDLK_D:
        DrawCard();
        return;
      case SDLK_G:
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
  if (sound_button_.HandleEvent(event)) {
    if (ctx.music != nullptr) ctx.music->ToggleMuted();
    return;
  }
  // Clicks on the buttons never reach the table.
  if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
      sound_button_.Contains(event.button.x, event.button.y)) {
    return;
  }
  if (hint_button_.HandleEvent(event)) {
    hints_enabled_ = !hints_enabled_;
    return;
  }
  if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
      hint_button_.Contains(event.button.x, event.button.y)) {
    return;
  }
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
      if (CanAct() && AttacksAllowed(Side::kPlayer) && hand_.Hovered() < 0) {
        const int slot = board_.PickSlot(pos, arena_view_proj_, ctx.logical_width,
                                         ctx.logical_height);
        const Board::Card* card = board_.At(slot);
        if (card != nullptr && !card->defense && !card->attacked) {
          attack_from_ = slot;
        }
      }
    }
    hand_.MouseDown(pos, event.button.button == SDL_BUTTON_LEFT, event.button.clicks);
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
    const math::Vec2 pos{event.button.x, event.button.y};
    const bool inspecting = hand_.Inspecting();
    // Dropping a dragged hand card on a slot summons it; anywhere else it returns to the hand.
    if (CanSummon()) {
      const bool field = DraggedIsField();
      const int slot =
          board_.PickSlot(pos, arena_view_proj_, ctx.logical_width, ctx.logical_height);
      const int opponent_slot =
          opponent_board_.PickSlot(pos, arena_view_proj_, ctx.logical_width, ctx.logical_height);
      // Field card: either side of the arena works; the slot is ignored.
      if (slot >= 0 || (field && opponent_slot >= 0)) Summon(field ? 0 : slot);
    }
    hand_.MouseUp(pos, event.button.clicks);
    const float moved = std::fabs(pos.x - press_.x) + std::fabs(pos.y - press_.y);
    if (attack_from_ >= 0 && attack_drag_) {
      if (CanAct()) {
        const int target = opponent_board_.PickSlot(pos, arena_view_proj_, ctx.logical_width,
                                                    ctx.logical_height);
        if (target >= 0 && !opponent_board_.Empty(target)) {
          PerformAttack(Side::kPlayer, attack_from_, target);
        } else if (target >= 0 && opponent_board_.Count() == 0) {
          PerformAttack(Side::kPlayer, attack_from_, -1);
        }
      }
    } else if (!inspecting && press_.x >= 0 && moved < kClickSlop && hand_.Hovered() < 0 &&
               CanAct()) {
      const int slot =
          board_.PickSlot(pos, arena_view_proj_, ctx.logical_width, ctx.logical_height);
      if (slot >= 0 && board_.TogglePosition(slot)) {
        char name[3];
        LOG("Turn %d: card at %s switched to %s", turn_, SlotName(slot, name),
                board_.At(slot)->defense ? "defense" : "attack");
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
  sfx_.Update();
  if (!ready_ || paused_) return;
  sfx_.SetMuted(ctx.music != nullptr && ctx.music->Muted());
  for (auto it = sfx_queue_.begin(); it != sfx_queue_.end();) {
    it->first -= dt_seconds;
    if (it->first > 0.0f) {
      ++it;
      continue;
    }
    sfx_.Play(it->second);
    it = sfx_queue_.erase(it);
  }
  hud_.Update(dt_seconds, mouse_.x, mouse_.y);
  if (reveal_.card != nullptr) reveal_.time += dt_seconds;
  UpdateIntro(dt_seconds);
  if (shake_left_ > 0.0f) {
    shake_left_ = std::max(0.0f, shake_left_ - dt_seconds);
    shake_time_ += dt_seconds;
  }
  if (orbit_ < orbit_target_) orbit_ = std::min(orbit_target_, orbit_ + dt_seconds / kOrbitSeconds);
  if (orbit_ > orbit_target_) orbit_ = std::max(orbit_target_, orbit_ - dt_seconds / kOrbitSeconds);
  UpdateCamera();
  if (!started_ && intro_time_ >= kMatchStart) StartMatch();
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
  // Mouse off screen: the opponent hand ignores it.
  opponent_hand_view_.Update(dt_seconds, math::Vec2{-1.0f, -1.0f}, hand_view_proj_,
                             ctx.logical_width, ctx.logical_height);
  const bool field_selected = DraggedIsField();
  if (field_selected || field_.active) {
    bool evil = field_.favored == 'E';
    if (field_selected) {
      evil = hand_.CardAt(static_cast<size_t>(hand_.Dragging())).info->favored == "evil";
    }
    field_mist_color_ = colors::GetF(evil ? "board.fieldMistEvil" : "board.fieldMistGood");
    field_fog_color_ = colors::GetF(evil ? "board.fieldFogEvil" : "board.fieldFogGood");
  }
  field_glow_ += ((field_selected || field_.active ? 1.0f : 0.0f) - field_glow_) *
                 std::min(1.0f, dt_seconds * kFieldGlowSpeed);
  arena_atmosphere_.fog_color = LerpColor(fog_color_, field_fog_color_, field_glow_);
  const bool summoning = CanSummon();
  board_.SetHighlight(summoning);
  int hover = -1;
  int opponent_hover = -1;
  if (attack_from_ >= 0 && attack_drag_) {
    hover = attack_from_;
    const int target = opponent_board_.PickSlot(mouse_, arena_view_proj_, ctx.logical_width,
                                                ctx.logical_height);
    if (target >= 0 && (!opponent_board_.Empty(target) || opponent_board_.Count() == 0)) {
      opponent_hover = target;
    }
  } else if (CanAct() && hand_.Hovered() < 0) {
    hover = board_.PickSlot(mouse_, arena_view_proj_, ctx.logical_width, ctx.logical_height);
    if (!summoning && board_.Empty(hover)) hover = -1;
  }
  board_.SetHoverSlot(hover);
  opponent_board_.SetHoverSlot(opponent_hover);
  board_.SetHoverRole(HoverRole());
  opponent_board_.SetHoverRole(HoverRole());
  board_.Update(dt_seconds);
  opponent_board_.Update(dt_seconds);
  sword_.Update(dt_seconds);
  shield_.Update(dt_seconds);
  tooltip_slot_ = -1;
  if (intro_ >= 1.0f && hand_.Hovered() < 0 && hand_.Dragging() < 0 && !hand_.Inspecting() &&
      mouse_.x >= 0) {
    for (Side side : {Side::kPlayer, Side::kOpponent}) {
      const Board& board = side == Side::kPlayer ? board_ : opponent_board_;
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
  opponent_board_.SetHoverCard(
      tooltip_slot_ >= 0 && tooltip_side_ == Side::kOpponent ? tooltip_slot_ : -1);
}

namespace {
void RevealCurve(float t, float& alpha, float& scale) {
  const float in = SmoothStep(t / kRevealFadeIn);
  const float out = 1.0f - SmoothStep((t - (kRevealSeconds - kRevealFadeOut)) / kRevealFadeOut);
  alpha = std::min(in, out);
  scale = (0.85f + 0.15f * in) * (0.92f + 0.08f * out);
}
}

void BattleScene::RenderReveal3D(int width, int height) {
  if (reveal_.card == nullptr || reveal_.front3d == nullptr) return;
  const float travel = reveal_.from_hand ? kRevealTravel : 0.0f;
  const float hold = reveal_.time - travel;
  float alpha = 0.0f, scale = 1.0f;
  RevealCurve(hold, alpha, scale);
  float veil_alpha = alpha;
  if (reveal_.from_hand) {  // coming from the hand, no zoom in
    RevealCurve(std::max(hold, kRevealFadeIn), alpha, scale);
    veil_alpha = std::min(SmoothStep(reveal_.time / travel), alpha);
  }
  const Hand::Layout& layout = hand_.GetLayout();
  // Veil behind the card, covering the whole hand camera view.
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
    const float u = SmoothStep(reveal_.time / travel);
    const math::Vec3 pos = reveal_.from_pos + (center - reveal_.from_pos) * u;
    const float s = math::Lerp(reveal_.from_scale, center_scale, u);
    const float roll = math::Lerp(reveal_.from_roll, 0.0f, u);
    const float flip = math::kPi * u;
    const bool hidden = flip < math::kPi * 0.5f;  // hidden front until half turn
    const math::Mat4 world =
        Hand::PoseMatrix(pos, s, hidden ? flip : flip - math::kPi, 0.0f, roll);
    hand_.DrawLoose(*gpu_, hand_view_proj_, world, hidden ? hidden_face_ : reveal_.front3d,
                    !hidden && reveal_.foil, 1.0f);
    return;
  }
  const float t = std::max(hold, 0.0f);
  const float sway = reveal_.from_hand ? SmoothStep(t / 0.3f) : 1.0f;
  const float yaw = sway * 0.42f * std::sin(t * 3.1f);
  const float pitch = sway * 0.1f * std::sin(t * 2.3f);
  const math::Mat4 world = Hand::PoseMatrix(center, center_scale, yaw, pitch, 0.0f);
  hand_.DrawLoose(*gpu_, hand_view_proj_, world, reveal_.front3d, reveal_.foil, alpha);
}

void BattleScene::RenderReveal(SDL_Renderer* r, int width, int height) const {
  if (reveal_.card == nullptr) return;
  if (reveal_.front3d != nullptr) {  // card drawn in 3D: only caption and detail here
    float alpha = 0.0f, scale = 1.0f;
    const float hold = reveal_.time - (reveal_.from_hand ? kRevealTravel : 0.0f);
    RevealCurve(hold, alpha, scale);
    if (reveal_.from_hand) {
      float unused = 0.0f;
      RevealCurve(std::max(hold, kRevealFadeIn), unused, scale);
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
  const SDL_Color veil = with_alpha(colors::Get("background.overlay"));
  SDL_SetRenderDrawColor(r, veil.r, veil.g, veil.b, veil.a);
  const SDL_FRect full{0, 0, static_cast<float>(width), static_cast<float>(height)};
  SDL_RenderFillRect(r, &full);

  const float h = kRevealCardH * scale;
  const float w = h * kCardAspect;
  const float cx = width / 2.0f;
  const float cy = height / 2.0f + ui::Grid(1);
  const SDL_FRect card{cx - w / 2.0f, cy - h / 2.0f, w, h};
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
  SDL_SetTextureAlphaMod(reveal_.face, 255);

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
  const Board& board = tooltip_side_ == Side::kPlayer ? board_ : opponent_board_;
  const Board::Card* card = board.At(tooltip_slot_);
  if (card == nullptr || card->info == nullptr) return;
  const CardInfo& info = *card->info;
  const int attack = rules::EffectiveAttack(info, field_);
  const int defense = rules::EffectiveDefense(info, field_);

  struct Line {
    std::string text;
    float scale;
    SDL_Color color;
    float gap_after;
    text::Face face = text::Face::kTitle;
  };
  std::vector<Line> lines;
  const SDL_Color title = colors::Get("panel.title");
  const SDL_Color label = colors::Get("panel.label");
  const SDL_Color value = colors::Get("panel.value");
  auto stat_color = [&](int effective, int base) {
    return effective > base ? colors::Get("status.buff")
           : effective < base ? colors::Get("status.nerf")
                              : value;
  };
  lines.push_back({info.name, kTipScale, title, kTipGap * 2});
  lines.push_back({i18n::Get("cardface.atk") + " " + std::to_string(attack),
                   kTipScale, stat_color(attack, info.attack), kTipGap});
  lines.push_back({i18n::Get("cardface.def") + " " + std::to_string(defense),
                   kTipScale, stat_color(defense, info.defense), kTipGap * 2});
  const std::string mode =
      i18n::Get(card->defense ? "battle.tooltip.inDefense" : "battle.tooltip.inAttack");
  lines.push_back({i18n::Get("battle.tooltip.mode") + " " + mode, kTipDetailScale, label, kTipGap,
                   text::Face::kBody});
  lines.push_back({i18n::Get("battle.tooltip.category") + " " + info.CategoryLabel(),
                   kTipDetailScale, label, kTipGap, text::Face::kBody});
  if (info.hierarchy > 0) {
    std::string hier = i18n::Get("battle.tooltip.hierarchy") + " ";
    hier += info.hierarchy_name.empty() ? std::to_string(info.hierarchy)
                                        : info.hierarchy_name + " (" +
                                              std::to_string(info.hierarchy) + ")";
    lines.push_back({hier, kTipDetailScale, label, 0.0f, text::Face::kBody});
  }
  lines.back().gap_after = 0.0f;

  float text_w = 0.0f;
  float text_h = 0.0f;
  for (const Line& line : lines) {
    text_w = std::max(text_w, text::Width(line.text.c_str(), line.scale, line.face));
    text_h += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * line.scale + line.gap_after;
  }
  const float image_w = kTipImageH * kCardAspect;
  const float w = ui::SnapUp(3 * kTipPadding + image_w + std::max(text_w, kTipTextW * 0.6f));
  const float h = ui::SnapUp(2 * kTipPadding + std::max(kTipImageH, text_h));
  float x = mouse_.x + kTipOffset;
  float y = mouse_.y + kTipOffset;
  if (x + w > width) x = mouse_.x - kTipOffset - w;
  if (y + h > height) y = std::max(0.0f, mouse_.y - kTipOffset - h);
  const SDL_FRect panel{x, y, w, h};
  frame::Draw(r, panel, frame::Kind::kWindow);
  SDL_Color c;

  const SDL_FRect image{x + kTipPadding, y + kTipPadding, image_w, kTipImageH};
  SDL_Texture* face = const_cast<BattleScene*>(this)->HudFace(info);  // only fills the cache
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
    text::Draw(r, line.text.c_str(), tx, ty, line.scale, line.color, line.face);
    ty += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * line.scale + line.gap_after;
  }
}

std::string BattleScene::Hint(int width, int height) const {
  if (!started_ || hud_.Over() || paused_) return {};
  if (hand_.Inspecting()) return i18n::Get("battle.hint.inspect");
  if (!player_turn_) {
    return turn_side_ == Side::kOpponent && reveal_.card == nullptr
               ? i18n::Get("battle.hint.opponentTurn")
               : std::string();
  }
  if (!CanAct()) return {};

  if (attack_from_ >= 0 && attack_drag_) {
    const Board::Card* attacker = board_.At(attack_from_);
    if (attacker == nullptr || attacker->info == nullptr) return {};
    const int attack = rules::EffectiveAttack(*attacker->info, field_);
    if (opponent_board_.Count() == 0) {
      return i18n::Format("battle.hint.directAttack", {std::to_string(attack)});
    }
    const Board::Card* defender =
        opponent_board_.At(opponent_board_.PickSlot(mouse_, arena_view_proj_, width, height));
    if (defender == nullptr || defender->info == nullptr) {
      return i18n::Get("battle.hint.attackCard");
    }
    const rules::Outcome o =
        rules::Resolve(attack, defender->defense, rules::EffectiveAttack(*defender->info, field_),
                       rules::EffectiveDefense(*defender->info, field_));
    if (o.attacker_destroyed && o.defender_destroyed) {
      return i18n::Get("battle.hint.preview.tie");
    }
    if (o.defender_destroyed) {
      return o.defender_damage > 0 ? i18n::Format("battle.hint.preview.win",
                                                  {std::to_string(o.defender_damage)})
                                   : i18n::Get("battle.hint.preview.winDefense");
    }
    if (o.attacker_destroyed) {
      return i18n::Format("battle.hint.preview.lose", {std::to_string(o.attacker_damage)});
    }
    return o.attacker_damage > 0
               ? i18n::Format("battle.hint.preview.blocked", {std::to_string(o.attacker_damage)})
               : i18n::Get("battle.hint.preview.even");
  }

  if (hand_.Dragging() >= 0) {
    if (DraggedIsField()) {
      return i18n::Get(field_.active ? "battle.hint.replaceField" : "battle.hint.playField");
    }
    if (summoned_) return i18n::Get("battle.hint.alreadySummoned");
    if (hand_.Hovered() < 0 &&
        board_.At(board_.PickSlot(mouse_, arena_view_proj_, width, height)) != nullptr) {
      return i18n::Get("battle.hint.summonReplace");
    }
    return i18n::Get(board_.Count() >= Board::kSlots ? "battle.hint.boardFull"
                                                     : "battle.hint.summon");
  }

  if (hand_.Hovered() >= 0) return i18n::Get("battle.hint.handCard");

  if (tooltip_slot_ >= 0 && tooltip_side_ == Side::kPlayer) {
    if (const Board::Card* card = board_.At(tooltip_slot_)) {
      if (card->defense) return i18n::Get("battle.hint.cardDefense");
      if (!AttacksAllowed(Side::kPlayer)) return i18n::Get("battle.hint.cardFirstTurn");
      return i18n::Get(card->attacked ? "battle.hint.cardAttacked" : "battle.hint.cardAttack");
    }
  }

  if (AttacksAllowed(Side::kPlayer)) {
    for (int slot : board_.Occupied()) {
      const Board::Card* card = board_.At(slot);
      if (card != nullptr && !card->defense && !card->attacked) {
        return i18n::Get(opponent_board_.Count() == 0 ? "battle.hint.attackDirect"
                                                      : "battle.hint.attack");
      }
    }
  }
  bool has_battle_card = false;
  bool has_field_card = false;
  for (size_t i = 0; i < hand_.Size(); ++i) {
    const CardInfo* info = hand_.CardAt(i).info;
    if (info == nullptr) continue;
    if (info->IsField()) {
      has_field_card = true;
    } else {
      has_battle_card = true;
    }
  }
  if (!summoned_ && has_battle_card) return i18n::Get("battle.hint.select");
  if (hand_.Size() > static_cast<size_t>(kHandLimit)) return i18n::Get("battle.hint.handLimit");
  if (has_field_card) return i18n::Get("battle.hint.fieldLeft");
  const std::string end_turn = i18n::Get("battle.endTurn");
  return i18n::Format(AttacksAllowed(Side::kPlayer) ? "battle.hint.endTurn"
                                                    : "battle.hint.firstTurn",
                      {end_turn});
}

void BattleScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color fg = colors::Get("text.primary");
  const SDL_Color background = colors::Get("background.fog");
  const float cx = ctx.logical_width / 2.0f;

  if (ready_) {
    for (int k = 0; k < 2; ++k) {
      if (!gpu_->BeginShadowPass(k, torch_light_vp_[k])) break;
      if (arena_model_ != nullptr) arena_model_->DrawDepth(*gpu_, arena_world_);
      if (torch_ != nullptr) {
        for (const math::Mat4& world : torch_world_) torch_->DrawBodyDepth(*gpu_, world);
      }
      board_.RenderDepth(*gpu_);
      opponent_board_.RenderDepth(*gpu_);
      gpu_->EndShadowPass();
    }
  }
  if (ready_ && gpu_->BeginFrame(arena_atmosphere_.fog_color)) {
    gpu_->SetAtmosphere(arena_atmosphere_);
    if (arena_model_ != nullptr) {
      SDL_FColor gold = colors::GetF("board.fieldArena");
      gold = SDL_FColor{gold.r * kFieldArenaBoost, gold.g * kFieldArenaBoost,
                        gold.b * kFieldArenaBoost, 1.0f};
      arena_model_->Draw(*gpu_, arena_world_, arena_view_proj_,
                         LerpColor(SDL_FColor{1, 1, 1, 1}, gold, field_glow_));
    }
    if (torch_ != nullptr) {
      for (const math::Mat4& world : torch_world_) {
        torch_->DrawBody(*gpu_, world, arena_view_proj_);
      }
    }
    board_.Render(*gpu_, arena_view_proj_);
    opponent_board_.Render(*gpu_, arena_view_proj_);
    sword_.Render(*gpu_, arena_view_proj_);
    shield_.Render(*gpu_, arena_view_proj_);
    const SDL_FColor mist = LerpColor(mist_color_, field_mist_color_, field_glow_);
    for (const MistLayer& layer : mist_layers_) {
      gpu_->Draw(mist_quad_, nullptr, math::Mat4::Translate({0, layer.y, 0}), arena_view_proj_,
                 SDL_FColor{mist.r, mist.g, mist.b, layer.alpha}, Gpu::DrawMode::kMist);
    }
    if (intro_ < 1.0f) {  // intro mist, fading as the arena rises
      const float remain = 1.0f - intro_;
      Atmosphere dense = arena_atmosphere_;
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
    if (torch_ != nullptr) {
      gpu_->SetAtmosphere(arena_atmosphere_);
      for (const math::Mat4& world : torch_world_) {
        torch_->DrawFlame(*gpu_, world, arena_view_proj_, arena_atmosphere_.camera_pos,
                          ignite_);
      }
    }
    gpu_->NewLayer();  // hand and reveal on their own layer, above the arena
    gpu_->SetAtmosphere(hand_atmosphere_);
    hand_.Render(*gpu_, hand_view_proj_);
    opponent_hand_view_.Render(*gpu_, hand_view_proj_);
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

  if (started_ && !hud_.Over()) {
    const std::string status = i18n::Format(
        "battle.status",
        {std::to_string(turn_),
         i18n::Get(turn_side_ == Side::kPlayer && player_turn_ ? "battle.status.yourTurn"
                                                               : "battle.status.opponent")});
    text::DrawCentered(r, status.c_str(), cx, kStatusY, kStatusScale,
                       colors::Get("board.status"), text::Face::kBody);
  }
  if (attack_from_ >= 0 && attack_drag_ && mouse_.x >= 0) {
    const math::Vec2 from =
        ScreenOf(board_.CardPosition(attack_from_), ctx.logical_width, ctx.logical_height);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    DrawAttackArc(r, SDL_FPoint{from.x, from.y}, SDL_FPoint{mouse_.x, mouse_.y},
                  colors::Get(HoverRole()),
                  static_cast<float>(SDL_GetTicks()) / 1000.0f);
  }
  const std::string hint =
      hints_enabled_ ? Hint(ctx.logical_width, ctx.logical_height) : std::string();
  if (!hint.empty()) {
    text::DrawCentered(r, hint.c_str(), cx, kHintY, kHintScale, colors::Get("board.hint"),
                       text::Face::kBody);
  }
  hud_.Render(r, ctx.logical_width, ctx.logical_height);
  if (CanAct()) end_turn_->Render(r);
  sound_button_.SetActive(ctx.music == nullptr || !ctx.music->Muted());
  sound_button_.Render(r);
  hint_button_.SetActive(hints_enabled_);
  hint_button_.Render(r);
  RenderReveal(r, ctx.logical_width, ctx.logical_height);
  if (!paused_ && reveal_.card == nullptr) RenderTooltip(r, ctx.logical_width, ctx.logical_height);
  abandon_dialog_.Render(r, ctx.logical_width, ctx.logical_height);
}
