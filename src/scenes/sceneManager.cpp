#include "scenes/sceneManager.h"

#include <utility>

#include "scenes/battle/battleScene.h"
#include "scenes/grimoire/grimoireScene.h"
#include "scenes/language/languageScene.h"
#include "scenes/loading/loadingScene.h"
#include "scenes/menu/menuScene.h"
#include "scripts/audios/music.h"

SceneManager::SceneManager(SceneContext ctx) : ctx_(std::move(ctx)) {}

void SceneManager::SwitchTo(SceneId id) {
  if (current_) current_->OnExit(ctx_);
  ctx_.next_scene.reset();
  if (ctx_.music != nullptr) {
    const bool battle = id == SceneId::kBattle;
    ctx_.music->SetScene(battle ? "battle" : "menu");
    ctx_.music->SetCategory(battle ? "battle" : "menu");
  }
  fade_in_total_ = fade_in_left_ = ctx_.fade_in_seconds;
  ctx_.fade_in_seconds = 0.0f;
  fade_out_total_ = fade_out_left_ = 0.0f;
  ctx_.fade_out_seconds = 0.0f;
  if (ctx_.preloaded) {
    current_ = std::move(ctx_.preloaded);  // OnEnter already ran in the loading scene
    return;
  }
  current_ = Create(id);
  current_->OnEnter(ctx_);
}

void SceneManager::HandleEvent(const SDL_Event& event) {
  if (fade_out_total_ > 0.0f) return;  // no input during fade out
  if (current_) current_->HandleEvent(ctx_, event);
  ApplyPendingSwitch();
}

void SceneManager::Update(float dt_seconds) {
  if (fade_in_left_ > 0.0f) fade_in_left_ -= dt_seconds;
  if (fade_out_total_ > 0.0f) fade_out_left_ -= dt_seconds;
  if (current_) current_->Update(ctx_, dt_seconds);
  ApplyPendingSwitch();
}

void SceneManager::Render() {
  if (current_) current_->Render(ctx_);
  float alpha = 0.0f;
  if (fade_out_total_ > 0.0f) {
    alpha = 1.0f - SDL_max(fade_out_left_, 0.0f) / fade_out_total_;
  } else if (fade_in_left_ > 0.0f && fade_in_total_ > 0.0f) {
    alpha = fade_in_left_ / fade_in_total_;
  }
  if (alpha > 0.0f) {
    SDL_SetRenderDrawBlendMode(ctx_.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ctx_.renderer, 0, 0, 0, static_cast<Uint8>(alpha * 255.0f));
    const SDL_FRect full{0, 0, static_cast<float>(ctx_.logical_width),
                         static_cast<float>(ctx_.logical_height)};
    SDL_RenderFillRect(ctx_.renderer, &full);
  }
}

void SceneManager::ApplyPendingSwitch() {
  if (!ctx_.next_scene.has_value()) return;
  if (fade_out_total_ <= 0.0f && ctx_.fade_out_seconds > 0.0f) {
    fade_out_total_ = fade_out_left_ = ctx_.fade_out_seconds;
    ctx_.fade_out_seconds = 0.0f;
  }
  if (fade_out_total_ > 0.0f && fade_out_left_ > 0.0f) return;
  SwitchTo(*ctx_.next_scene);
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
    case SceneId::kGrimoire:
      return std::make_unique<GrimoireScene>();
  }
  return std::make_unique<MenuScene>();
}
