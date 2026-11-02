#include "scenes/menu/MenuScene.h"

#include <algorithm>
#include <string>

#include "scripts/ai/Ai.h"
#include "scripts/button/ButtonAction.h"
#include "scripts/button/ButtonExit.h"
#include "scripts/button/ButtonPlay.h"
#include "scripts/colors/Colors.h"
#include "scripts/image/Image.h"
#include "scripts/music/Music.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

// Grade de 16 px: botões de 22 x 5 células, 2 células entre eles.
constexpr float kButtonWidth = ui::Grid(22);
constexpr float kButtonHeight = ui::Grid(5);
constexpr float kButtonGap = ui::Grid(2);
constexpr float kTitleScale = 6.0f * ui::kScale;  // só sem o logo
// Topo na grade de 16 px: ícone (assets/icon.png) de 8 células centrado em y = 6 células;
// logo (sprites/Logo.png, de art/logo.png por tools/make_logo.py) com 40 células de largura
// centrado em y = 16 células; os botões começam em ~25 células.
constexpr float kIconSize = ui::Grid(8);     // 128 px
constexpr float kIconCenterY = ui::Grid(6);
constexpr float kLogoWidth = ui::Grid(40);   // 640 px
constexpr float kLogoCenterY = ui::Grid(16);

SDL_Texture* LoadTexture(SDL_Renderer* r, const Image& image) {
  if (!image.Valid()) return nullptr;
  SDL_Texture* texture = SDL_CreateTexture(r, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC,
                                           image.width, image.height);
  if (texture == nullptr) return nullptr;
  SDL_UpdateTexture(texture, nullptr, image.rgba.data(), image.width * 4);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
  return texture;
}

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}
constexpr float kFooterScale = 2.0f * ui::kScale;
constexpr float kSoundButtonSize = ui::Grid(3);
constexpr float kSoundMargin = ui::Grid(1);

std::string DifficultyLabel(int level) {
  return std::string("Nível: ") +
         ai::DifficultyName(static_cast<ai::Difficulty>(std::clamp(level, 0, 3)));
}

}  // namespace

MenuScene::MenuScene() {
  buttons_.push_back(std::make_unique<ButtonPlay>());
  // Nível do oponente (docs/menu.md): cada clique passa ao próximo; vale para a próxima batalha.
  auto difficulty = std::make_unique<ButtonAction>(
      DifficultyLabel(kDefaultDifficulty), [this](SceneContext& ctx) {
        ctx.difficulty = static_cast<int>(
            ai::NextDifficulty(static_cast<ai::Difficulty>(std::clamp(ctx.difficulty, 0, 3))));
        difficulty_button_->SetLabel(DifficultyLabel(ctx.difficulty));
      });
  difficulty_button_ = difficulty.get();
  buttons_.push_back(std::move(difficulty));
  buttons_.push_back(std::make_unique<ButtonExit>());
}

MenuScene::~MenuScene() {
  if (logo_ != nullptr) SDL_DestroyTexture(logo_);
  if (icon_ != nullptr) SDL_DestroyTexture(icon_);
}

void MenuScene::OnEnter(SceneContext& ctx) {
  if (logo_ == nullptr) {
    const Image image = Image::LoadFile(BasePath() + "sprites/Logo.png");
    logo_ = LoadTexture(ctx.renderer, image);
    if (logo_ != nullptr) {
      logo_w_ = image.width;
      logo_h_ = image.height;
    }
  }
  if (icon_ == nullptr) {
    // 1024x1024 reduzido 4x: aparece com 128 px.
    icon_ = LoadTexture(ctx.renderer, Image::LoadFile(BasePath() + "assets/icon.png").Downscaled(4));
  }
  difficulty_button_->SetLabel(DifficultyLabel(ctx.difficulty));
  LayoutButtons(ctx);
  sound_button_.SetRect(SDL_FRect{kSoundMargin, ctx.logical_height - kSoundMargin - kSoundButtonSize,
                                  kSoundButtonSize, kSoundButtonSize});
  clouds_ready_ = clouds_.Init(ctx.renderer, ctx.logical_width, ctx.logical_height);
  focused_ = 0;
  for (size_t i = 0; i < buttons_.size(); ++i) buttons_[i]->SetFocused(i == 0);
}

void MenuScene::LayoutButtons(const SceneContext& ctx) {
  const float total = buttons_.size() * kButtonHeight + (buttons_.size() - 1) * kButtonGap;
  const float x = (ctx.logical_width - kButtonWidth) / 2.0f;
  float y = ui::SnapUp((ctx.logical_height - total) / 2.0f);
  for (auto& button : buttons_) {
    button->SetRect(SDL_FRect{x, y, kButtonWidth, kButtonHeight});
    y += kButtonHeight + kButtonGap;
  }
}

void MenuScene::MoveFocus(int delta) {
  const int count = static_cast<int>(buttons_.size());
  focused_ = (focused_ + delta + count) % count;
  for (int i = 0; i < count; ++i) buttons_[i]->SetFocused(i == focused_);
}

void MenuScene::ActivateFocused(SceneContext& ctx) { buttons_[focused_]->OnClick(ctx); }

void MenuScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_UP:
        MoveFocus(-1);
        return;
      case SDLK_DOWN:
        MoveFocus(+1);
        return;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
      case SDLK_SPACE:
        ActivateFocused(ctx);
        return;
      case SDLK_ESCAPE:
        ctx.quit = true;
        return;
      default:
        break;
    }
  }

  if (sound_button_.HandleEvent(event)) {
    if (ctx.music != nullptr) ctx.music->ToggleMuted();
    return;
  }
  for (size_t i = 0; i < buttons_.size(); ++i) {
    if (buttons_[i]->HandleEvent(event)) {
      focused_ = static_cast<int>(i);
      buttons_[i]->OnClick(ctx);
      return;
    }
  }
}

void MenuScene::Update(SceneContext& /*ctx*/, float dt_seconds) {
  if (clouds_ready_) clouds_.Update(dt_seconds);
}

void MenuScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color background = colors::Get("background.menu");
  SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
  SDL_RenderClear(r);
  if (clouds_ready_) clouds_.RenderBack(r);

  const float center_x = ctx.logical_width / 2.0f;
  if (icon_ != nullptr) {
    const SDL_FRect dst{center_x - kIconSize / 2.0f, kIconCenterY - kIconSize / 2.0f, kIconSize,
                        kIconSize};
    SDL_RenderTexture(r, icon_, nullptr, &dst);
  }
  if (logo_ != nullptr) {
    const float h = kLogoWidth * static_cast<float>(logo_h_) / static_cast<float>(logo_w_);
    const SDL_FRect dst{center_x - kLogoWidth / 2.0f, kLogoCenterY - h / 2.0f, kLogoWidth, h};
    SDL_RenderTexture(r, logo_, nullptr, &dst);
  } else {
    text::DrawCentered(r, "PONTIFICE", center_x, kLogoCenterY, kTitleScale,
                       colors::Get("text.title"));
  }

  for (auto& button : buttons_) button->Render(r);
  sound_button_.Render(r, ctx.music != nullptr && ctx.music->Muted());

  text::DrawCentered(r, "Criado por pedraugo", center_x, ctx.logical_height - ui::Grid(3),
                     kFooterScale, colors::Get("text.secondary"));
  if (clouds_ready_) clouds_.RenderFront(r);  // névoa rala por cima do texto
}
