#include "scenes/SceneManager.h"

#include "scenes/BattleScene.h"
#include "scenes/MenuScene.h"

SceneManager::SceneManager(SceneContext ctx) : ctx_(ctx) {}

void SceneManager::SwitchTo(SceneId id) {
  if (current_) current_->OnExit(ctx_);
  current_ = Create(id);
  ctx_.next_scene.reset();
  current_->OnEnter(ctx_);
}

void SceneManager::HandleEvent(const SDL_Event& event) {
  if (current_) current_->HandleEvent(ctx_, event);
  ApplyPendingSwitch();
}

void SceneManager::Update(float dt_seconds) {
  if (current_) current_->Update(ctx_, dt_seconds);
  ApplyPendingSwitch();
}

void SceneManager::Render() {
  if (current_) current_->Render(ctx_);
}

void SceneManager::ApplyPendingSwitch() {
  if (ctx_.next_scene.has_value()) SwitchTo(*ctx_.next_scene);
}

std::unique_ptr<Scene> SceneManager::Create(SceneId id) {
  switch (id) {
    case SceneId::kMenu:
      return std::make_unique<MenuScene>();
    case SceneId::kBattle:
      return std::make_unique<BattleScene>();
  }
  return std::make_unique<MenuScene>();
}
