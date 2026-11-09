#include "scenes/language/languageScene.h"

#include <algorithm>
#include <string>

#include "scripts/colors/colors.h"
#include "scripts/flag/flag.h"
#include "scripts/i18n/i18n.h"
#include "scripts/save/save.h"
#include "scripts/ui/ui.h"

LanguageScene::LanguageScene() {
  for (const i18n::Language& language : i18n::Languages()) {
    AddLanguage(language.code, language.name, flag::FromCode(language.flag));
  }
  // Without translate.json: Portuguese only, so the menu is reachable.
  if (buttons_.empty()) AddLanguage("pt", "Português", flag::Country::kBrazil);
}

void LanguageScene::AddLanguage(const std::string& code, const std::string& name,
                                flag::Country country) {
  auto button = std::make_unique<ButtonText>(name, [code](SceneContext& ctx) {
    i18n::SetLanguage(code);
    if (ctx.save && ctx.save->language != code) {
      ctx.save->language = code;
      save::Write(*ctx.save);
    }
    ctx.next_scene = SceneId::kMenu;
  });
  button->SetFlag(country);
  buttons_.push_back(std::move(button));
}

void LanguageScene::OnEnter(SceneContext& ctx) {
  LayoutButtons(ctx);
  focused_ = -1;
  for (auto& button : buttons_) button->SetFocused(false);
}

void LanguageScene::OnExit(SceneContext& ctx) { ctx.language_from_menu = false; }

void LanguageScene::LayoutButtons(const SceneContext& ctx) {
  float width = 0.0f;
  for (const auto& button : buttons_) width = std::max(width, button->ContentWidth());
  const float row_h = buttons_.front()->Measure("rowHeight", 4.0f);
  const float row_gap = buttons_.front()->Measure("rowGap", 2.0f);
  const float total = buttons_.size() * row_h + (buttons_.size() - 1) * row_gap;
  const float x = ui::SnapUp((ctx.logical_width - width) / 2.0f);
  float y = ui::SnapUp((ctx.logical_height - total) / 2.0f);
  for (auto& button : buttons_) {
    button->SetRect(SDL_FRect{x, y, width, row_h});
    y += row_h + row_gap;
  }
}

void LanguageScene::MoveFocus(int delta) {
  const int count = static_cast<int>(buttons_.size());
  if (focused_ < 0) {
    focused_ = delta > 0 ? 0 : count - 1;
  } else {
    focused_ = (focused_ + delta + count) % count;
  }
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
        if (focused_ >= 0) buttons_[focused_]->OnClick(ctx);
        return;
      case SDLK_ESCAPE:
        if (ctx.language_from_menu) {
          ctx.next_scene = SceneId::kMenu;
        } else {
          ctx.quit = true;
        }
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

void LanguageScene::Update(SceneContext&, float) {}

void LanguageScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color background = colors::Get("stoneDarker");
  SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
  SDL_RenderClear(r);
}

void LanguageScene::RenderHud(SceneContext& ctx) {
  for (auto& button : buttons_) button->Render(ctx.renderer);
}
