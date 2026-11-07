#include "scenes/grimoire/grimoireScene.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <thread>

#include "scripts/cardface/cardFace.h"
#include "scripts/colors/colors.h"
#include "scripts/opacity/opacity.h"
#include "scripts/config/config.h"
#include "scripts/frame/frame.h"
#include "scripts/i18n/i18n.h"
#include "scripts/log/log.h"
#include "scripts/rules/rules.h"
#include "scripts/save/save.h"
#include "scripts/text/text.h"
#include "scripts/layout/layout.h"
#include "scripts/ui/ui.h"

namespace {

constexpr float kCardAspect = static_cast<float>(cardface::kWidth) / cardface::kHeight;
// Card codes never start with '?'.
constexpr const char* kHiddenFace = "?";

bool Inside(const SDL_FRect& rect, float x, float y) {
  return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

float SmoothStep(float t) {
  t = std::clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float Approach(float value, float target, float speed, float dt) {
  return value + (target - value) * (1.0f - std::exp(-speed * dt));
}

// Sizes from ui.json, motion and timing from layout.json, text from text.json ("grimoire").
struct Measures {
  float title_y = ui::Cells("grimoire.titleCenterY", 4.0f);
  float filter_y = ui::Cells("grimoire.filterY", 8.0f);
  float filter_gap = ui::Cells("grimoire.filterGap", 1.0f);
  float grid_top = ui::Cells("grimoire.gridTop", 13.0f);
  float grid_bottom = ui::Cells("grimoire.gridBottom", 54.0f);
  float card_gap = ui::Cells("grimoire.cardGap", 1.0f);
  float nav_y = ui::Cells("grimoire.navY", 56.0f);
  float margin = ui::Cells("grimoire.margin", 4.0f);
  float exit_margin = ui::Cells("grimoire.exitMargin", 1.0f);
  float hover_grow = ui::Number("grimoire.hoverGrow", 0.06f);
  float strip_height = ui::Cells("grimoire.stripHeight", 2.5f);
  float strip_inset = ui::Cells("grimoire.stripInset", 0.5f);
  float count_width = ui::Cells("grimoire.countWidth", 4.0f);
  float slide_seconds = layout::Number("grimoire.slideSeconds", 0.8f);
  float slide_shrink = layout::Number("grimoire.slideShrink", 0.25f);  // size lost while leaving
  // Hovered grid card a bit closer to the camera.
  float hover_lift = layout::Number("grimoire.hoverLift", 0.05f);
  int columns = std::max(1, static_cast<int>(ui::Number("grimoire.columns", 4.0f)));
  int rows = std::max(1, static_cast<int>(ui::Number("grimoire.rows", 2.0f)));
  float fade_out = layout::Number("grimoire.fadeOut", 0.4f);
  float menu_fade_in = layout::Number("grimoire.menuFadeIn", 0.4f);
  text::Style title_style = text::Styles({"title"});
  text::Style page_style = text::Styles({"xsmall", "body"});
  text::Style strip_style = text::Styles({"small"});
  text::Style deck_style = text::Styles({"large"});
};

const Measures& M() {
  static const Measures m;
  return m;
}

int Columns() { return M().columns; }
int Rows() { return M().rows; }
int PerPage() { return M().columns * M().rows; }

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

const char* CategoryKey(char code) {
  switch (code) {
    case 'G':
      return "category.good";
    case 'E':
      return "category.evil";
    case 'N':
      return "category.neutral";
    default:
      return "category.field";
  }
}

SDL_Texture* Upload(SDL_Renderer* renderer, const Image& image) {
  if (!image.Valid()) return nullptr;
  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STATIC, image.width, image.height);
  if (texture == nullptr) return nullptr;
  SDL_UpdateTexture(texture, nullptr, image.rgba.data(), image.width * 4);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
  return texture;
}

}

constexpr char GrimoireScene::kCategoryCodes[];

GrimoireScene::GrimoireScene() {
  for (int i = 0; i < kCategories; ++i) {
    filters_[i] = std::make_unique<Button>(i18n::Get(CategoryKey(kCategoryCodes[i])),
                                           [this, i](SceneContext&) {
                                             selected_[i] = !selected_[i];
                                             filters_[i]->SetFocused(selected_[i]);
                                             Refilter();
                                           });
    filters_[i]->SetFocused(true);
    filters_[i]->SetSize(Button::Size::kSmall);
  }
  back_ = std::make_unique<Button>(i18n::Get("general.back"),
                                   [this](SceneContext&) { ChangePage(-1); });
  forward_ = std::make_unique<Button>(i18n::Get("general.forward"),
                                      [this](SceneContext&) { ChangePage(+1); });
  back_->SetSize(Button::Size::kSmall);
  forward_->SetSize(Button::Size::kSmall);
  exit_ = std::make_unique<Button>(i18n::Get("general.menu"), [](SceneContext& ctx) {
    ctx.fade_out_seconds = M().fade_out;
    ctx.fade_in_seconds = M().menu_fade_in;
    ctx.next_scene = SceneId::kMenu;
  });
}

GrimoireScene::~GrimoireScene() {
  if (inspect_full_.valid()) inspect_full_.wait();
  for (auto& [code, texture] : textures_) SDL_DestroyTexture(texture);
  if (gpu_ != nullptr) {
    if (inspect_front_ != nullptr) gpu_->DestroyTexture(inspect_front_);
    for (auto& [code, texture] : gpu_faces_) gpu_->DestroyTexture(texture);
    for (SDL_GPUTexture* t : {card_back_, front_normal_, back_normal_}) {
      if (t != nullptr) gpu_->DestroyTexture(t);
    }
    gpu_->DestroyMesh(veil_quad_);
    card_aura_.Destroy(*gpu_);
    card_model_.reset();
  }
}

// Composes every card front at half size on worker threads and sets up the 3D card for inspect.
void GrimoireScene::Preload(const PreloadInfo& info) {
  preloaded_ = true;
  const std::string base = BasePath();
  cards_ = cards::LoadAll(base);
  unlocked_.clear();
  if (info.save) {
    for (const save::Unlock& unlock : info.save->unlocked) unlocked_.insert(unlock.code);
  }
  if (info.gpu_device != nullptr) {
    gpu_ = std::make_unique<Gpu>();
    if (!gpu_->InitDevice(info.gpu_device, info.logical_width, info.logical_height,
                          base + "shaders")) {
      gpu_.reset();
    }
  }
  if (gpu_ != nullptr) {
    card_model_ = std::make_unique<Model>();
    if (!card_model_->Upload(*gpu_, Model::Parse(base + "models/card.glb"))) {
      LOG("Grimoire: models/card.glb not loaded; inspect unavailable");
      card_model_.reset();
      gpu_.reset();
    }
  }
  if (gpu_ != nullptr) {
    card_aura_.Init(*gpu_, *card_model_);
    card_back_ = gpu_->CreateTexture(Image::LoadFile(base + "sprites/cardBack.webp"));
    front_normal_ = gpu_->CreateTexture(
        cardface::NormalMap(Image::LoadFile(base + "sprites/cardFrontNormalMap.webp"), 2));
    back_normal_ = gpu_->CreateTexture(Image::LoadFile(base + "sprites/cardBackNormalMap.webp"));
    hand_.SetModel(card_model_.get());
    hand_.SetAura(&card_aura_);
    hand_.SetBack(card_back_);
    hand_.SetNormalMaps(front_normal_, back_normal_);
    hand_.SetAwayNow(true);  // the single card waits below the screen
    veil_quad_ = gpu_->CreateMesh({{-0.5f, -0.5f, 0, 0, 1}, {0.5f, -0.5f, 0, 1, 1},
                                   {0.5f, 0.5f, 0, 1, 0}, {-0.5f, 0.5f, 0, 0, 0}},
                                  {0, 1, 2, 0, 2, 3});
  }
  Image frames[4] = {Image::LoadFile(base + "sprites/cardFrontGood.webp").Downscaled(2),
                     Image::LoadFile(base + "sprites/cardFrontEvil.webp").Downscaled(2),
                     Image::LoadFile(base + "sprites/cardFrontNeutral.webp").Downscaled(2),
                     Image::LoadFile(base + "sprites/cardFrontField.webp").Downscaled(2)};
  auto frame_for = [&](const CardInfo& card) -> const Image& {
    switch (card.category) {
      case 'G':
        return frames[0];
      case 'E':
        return frames[1];
      case 'N':
        return frames[2].Valid() ? frames[2] : frames[3];
      default:
        return frames[3];
    }
  };
  const Uint64 started = SDL_GetTicksNS();
  std::vector<Image> results(cards_.size());
  std::atomic<size_t> next{0};
  const unsigned workers = SDL_clamp(std::thread::hardware_concurrency(), 1u, 8u);
  std::vector<std::thread> pool;
  for (unsigned k = 0; k < workers; ++k) {
    pool.emplace_back([&] {
      for (size_t i = next++; i < cards_.size(); i = next++) {
        const CardInfo& card = cards_[i];
        if (!Unlocked(card)) continue;
        const Image& frame = frame_for(card);
        if (!frame.Valid()) continue;
        Image art;
        if (!card.image_url.empty()) art = cardface::FitArt(Image::LoadFile(base + card.image_url));
        results[i] = cardface::Compose(frame, card, art.Valid() ? &art : nullptr).Downscaled(2);
      }
    });
  }
  for (std::thread& t : pool) t.join();
  for (size_t i = 0; i < cards_.size(); ++i) {
    if (results[i].Valid()) faces_[cards_[i].code] = std::move(results[i]);
  }
  Image hidden = cardface::ComposeHidden(
      Image::LoadFile(base + "sprites/cardFrontOpponent.webp").Downscaled(2));
  if (hidden.Valid()) faces_[kHiddenFace] = hidden.Downscaled(2);
  LOG("Grimoire: %zu card front(s) composed (%.0f ms, %u threads)", faces_.size(),
      static_cast<double>(SDL_GetTicksNS() - started) / 1e6, workers);
}

void GrimoireScene::OnEnter(SceneContext& ctx) {
  if (!preloaded_) Preload(ctx.Preload());
  Layout(ctx);
  Refilter();
  if (gpu_ == nullptr) return;
  gpu_->SetRenderer(ctx.renderer);
  const float aspect = static_cast<float>(ctx.logical_width) / ctx.logical_height;
  const Hand::Layout& hand = hand_.GetLayout();
  hand_view_proj_ = math::Mat4::Perspective(hand.fov_y, aspect, 0.1f, 100.0f) *
                    math::Mat4::LookAt({0, 0, hand.camera_z}, {0, 0, 0}, {0, 1, 0});
  // Same light as the battle hand.
  hand_atmosphere_.light_dir = config::Vec3("light", "battle.hand.lightDir", {0.3f, 0.6f, 1.0f});
  hand_atmosphere_.light_intensity = config::Float("light", "battle.hand.lightIntensity", 0.5f);
  hand_atmosphere_.ambient =
      config::Color("light", "battle.hand.ambient", {0.34f, 0.36f, 0.46f, 1.0f});
  hand_atmosphere_.point_pos = config::Vec3("light", "battle.hand.pointPos", {0.0f, 2.5f, 4.0f});
  hand_atmosphere_.point_intensity = config::Float("light", "battle.hand.pointIntensity", 1.0f);
  hand_atmosphere_.camera_pos = {0, 0, hand_.GetLayout().camera_z};
}

void GrimoireScene::OnExit(SceneContext&) {
  if (inspect_full_.valid()) inspect_full_.wait();
  for (auto& [code, texture] : textures_) SDL_DestroyTexture(texture);
  textures_.clear();
  if (gpu_ != nullptr) {
    for (auto& [code, texture] : gpu_faces_) gpu_->DestroyTexture(texture);
  }
  gpu_faces_.clear();
  hand_.StopInspect();
  EndInspect();
}

const std::string& GrimoireScene::FaceKey(const CardInfo& card) const {
  static const std::string hidden = kHiddenFace;
  return Unlocked(card) ? card.code : hidden;
}

SDL_GPUTexture* GrimoireScene::GpuFaceFor(const CardInfo& card) {
  const std::string& key = FaceKey(card);
  if (const auto it = gpu_faces_.find(key); it != gpu_faces_.end()) return it->second;
  const auto face = faces_.find(key);
  if (face == faces_.end()) return nullptr;
  SDL_GPUTexture* texture = gpu_->CreateTexture(face->second);
  if (texture != nullptr) gpu_faces_[key] = texture;
  return texture;
}

// Card model posed on the plane z of the hand camera so it covers `rect` on screen.
math::Mat4 GrimoireScene::PoseForRect(const SDL_FRect& rect, int width, int height, float z,
                                      float yaw, float pitch) const {
  const Hand::Layout& layout = hand_.GetLayout();
  const float visible_h = 2.0f * (layout.camera_z - z) * std::tan(layout.fov_y * 0.5f);
  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  const float cx = rect.x + rect.w / 2.0f;
  const float cy = rect.y + rect.h / 2.0f;
  const math::Vec3 pos{(cx / static_cast<float>(width) - 0.5f) * visible_h * aspect,
                       (0.5f - cy / static_cast<float>(height)) * visible_h, z};
  const float card_h = card_model_->BoundsMax().y - card_model_->BoundsMin().y;
  const float scale = (rect.h / static_cast<float>(height)) * visible_h / std::max(card_h, 1e-3f);
  return Hand::PoseMatrix(pos, scale, yaw, pitch, 0.0f);
}

void GrimoireScene::OpenInspect(int index, const SceneContext& ctx) {
  if (gpu_ == nullptr || inspect_ != nullptr) return;
  const int first = page_ * PerPage();
  if (first + index >= static_cast<int>(visible_.size())) return;
  const CardInfo* card = visible_[static_cast<size_t>(first + index)];
  if (!Unlocked(*card)) return;
  const auto face = faces_.find(card->code);
  if (face == faces_.end()) return;
  inspect_front_ = gpu_->CreateTexture(face->second);
  if (inspect_front_ == nullptr) return;
  inspect_ = card;
  hovered_ = -1;
  hand_.Clear();
  hand_.Add(inspect_front_, card->hierarchy == 1, card);  // rarest hierarchy gets the foil aura
  hand_.SetInspectOrigin(CardRect(index), ctx.logical_width, ctx.logical_height);
  hand_.Inspect(0);
  const CardInfo copy = *card;
  inspect_full_ = std::async(std::launch::async, [copy] {
    const std::string base = BasePath();
    const Image frame = Image::LoadFile(base + "sprites/" + copy.FrameSprite()).Downscaled(2);
    if (!frame.Valid()) return Image{};
    Image art;
    if (!copy.image_url.empty()) art = cardface::FitArt(Image::LoadFile(base + copy.image_url));
    return cardface::Compose(frame, copy, art.Valid() ? &art : nullptr);
  });
}

// Called once the card is back below the screen: frees the front and empties the hand.
void GrimoireScene::EndInspect() {
  if (inspect_full_.valid()) inspect_full_.wait();
  hand_.Clear();
  if (inspect_front_ != nullptr && gpu_ != nullptr) gpu_->DestroyTexture(inspect_front_);
  inspect_front_ = nullptr;
  inspect_ = nullptr;
}

void GrimoireScene::Layout(const SceneContext& ctx) {
  const Measures& m = M();
  const float width = static_cast<float>(ctx.logical_width);
  const float height = static_cast<float>(ctx.logical_height);
  // Filter row centered: each plate as wide as its label plus padding.
  float total = 0.0f;
  float widths[kCategories];
  for (int i = 0; i < kCategories; ++i) {
    widths[i] = filters_[i]->PreferredWidth();
    total += widths[i];
  }
  total += m.filter_gap * (kCategories - 1);
  float x = (width - total) / 2.0f;
  for (int i = 0; i < kCategories; ++i) {
    filters_[i]->SetRect(SDL_FRect{x, m.filter_y, widths[i], filters_[i]->Height()});
    x += widths[i] + m.filter_gap;
  }
  // Grid: as tall as fits between gridTop and gridBottom, as wide as fits the margins.
  gap_ = m.card_gap;
  const float avail_h = m.grid_bottom - m.grid_top;
  const float avail_w = width - 2 * m.margin;
  card_h_ = (avail_h - gap_ * (Rows() - 1)) / Rows();
  card_w_ = card_h_ * kCardAspect;
  if (card_w_ * Columns() + gap_ * (Columns() - 1) > avail_w) {
    card_w_ = (avail_w - gap_ * (Columns() - 1)) / Columns();
    card_h_ = card_w_ / kCardAspect;
  }
  const float grid_w = card_w_ * Columns() + gap_ * (Columns() - 1);
  const float grid_h = card_h_ * Rows() + gap_ * (Rows() - 1);
  grid_ = SDL_FRect{(width - grid_w) / 2.0f, m.grid_top + (avail_h - grid_h) / 2.0f, grid_w,
                    grid_h};
  const float back_w = back_->PreferredWidth();
  const float forward_w = forward_->PreferredWidth();
  back_->SetRect(SDL_FRect{grid_.x, m.nav_y, back_w, back_->Height()});
  forward_->SetRect(
      SDL_FRect{grid_.x + grid_.w - forward_w, m.nav_y, forward_w, forward_->Height()});
  const float exit_w = exit_->PreferredWidth();
  const float exit_h = exit_->Height();
  exit_->SetRect(
      SDL_FRect{width - m.exit_margin - exit_w, height - m.exit_margin - exit_h, exit_w, exit_h});
}

void GrimoireScene::Refilter() {
  visible_.clear();
  for (const CardInfo& card : cards_) {
    for (int i = 0; i < kCategories; ++i) {
      if (selected_[i] && card.category == kCategoryCodes[i]) visible_.push_back(&card);
    }
  }
  page_ = std::clamp(page_, 0, std::max(0, PageCount() - 1));
  hovered_ = -1;
  lift_card_ = -1;
  editing_ = nullptr;
}

int GrimoireScene::PageCount() const {
  return (static_cast<int>(visible_.size()) + PerPage() - 1) / PerPage();
}

void GrimoireScene::ChangePage(int delta) {
  if (slide_from_ >= 0) return;  // one slide at a time
  const int target = std::clamp(page_ + delta, 0, std::max(0, PageCount() - 1));
  if (target == page_) return;
  slide_from_ = page_;
  slide_dir_ = delta > 0 ? 1 : -1;
  slide_t_ = 0.0f;
  page_ = target;
  hovered_ = -1;
  lift_card_ = -1;
  editing_ = nullptr;
}

SDL_Texture* GrimoireScene::FaceFor(SDL_Renderer* r, const CardInfo& card) {
  const std::string& key = FaceKey(card);
  if (const auto it = textures_.find(key); it != textures_.end()) return it->second;
  const auto face = faces_.find(key);
  if (face == faces_.end()) return nullptr;
  SDL_Texture* texture = Upload(r, face->second);
  if (texture != nullptr) textures_[key] = texture;
  return texture;
}

SDL_FRect GrimoireScene::CardRect(int index) const {
  const int col = index % Columns();
  const int row = index / Columns();
  return SDL_FRect{grid_.x + col * (card_w_ + gap_), grid_.y + row * (card_h_ + gap_), card_w_,
                   card_h_};
}

SDL_FRect GrimoireScene::StripRect(const SDL_FRect& card) const {
  const float inset = M().strip_inset;
  const float h = M().strip_height;
  return SDL_FRect{card.x + inset, card.y + card.h - inset - h, card.w - 2 * inset, h};
}

SDL_FRect GrimoireScene::CountRect(const SDL_FRect& strip) const {
  const float w = M().count_width;
  return SDL_FRect{strip.x + strip.w - w, strip.y, w, strip.h};
}

int GrimoireScene::SlotAt(float x, float y) const {
  const int count = std::min(PerPage(), static_cast<int>(visible_.size()) - page_ * PerPage());
  for (int i = 0; i < count; ++i) {
    if (Inside(CardRect(i), x, y)) return i;
  }
  return -1;
}

// Copies of the selected card in the deck; save::Data clamps to the available copies and deckSize.
void GrimoireScene::SetCopies(SceneContext& ctx, int copies) {
  if (editing_ == nullptr || !ctx.save) return;
  const int before = ctx.save->InDeck(editing_->code);
  ctx.save->SetInDeck(editing_->code, copies);
  if (ctx.save->InDeck(editing_->code) != before) save::Write(*ctx.save);
}

// Slot rectangle shifted by the page slide, with the whole grid scaled about its center.
SDL_FRect GrimoireScene::PageCardRect(int index, float offset_x, float scale) const {
  SDL_FRect rect = CardRect(index);
  const float cx = grid_.x + grid_.w / 2.0f + offset_x;
  const float cy = grid_.y + grid_.h / 2.0f;
  rect.x = cx + (rect.x + offset_x - cx) * scale;
  rect.y = cy + (rect.y - cy) * scale;
  rect.w *= scale;
  rect.h *= scale;
  return rect;
}

SDL_FRect Grown(SDL_FRect rect, float grow) {
  rect.x -= rect.w * grow / 2.0f;
  rect.y -= rect.h * grow / 2.0f;
  rect.w *= 1.0f + grow;
  rect.h *= 1.0f + grow;
  return rect;
}

void GrimoireScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  if (event.type == SDL_EVENT_MOUSE_MOTION) mouse_ = {event.motion.x, event.motion.y};
  if (inspect_ != nullptr) {
    // Inspecting: the hand handles drag to rotate, click to flip, wheel to zoom; Esc, right
    // click or a click outside the card closes it. Nothing else reacts meanwhile.
    switch (event.type) {
      case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE) hand_.StopInspect();
        if (event.key.key == SDLK_SPACE) hand_.Flip();
        break;
      case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (hand_.Inspecting()) {
          hand_.MouseDown({event.button.x, event.button.y},
                          event.button.button == SDL_BUTTON_LEFT, event.button.clicks);
        }
        break;
      case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event.button.button == SDL_BUTTON_LEFT) {
          hand_.MouseUp({event.button.x, event.button.y}, event.button.clicks);
        }
        break;
      case SDL_EVENT_MOUSE_WHEEL:
        hand_.Wheel(event.wheel.y);
        break;
      default:
        break;
    }
    return;
  }
  if (event.type == SDL_EVENT_KEY_DOWN && editing_ != nullptr && ctx.save) {
    const SDL_Keycode key = event.key.key;
    const int copies = ctx.save->InDeck(editing_->code);
    int digit = -1;
    if (key >= SDLK_0 && key <= SDLK_9) digit = static_cast<int>(key - SDLK_0);
    if (key >= SDLK_KP_1 && key <= SDLK_KP_9) digit = static_cast<int>(key - SDLK_KP_1) + 1;
    if (key == SDLK_KP_0) digit = 0;
    if (digit >= 0) {
      SetCopies(ctx, typed_ ? copies * 10 + digit : digit);
      typed_ = true;
      return;
    }
    switch (key) {
      case SDLK_BACKSPACE:
        SetCopies(ctx, copies / 10);
        typed_ = true;
        return;
      case SDLK_UP:
        SetCopies(ctx, copies + 1);
        return;
      case SDLK_DOWN:
        SetCopies(ctx, copies - 1);
        return;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
      case SDLK_ESCAPE:
        editing_ = nullptr;
        return;
      default:
        break;
    }
  }
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
        exit_->OnClick(ctx);
        return;
      case SDLK_LEFT:
        ChangePage(-1);
        return;
      case SDLK_RIGHT:
        ChangePage(+1);
        return;
      default:
        break;
    }
  }
  if (event.type == SDL_EVENT_MOUSE_MOTION && slide_from_ < 0) {
    hovered_ = -1;
    const int count = std::min(PerPage(), static_cast<int>(visible_.size()) - page_ * PerPage());
    for (int i = 0; i < count; ++i) {
      const SDL_FRect rect = CardRect(i);
      if (event.motion.x >= rect.x && event.motion.x < rect.x + rect.w &&
          event.motion.y >= rect.y && event.motion.y < rect.y + rect.h) {
        hovered_ = i;
      }
    }
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT &&
      slide_from_ < 0) {
    const int slot = SlotAt(event.button.x, event.button.y);
    const CardInfo* card =
        slot >= 0 ? visible_[static_cast<size_t>(page_ * PerPage() + slot)] : nullptr;
    const CardInfo* next = card != nullptr && Unlocked(*card) && ctx.save ? card : nullptr;
    if (next != editing_) typed_ = false;
    editing_ = next;
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT &&
      event.button.clicks >= 2 && slide_from_ < 0) {
    const int count = std::min(PerPage(), static_cast<int>(visible_.size()) - page_ * PerPage());
    for (int i = 0; i < count; ++i) {
      if (Inside(CardRect(i), event.button.x, event.button.y)) {
        OpenInspect(i, ctx);
        return;
      }
    }
  }
  for (auto& filter : filters_) {
    if (filter->HandleEvent(event)) {
      filter->OnClick(ctx);
      return;
    }
  }
  for (Button* button : {back_.get(), forward_.get(), exit_.get()}) {
    if (button->HandleEvent(event)) {
      button->OnClick(ctx);
      return;
    }
  }
}

void GrimoireScene::Update(SceneContext& ctx, float dt_seconds) {
  // Hovered card tilt: mouse at the corners leans it up to tilt_max, like the hand.
  const Hand::Layout& layout = hand_.GetLayout();
  float tilt_x_target = 0.0f, tilt_y_target = 0.0f;
  if (hovered_ >= 0 && inspect_ == nullptr && slide_from_ < 0) {
    const SDL_FRect rect = CardRect(hovered_);
    const float rx = std::clamp((mouse_.x - (rect.x + rect.w / 2.0f)) / (rect.w / 2.0f), -1.0f, 1.0f);
    const float ry = std::clamp((mouse_.y - (rect.y + rect.h / 2.0f)) / (rect.h / 2.0f), -1.0f, 1.0f);
    tilt_x_target = -ry * layout.tilt_max;
    tilt_y_target = rx * layout.tilt_max;
  }
  tilt_x_ = Approach(tilt_x_, tilt_x_target, layout.tilt_speed, dt_seconds);
  tilt_y_ = Approach(tilt_y_, tilt_y_target, layout.tilt_speed, dt_seconds);
  const bool raised = hovered_ >= 0 && inspect_ == nullptr && slide_from_ < 0;
  if (raised) lift_card_ = hovered_;
  hover_t_ = Approach(hover_t_, raised ? 1.0f : 0.0f, layout.anim_speed, dt_seconds);
  if (!raised && hover_t_ < 0.01f && std::fabs(tilt_x_) + std::fabs(tilt_y_) < 0.002f) {
    lift_card_ = -1;
  }
  if (inspect_ != nullptr) {
    hand_.Update(dt_seconds, mouse_, hand_view_proj_, ctx.logical_width, ctx.logical_height);
    if (!hand_.Inspecting() && !hand_.InspectShown()) EndInspect();
  }
  if (slide_from_ < 0) return;
  slide_t_ += dt_seconds / std::max(0.01f, M().slide_seconds);
  if (slide_t_ >= 1.0f) slide_from_ = -1;
}

// The full-size front arrives from the worker thread and replaces the half-size one.
void GrimoireScene::UpdateInspectFront() {
  if (inspect_ == nullptr || gpu_ == nullptr || !inspect_full_.valid()) return;
  if (inspect_full_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
  const Image full = inspect_full_.get();
  if (SDL_GPUTexture* texture = gpu_->CreateTexture(full)) {
    gpu_->DestroyTexture(inspect_front_);
    inspect_front_ = texture;
    if (hand_.Size() > 0) hand_.SetFront(0, texture);
  }
}

void GrimoireScene::RenderPage3D(int page, float offset_x, float alpha, float scale,
                                 bool with_hover, int width, int height) {
  const int first = page * PerPage();
  const int count = std::min(PerPage(), static_cast<int>(visible_.size()) - first);
  const float z = hand_.GetLayout().inspect_z;
  for (int i = 0; i < count; ++i) {
    if (with_hover && i == lift_card_) continue;  // drawn last, in front
    const CardInfo& card = *visible_[static_cast<size_t>(first + i)];
    SDL_GPUTexture* face = GpuFaceFor(card);
    if (face == nullptr) continue;
    hand_.DrawLoose(*gpu_, hand_view_proj_, PoseForRect(PageCardRect(i, offset_x, scale), width,
                                                       height, z),
                    face, card.hierarchy == 1 && Unlocked(card), alpha);
  }
  if (with_hover && lift_card_ >= 0 && lift_card_ < count) {
    const CardInfo& card = *visible_[static_cast<size_t>(first + lift_card_)];
    SDL_GPUTexture* face = GpuFaceFor(card);
    if (face == nullptr) return;
    hand_.DrawLoose(
        *gpu_, hand_view_proj_,
        PoseForRect(Grown(PageCardRect(lift_card_, offset_x, scale), M().hover_grow * hover_t_),
                    width, height, z + M().hover_lift * hover_t_, tilt_y_, tilt_x_),
        face, card.hierarchy == 1 && Unlocked(card), alpha);
  }
}

// Grid pages, then the inspect veil and card, all in one 3D pass composited over the 2D chrome.
void GrimoireScene::RenderPages(SDL_Renderer* r, const SceneContext& ctx) {
  const float travel = static_cast<float>(ctx.logical_width);
  const float shrink = M().slide_shrink;
  const float t = std::clamp(slide_t_, 0.0f, 1.0f);
  const float e = SmoothStep(t);
  if (gpu_ == nullptr) {
    if (slide_from_ >= 0) {
      RenderPage(r, slide_from_, -slide_dir_ * travel * e, 1.0f - e, 1.0f - shrink * e, false);
      RenderPage(r, page_, slide_dir_ * travel * (1.0f - e), e, 1.0f - shrink * (1.0f - e),
                 false);
    } else {
      RenderPage(r, page_, 0.0f, 1.0f, 1.0f, true);
    }
    return;
  }
  UpdateInspectFront();
  if (!gpu_->BeginFrame(SDL_FColor{0.0f, 0.0f, 0.0f, 0.0f})) return;
  gpu_->SetAtmosphere(hand_atmosphere_);
  const int w = ctx.logical_width, h = ctx.logical_height;
  if (slide_from_ >= 0) {
    RenderPage3D(slide_from_, -slide_dir_ * travel * e, 1.0f - e, 1.0f - shrink * e, false, w, h);
    RenderPage3D(page_, slide_dir_ * travel * (1.0f - e), e, 1.0f - shrink * (1.0f - e), false,
                 w, h);
  } else {
    RenderPage3D(page_, 0.0f, 1.0f, 1.0f, true, w, h);
  }
  if (inspect_ != nullptr) {
    const Hand::Layout& layout = hand_.GetLayout();
    const float veil_z = layout.inspect_z;
    const float veil_h = 2.2f * (layout.camera_z - veil_z) * std::tan(layout.fov_y * 0.5f);
    const float aspect = static_cast<float>(w) / static_cast<float>(h);
    SDL_FColor veil = opacity::Apply(colors::GetF("stoneBlack"), "overlay");
    veil.a *= SmoothStep(hand_.InspectProgress());
    gpu_->NewLayer();
    gpu_->Draw(veil_quad_, nullptr,
               math::Mat4::Translate({0.0f, 0.0f, veil_z}) *
                   math::Mat4::Scale({veil_h * aspect, veil_h, 1.0f}),
               hand_view_proj_, veil, Gpu::DrawMode::kEmissive);
    gpu_->NewLayer();
    hand_.Render(*gpu_, hand_view_proj_);
  }
  gpu_->EndFrame();
  SDL_RenderTexture(r, gpu_->Target(), nullptr, nullptr);
}

void GrimoireScene::RenderPage(SDL_Renderer* r, int page, float offset_x, float alpha,
                               float scale, bool with_hover) {
  const Uint8 mod = static_cast<Uint8>(std::clamp(alpha, 0.0f, 1.0f) * 255.0f);
  const int first = page * PerPage();
  const int count = std::min(PerPage(), static_cast<int>(visible_.size()) - first);
  SDL_FRect hovered_rect{};
  SDL_Texture* hovered_face = nullptr;
  for (int i = 0; i < count; ++i) {
    const CardInfo& card = *visible_[static_cast<size_t>(first + i)];
    SDL_Texture* face = FaceFor(r, card);
    SDL_FRect rect = PageCardRect(i, offset_x, scale);
    if (face == nullptr) {
      frame::Draw(r, rect, frame::Kind::kWindow, frame::State::kNormal, alpha);
      continue;
    }
    SDL_SetTextureAlphaMod(face, mod);
    if (with_hover && i == lift_card_) {
      // Drawn last, slightly larger, over its neighbors.
      hovered_rect = rect;
      hovered_face = face;
      continue;
    }
    SDL_RenderTexture(r, face, nullptr, &rect);
  }
  if (hovered_face != nullptr) {
    const SDL_FRect rect = Grown(hovered_rect, M().hover_grow * hover_t_);
    SDL_RenderTexture(r, hovered_face, nullptr, &rect);
  }
}

// Deck total in the bottom left corner.
void GrimoireScene::RenderDeckBuilder(SDL_Renderer* r, const SceneContext& ctx) const {
  if (!ctx.save) return;
  const Measures& m = M();
  const save::Data& data = *ctx.save;
  const int deck_size = rules::Get().deck_size;
  const int in_deck = static_cast<int>(data.deck.size());
  const std::string total =
      i18n::Format("grimoire.deck", {std::to_string(in_deck), std::to_string(deck_size)});
  const SDL_FRect& exit = exit_->Rect();
  text::Draw(r, total.c_str(), m.exit_margin,
             exit.y + (exit.h - text::LineHeight(m.deck_style)) / 2.0f, m.deck_style,
             colors::Get(in_deck == deck_size ? "green" : "silver"));
}

// Strips over the 3D pass, on the bottom of each unlocked card; the raised card's grows with it.
void GrimoireScene::RenderCardOverlay(SDL_Renderer* r, const SceneContext& ctx) const {
  if (!ctx.save || slide_from_ >= 0 || inspect_ != nullptr) return;
  const Measures& m = M();
  const save::Data& data = *ctx.save;
  const std::map<std::string, int> available = data.Available();
  const int first = page_ * PerPage();
  const int count = std::min(PerPage(), static_cast<int>(visible_.size()) - first);
  for (int i = 0; i < count; ++i) {
    const CardInfo& card = *visible_[static_cast<size_t>(first + i)];
    if (!Unlocked(card)) continue;
    const bool selected = &card == editing_;
    const SDL_FRect rect =
        i == lift_card_ ? Grown(CardRect(i), m.hover_grow * hover_t_) : CardRect(i);
    const SDL_FRect strip = StripRect(rect);
    frame::Draw(r, strip, frame::Kind::kWindow);
    const auto copies_unlocked = available.find(card.code);
    const int unlocked_count = copies_unlocked != available.end() ? copies_unlocked->second : 0;
    const std::string unlocked =
        i18n::Format("grimoire.unlocked", {std::to_string(unlocked_count)});
    text::Draw(r, unlocked.c_str(), strip.x + m.strip_inset,
               strip.y + (strip.h - text::LineHeight(m.strip_style)) / 2.0f, m.strip_style,
               colors::Get("steelLight"));
    const SDL_FRect field = CountRect(strip);
    frame::Draw(r, field, frame::Kind::kWindow,
                selected ? frame::State::kHighlight : frame::State::kNormal);
    const int copies = data.InDeck(card.code);
    text::DrawCentered(r, std::to_string(copies).c_str(), field.x + field.w / 2.0f,
                       field.y + field.h / 2.0f, m.strip_style,
                       colors::Get(copies > 0 ? "silverLight" : "steelLight"));
  }
}

void GrimoireScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color background = colors::Get("stoneDarker");
  SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
  SDL_RenderClear(r);
}

void GrimoireScene::RenderHud(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const Measures& m = M();
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  const float cx = ctx.logical_width / 2.0f;
  text::DrawCentered(r, i18n::Get("grimoire.title").c_str(), cx, m.title_y, m.title_style,
                     colors::Get("silverLight"));
  for (int i = 0; i < kCategories; ++i) {
    filters_[i]->SetAlpha(selected_[i] ? 1.0f : opacity::Get("disabled"));
    filters_[i]->Render(r);
  }

  if (visible_.empty()) {
    text::DrawCentered(r, i18n::Get("grimoire.empty").c_str(), cx, grid_.y + grid_.h / 2.0f,
                       m.page_style, colors::Get("steelLight"));
  }

  const std::string page = i18n::Format(
      "grimoire.page", {std::to_string(PageCount() == 0 ? 0 : page_ + 1),
                        std::to_string(PageCount())});
  text::DrawCentered(r, page.c_str(), cx, m.nav_y + back_->Height() / 2.0f, m.page_style,
                     colors::Get("steelLight"));
  back_->SetAlpha(page_ > 0 ? 1.0f : opacity::Get("disabled"));
  forward_->SetAlpha(page_ + 1 < PageCount() ? 1.0f : opacity::Get("disabled"));
  back_->Render(r);
  forward_->Render(r);
  exit_->Render(r);
  RenderDeckBuilder(r, ctx);
  // Pages last: the 3D target (cards, veil, inspected card) is composited over the chrome.
  RenderPages(r, ctx);
  RenderCardOverlay(r, ctx);
}
