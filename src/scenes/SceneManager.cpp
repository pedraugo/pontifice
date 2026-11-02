#include "scenes/SceneManager.h"

#include <utility>

#include "scenes/battle/BattleScene.h"
#include "scenes/language/LanguageScene.h"
#include "scenes/loading/LoadingScene.h"
#include "scenes/menu/MenuScene.h"
#include "scripts/music/Music.h"

SceneManager::SceneManager(SceneContext ctx) : ctx_(std::move(ctx)) {}

void SceneManager::SwitchTo(SceneId id) {
  if (current_) current_->OnExit(ctx_);
  ctx_.next_scene.reset();
  // Trilha por cena (assets/ost.json): menu e loading tocam a categoria "ost" no volume do
  // menu (a cena de idioma também); a batalha troca para "battle" (fade out/fade in) no
  // volume da batalha.
  if (ctx_.music != nullptr) {
    const bool battle = id == SceneId::kBattle;
    ctx_.music->SetScene(battle ? "battle" : "menu");
    ctx_.music->SetCategory(battle ? "battle" : "ost");
  }
  fade_in_total_ = fade_in_left_ = ctx_.fade_in_seconds;
  ctx_.fade_in_seconds = 0.0f;
  if (ctx_.preloaded) {
    current_ = std::move(ctx_.preloaded);  // já passou por OnEnter na tela de loading
    return;
  }
  current_ = Create(id);
  current_->OnEnter(ctx_);
}

void SceneManager::HandleEvent(const SDL_Event& event) {
  if (current_) current_->HandleEvent(ctx_, event);
  ApplyPendingSwitch();
}

void SceneManager::Update(float dt_seconds) {
  if (fade_in_left_ > 0.0f) fade_in_left_ -= dt_seconds;
  if (current_) current_->Update(ctx_, dt_seconds);
  ApplyPendingSwitch();
}

void SceneManager::Render() {
  if (current_) current_->Render(ctx_);
  if (fade_in_left_ > 0.0f && fade_in_total_ > 0.0f) {
    const float alpha = fade_in_left_ / fade_in_total_;
    SDL_SetRenderDrawBlendMode(ctx_.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ctx_.renderer, 0, 0, 0, static_cast<Uint8>(alpha * 255.0f));
    const SDL_FRect full{0, 0, static_cast<float>(ctx_.logical_width),
                         static_cast<float>(ctx_.logical_height)};
    SDL_RenderFillRect(ctx_.renderer, &full);
  }
}

void SceneManager::ApplyPendingSwitch() {
  if (ctx_.next_scene.has_value()) SwitchTo(*ctx_.next_scene);
}

std::unique_ptr<Scene> SceneManager::Create(SceneId id) {
  switch (id) {
    case SceneId::kLanguage:
      return std::make_unique<LanguageScene>();
    case SceneId::kMenu:
      return std::make_unique<MenuScene>();
    case SceneId::kLoading:
      return std::make_unique<LoadingScene>();
    case SceneId::kBattle:
      return std::make_unique<BattleScene>();
  }
  return std::make_unique<MenuScene>();
}
