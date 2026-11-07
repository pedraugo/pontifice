#include "scenes/menu/menuScene.h"

#include <algorithm>
#include <random>
#include <string>

#include "scripts/ai/ai.h"
#include "scripts/button/buttonText.h"
#include "scripts/cards/cards.h"
#include "scripts/colors/colors.h"
#include "scripts/i18n/i18n.h"
#include "scripts/audios/music.h"
#include "scripts/opacity/opacity.h"
#include "scripts/save/save.h"
#include "scripts/text/text.h"
#include "scripts/layout/layout.h"
#include "scripts/ui/ui.h"

namespace {

// Transition into the loading screen, from assets/layout.json ("menu").
float FadeOut() { return layout::Number("menu.fadeOut", 0.5f); }
float LoadingFadeIn() { return layout::Number("menu.loadingFadeIn", 0.5f); }

std::string DifficultyLabel(int level) {
  const auto difficulty = static_cast<ai::Difficulty>(std::clamp(level, 0, 3));
  return i18n::Format("menu.level", {i18n::Get(ai::DifficultyKey(difficulty))});
}

// Player names: letters, digits, dot and underscore only.
bool NameChar(Uint32 ch) {
  return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
         ch == '.' || ch == '_';
}

std::string Trimmed(const std::string& text) {
  const size_t first = text.find_first_not_of(' ');
  if (first == std::string::npos) return "";
  return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

// Battle needs exactly deckSize cards chosen in the grimoire.
bool DeckReady(const SceneContext& ctx) {
  return ctx.save && static_cast<int>(ctx.save->deck.size()) == rules::Get().deck_size;
}

void OpenThroughLoading(SceneContext& ctx, SceneId target) {
  ctx.loading_target = target;
  ctx.fade_out_seconds = FadeOut();
  ctx.fade_in_seconds = LoadingFadeIn();
  ctx.next_scene = SceneId::kLoading;
}

}

MenuScene::MenuScene() {
  main_buttons_.push_back(
      std::make_unique<ButtonText>(i18n::Get("menu.play"), [this](SceneContext& ctx) {
        ShowPage(ctx, ctx.save ? MenuPage::kSave : MenuPage::kNewSave);
      }));
  main_buttons_.push_back(std::make_unique<ButtonText>(
      i18n::Get("menu.exit"), [](SceneContext& ctx) { ctx.quit = true; }));
  main_buttons_.back()->SetDanger(true);

  form_buttons_.push_back(std::make_unique<Button>(
      i18n::Get("general.menu"), [this](SceneContext& ctx) { ShowPage(ctx, MenuPage::kMain); }));
  form_buttons_.push_back(std::make_unique<Button>(
      i18n::Get("menu.save"), [this](SceneContext& ctx) { CreateSave(ctx); }));
  confirm_button_ = form_buttons_.back().get();
  name_input_.SetPlaceholder(i18n::Get("menu.namePlaceholder"));
  name_input_.SetMaxLength(static_cast<int>(ui::Number("menu.nameMaxLength", 16.0f)));
  name_input_.SetStyle(text::Styles({"large"}));
  name_input_.SetPadding(ui::Cells("menu.namePadding", 1.0f));
  name_input_.SetFilter(NameChar);

  save_buttons_.push_back(
      std::make_unique<ButtonText>(i18n::Get("menu.battle"), [](SceneContext& ctx) {
        if (DeckReady(ctx)) OpenThroughLoading(ctx, SceneId::kBattle);
      }));
  battle_button_ = save_buttons_.back().get();
  save_buttons_.push_back(std::make_unique<ButtonText>(
      DifficultyLabel(rules::Get().default_difficulty), [this](SceneContext& ctx) {
        ctx.difficulty = static_cast<int>(
            ai::NextDifficulty(static_cast<ai::Difficulty>(std::clamp(ctx.difficulty, 0, 3))));
        difficulty_button_->SetLabel(DifficultyLabel(ctx.difficulty));
        Layout(ctx);
        if (ctx.save) {
          ctx.save->difficulty = ctx.difficulty;
          save::Write(*ctx.save);
        }
      }));
  difficulty_button_ = save_buttons_.back().get();
  save_buttons_.push_back(std::make_unique<ButtonText>(
      i18n::Get("menu.grimoire"),
      [](SceneContext& ctx) { OpenThroughLoading(ctx, SceneId::kGrimoire); }));
  save_buttons_.push_back(std::make_unique<ButtonText>(
      i18n::Get("menu.exit"), [](SceneContext& ctx) { ctx.quit = true; }));
  save_buttons_.back()->SetDanger(true);
}

void MenuScene::OnEnter(SceneContext& ctx) {
  difficulty_button_->SetLabel(DifficultyLabel(ctx.difficulty));
  const float icon_size = sound_button_.Side();
  const float icon_margin = sound_button_.Margin();
  sound_button_.SetRect(SDL_FRect{icon_margin, ctx.logical_height - icon_margin - icon_size,
                                  icon_size, icon_size});
  ShowPage(ctx, ctx.menu_page == MenuPage::kSave && ctx.save ? MenuPage::kSave : MenuPage::kMain);
}

void MenuScene::OnExit(SceneContext& ctx) {
  SDL_StopTextInput(SDL_GetRenderWindow(ctx.renderer));
}

void MenuScene::ShowPage(SceneContext& ctx, MenuPage page) {
  page_ = page;
  ctx.menu_page = page;
  focused_ = -1;
  for (Buttons* buttons : {&main_buttons_, &form_buttons_, &save_buttons_}) {
    for (auto& button : *buttons) button->SetFocused(false);
  }
  const bool ready = DeckReady(ctx);
  const int deck_size = ctx.save ? static_cast<int>(ctx.save->deck.size()) : 0;
  battle_button_->SetLabel(
      ready ? i18n::Get("menu.battle")
            : i18n::Format("menu.battleIncomplete", {std::to_string(deck_size),
                                                      std::to_string(rules::Get().deck_size)}));
  battle_button_->SetAlpha(ready ? 1.0f : opacity::Get("disabled"));
  SDL_Window* window = SDL_GetRenderWindow(ctx.renderer);
  if (page == MenuPage::kNewSave) {
    name_input_.Clear();
    SDL_StartTextInput(window);
  } else {
    SDL_StopTextInput(window);
  }
  Layout(ctx);
}

MenuScene::Buttons& MenuScene::PageButtons() {
  switch (page_) {
    case MenuPage::kNewSave:
      return form_buttons_;
    case MenuPage::kSave:
      return save_buttons_;
    default:
      return main_buttons_;
  }
}

void MenuScene::Layout(const SceneContext& ctx) {
  if (page_ == MenuPage::kNewSave) {
    LayoutForm(ctx);
  } else {
    LayoutColumn(PageButtons(), ctx);
  }
}

void MenuScene::LayoutColumn(Buttons& buttons, const SceneContext& ctx) {
  const float row_h = buttons.front()->Measure("rowHeight", 4.0f);
  const float row_gap = buttons.front()->Measure("rowGap", 2.0f);
  const float total = buttons.size() * row_h + (buttons.size() - 1) * row_gap;
  float y = ui::SnapUp((ctx.logical_height - total) / 2.0f);
  for (auto& button : buttons) {
    const float w = button->LabelWidth();
    button->SetRect(SDL_FRect{(ctx.logical_width - w) / 2.0f, y, w, row_h});
    y += row_h + row_gap;
  }
}

// Name field centered, Menu under its left edge and Save under its right edge.
void MenuScene::LayoutForm(const SceneContext& ctx) {
  const float w = ui::Cells("menu.nameWidth", 30.0f);
  const float h = ui::Cells("menu.nameHeight", 4.0f);
  const float gap = ui::Cells("menu.nameGap", 2.0f);
  Button& back = *form_buttons_.front();
  const float button_h = back.Height();
  const float x = ui::SnapUp((ctx.logical_width - w) / 2.0f);
  const float y = ui::SnapUp((ctx.logical_height - (h + gap + button_h)) / 2.0f);
  name_input_.SetRect(SDL_FRect{x, y, w, h});
  const float back_w = back.PreferredWidth();
  const float confirm_w = confirm_button_->PreferredWidth();
  back.SetRect(SDL_FRect{x, y + h + gap, back_w, button_h});
  confirm_button_->SetRect(SDL_FRect{x + w - confirm_w, y + h + gap, confirm_w, button_h});
}

void MenuScene::MoveFocus(int delta) {
  Buttons& buttons = PageButtons();
  const int count = static_cast<int>(buttons.size());
  if (focused_ < 0) {
    focused_ = delta > 0 ? 0 : count - 1;
  } else {
    focused_ = (focused_ + delta + count) % count;
  }
  for (int i = 0; i < count; ++i) buttons[i]->SetFocused(i == focused_);
}

void MenuScene::CreateSave(SceneContext& ctx) {
  const std::string name = Trimmed(name_input_.Text());
  if (name.empty()) return;
  save::Data data;
  data.name = name;
  data.language = i18n::CurrentLanguage();
  data.difficulty = ctx.difficulty;
  std::mt19937 rng(std::random_device{}());
  data.deck = cards::StarterDeck(cards::LoadAll(BasePath()), rng);
  for (const std::string& code : data.deck) data.UnlockCard(code);
  save::Write(data);  // on failure the save still lasts for this session
  ctx.save = std::move(data);
  ShowPage(ctx, MenuPage::kSave);
}

void MenuScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  if (event.type == SDL_EVENT_KEY_DOWN) {
    const bool form = page_ == MenuPage::kNewSave;
    switch (event.key.key) {
      case SDLK_UP:
        if (!form) MoveFocus(-1);
        return;
      case SDLK_DOWN:
        if (!form) MoveFocus(+1);
        return;
      case SDLK_RETURN:
      case SDLK_KP_ENTER:
        if (form) {
          CreateSave(ctx);
        } else if (focused_ >= 0) {
          PageButtons()[focused_]->OnClick(ctx);
        }
        return;
      case SDLK_SPACE:
        if (!form && focused_ >= 0) PageButtons()[focused_]->OnClick(ctx);
        if (!form) return;
        break;
      case SDLK_ESCAPE:
        if (page_ == MenuPage::kMain) {
          ctx.quit = true;
        } else {
          ShowPage(ctx, MenuPage::kMain);
        }
        return;
      default:
        break;
    }
  }
  if (page_ == MenuPage::kNewSave && name_input_.HandleEvent(event)) return;

  if (sound_button_.HandleEvent(event)) {
    if (ctx.music != nullptr) ctx.music->ToggleMuted();
    return;
  }
  Buttons& buttons = PageButtons();
  for (size_t i = 0; i < buttons.size(); ++i) {
    if (buttons[i]->HandleEvent(event)) {
      focused_ = static_cast<int>(i);
      buttons[i]->OnClick(ctx);
      return;
    }
  }
}

void MenuScene::Update(SceneContext&, float dt_seconds) {
  if (page_ == MenuPage::kNewSave) name_input_.Update(dt_seconds);
}

void MenuScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color background = colors::Get("stoneDarker");
  SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
  SDL_RenderClear(r);
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);

  const float center_x = ctx.logical_width / 2.0f;
  static const text::Style title = text::Styles({"display"});
  static const text::Style footer = text::Styles({"regular"});
  text::DrawCentered(r, "Pontifice", center_x, ui::Cells("menu.titleCenterY", 12.0f), title,
                     colors::Get("silverLight"));

  if (page_ == MenuPage::kNewSave) {
    name_input_.Render(r);
    confirm_button_->SetAlpha(Trimmed(name_input_.Text()).empty() ? opacity::Get("disabled")
                                                                  : 1.0f);
  }
  for (auto& button : PageButtons()) button->Render(r);
  sound_button_.SetActive(ctx.music == nullptr || !ctx.music->Muted());
  sound_button_.Render(r);

  text::DrawCentered(r, i18n::Get("menu.credits").c_str(), center_x,
                     ctx.logical_height - ui::Cells("menu.footerBottom", 3.0f), footer,
                     colors::Get("steelLight"));
}
