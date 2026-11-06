#include "scenes/menu/MenuScene.h"

#include <algorithm>
#include <string>

#include "scripts/ai/Ai.h"
#include "scripts/button/ButtonText.h"
#include "scripts/colors/Colors.h"
#include "scripts/i18n/I18n.h"
#include "scripts/music/Music.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

// Grade de 16 px: itens só de texto (ButtonText) em linhas de 4 células, 2 células
// entre elas, cada um centrado em x com a largura do próprio rótulo. Título "PONTIFICE" em
// PirataOne (fonte de título), centrado em y = 12 células.
constexpr float kRowHeight = ui::Grid(4);
constexpr float kRowGap = ui::Grid(2);
constexpr float kTitleScale = 8.0f * ui::kScale;  // linha de 96 px
constexpr float kTitleCenterY = ui::Grid(12);
// Transição para o loading (docs/loading.md): o menu escurece e o loading entra do preto.
constexpr float kMenuFadeOut = 0.5f;
constexpr float kLoadingFadeIn = 0.5f;
constexpr float kFooterScale = 2.0f * ui::kScale;
constexpr float kSoundButtonSize = ui::Grid(3);
constexpr float kSoundMargin = ui::Grid(1);

std::string DifficultyLabel(int level) {
  const auto difficulty = static_cast<ai::Difficulty>(std::clamp(level, 0, 3));
  return i18n::Format("menu.level", {i18n::Get(ai::DifficultyKey(difficulty))});
}

}  // namespace

MenuScene::MenuScene() {
  // Jogar: o menu escurece, a tela de loading entra do preto, prepara a batalha e abre em
  // seguida.
  buttons_.push_back(std::make_unique<ButtonText>(i18n::Get("menu.play"), [](SceneContext& ctx) {
    ctx.loading_target = SceneId::kBattle;
    ctx.fade_out_seconds = kMenuFadeOut;
    ctx.fade_in_seconds = kLoadingFadeIn;
    ctx.next_scene = SceneId::kLoading;
  }));
  // Nível do oponente (docs/menu.md): cada clique passa ao próximo; vale para a próxima batalha.
  auto difficulty = std::make_unique<ButtonText>(
      DifficultyLabel(kDefaultDifficulty), [this](SceneContext& ctx) {
        ctx.difficulty = static_cast<int>(
            ai::NextDifficulty(static_cast<ai::Difficulty>(std::clamp(ctx.difficulty, 0, 3))));
        difficulty_button_->SetLabel(DifficultyLabel(ctx.difficulty));
        LayoutButtons(ctx);  // o rótulo muda de largura
      });
  difficulty_button_ = difficulty.get();
  buttons_.push_back(std::move(difficulty));
  // Sair: encerra o jogo.
  buttons_.push_back(std::make_unique<ButtonText>(i18n::Get("menu.exit"),
                                                  [](SceneContext& ctx) { ctx.quit = true; }));
}

void MenuScene::OnEnter(SceneContext& ctx) {
  difficulty_button_->SetLabel(DifficultyLabel(ctx.difficulty));
  LayoutButtons(ctx);
  sound_button_.SetRect(SDL_FRect{kSoundMargin, ctx.logical_height - kSoundMargin - kSoundButtonSize,
                                  kSoundButtonSize, kSoundButtonSize});
  // Nenhum item começa selecionado (como na cena de idioma): foco só com as setas ou hover.
  focused_ = -1;
  for (auto& button : buttons_) button->SetFocused(false);
}

void MenuScene::LayoutButtons(const SceneContext& ctx) {
  const float total = buttons_.size() * kRowHeight + (buttons_.size() - 1) * kRowGap;
  float y = ui::SnapUp((ctx.logical_height - total) / 2.0f);
  for (auto& button : buttons_) {
    const float w = button->LabelWidth();
    button->SetRect(SDL_FRect{(ctx.logical_width - w) / 2.0f, y, w, kRowHeight});
    y += kRowHeight + kRowGap;
  }
}

void MenuScene::MoveFocus(int delta) {
  const int count = static_cast<int>(buttons_.size());
  if (focused_ < 0) {
    focused_ = delta > 0 ? 0 : count - 1;
  } else {
    focused_ = (focused_ + delta + count) % count;
  }
  for (int i = 0; i < count; ++i) buttons_[i]->SetFocused(i == focused_);
}

void MenuScene::ActivateFocused(SceneContext& ctx) {
  if (focused_ >= 0) buttons_[focused_]->OnClick(ctx);
}

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

void MenuScene::Update(SceneContext& /*ctx*/, float /*dt_seconds*/) {}

void MenuScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color background = colors::Get("background.menu");
  SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
  SDL_RenderClear(r);

  // Título em texto, fonte de título (PirataOne), centrado.
  const float center_x = ctx.logical_width / 2.0f;
  text::DrawCentered(r, "PONTIFICE", center_x, kTitleCenterY, kTitleScale,
                     colors::Get("text.title"));

  for (auto& button : buttons_) button->Render(r);
  sound_button_.SetActive(ctx.music == nullptr || !ctx.music->Muted());
  sound_button_.Render(r);

  text::DrawCentered(r, i18n::Get("menu.credits").c_str(), center_x,
                     ctx.logical_height - ui::Grid(3), kFooterScale,
                     colors::Get("text.secondary"));
}
