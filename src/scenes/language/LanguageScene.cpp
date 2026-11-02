#include "scenes/language/LanguageScene.h"

#include <string>

#include "scripts/colors/Colors.h"
#include "scripts/i18n/I18n.h"
#include "scripts/ui/Ui.h"

namespace {

// Mesma grade do menu (docs/menu.md): botões de 22 x 5 células, 2 células entre eles,
// centrados na tela.
constexpr float kButtonWidth = ui::Grid(22);
constexpr float kButtonHeight = ui::Grid(5);
constexpr float kButtonGap = ui::Grid(2);

}  // namespace

LanguageScene::LanguageScene() {
  for (const i18n::Language& language : i18n::Languages()) {
    buttons_.push_back(std::make_unique<ButtonLanguage>(language.code, language.name,
                                                        flag::FromCode(language.flag)));
  }
  // Sem translate.json: só português, para o jogo seguir para o menu.
  if (buttons_.empty()) {
    buttons_.push_back(std::make_unique<ButtonLanguage>("pt", "Português", flag::Country::kBrazil));
  }
}

void LanguageScene::OnEnter(SceneContext& ctx) {
  LayoutButtons(ctx);
  clouds_ready_ = clouds_.Init(ctx.renderer, ctx.logical_width, ctx.logical_height);
  // Foco inicial no idioma atual (padrão: português).
  focused_ = 0;
  for (size_t i = 0; i < buttons_.size(); ++i) {
    if (buttons_[i]->Code() == i18n::CurrentLanguage()) focused_ = static_cast<int>(i);
  }
  for (size_t i = 0; i < buttons_.size(); ++i) buttons_[i]->SetFocused(static_cast<int>(i) == focused_);
}

void LanguageScene::LayoutButtons(const SceneContext& ctx) {
  const float total = buttons_.size() * kButtonHeight + (buttons_.size() - 1) * kButtonGap;
  const float x = (ctx.logical_width - kButtonWidth) / 2.0f;
  float y = ui::SnapUp((ctx.logical_height - total) / 2.0f);
  for (auto& button : buttons_) {
    button->SetRect(SDL_FRect{x, y, kButtonWidth, kButtonHeight});
    y += kButtonHeight + kButtonGap;
  }
}

void LanguageScene::MoveFocus(int delta) {
  const int count = static_cast<int>(buttons_.size());
  focused_ = (focused_ + delta + count) % count;
  for (int i = 0; i < count; ++i) buttons_[i]->SetFocused(i == focused_);
}

void LanguageScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
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
        buttons_[focused_]->OnClick(ctx);
        return;
      case SDLK_ESCAPE:
        ctx.quit = true;
        return;
      default:
        break;
    }
  }
  for (size_t i = 0; i < buttons_.size(); ++i) {
    if (buttons_[i]->HandleEvent(event)) {
      focused_ = static_cast<int>(i);
      buttons_[i]->OnClick(ctx);
      return;
    }
  }
}

void LanguageScene::Update(SceneContext& /*ctx*/, float dt_seconds) {
  if (clouds_ready_) clouds_.Update(dt_seconds);
}

void LanguageScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color background = colors::Get("background.menu");
  SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
  SDL_RenderClear(r);
  if (clouds_ready_) clouds_.RenderBack(r);

  for (auto& button : buttons_) button->Render(r);
  if (clouds_ready_) clouds_.RenderFront(r);
}
