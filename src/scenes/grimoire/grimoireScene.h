#pragma once

#include <SDL3/SDL.h>

#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "scenes/scene.h"
#include "scripts/aura/cardAura.h"
#include "scripts/button/button.h"
#include "scripts/cards/cards.h"
#include "scripts/gpu/gpu.h"
#include "scripts/hand/hand.h"
#include "scripts/image/image.h"
#include "scripts/math/math.h"
#include "scripts/model/model.h"

// Card gallery (docs/grimorio.md): category filters, 4x2 pages of card fronts, back to menu.
class GrimoireScene : public Scene {
 public:
  GrimoireScene();
  ~GrimoireScene() override;

  void Preload(const PreloadInfo& info) override;
  void OnEnter(SceneContext& ctx) override;
  void OnExit(SceneContext& ctx) override;
  void HandleEvent(SceneContext& ctx, const SDL_Event& event) override;
  void Update(SceneContext& ctx, float dt_seconds) override;
  void Render(SceneContext& ctx) override;
  void RenderHud(SceneContext& ctx) override;

 private:
  static constexpr int kCategories = 4;  // G, E, N, F
  static constexpr char kCategoryCodes[kCategories] = {'G', 'E', 'N', 'F'};

  void Layout(const SceneContext& ctx);
  void Refilter();
  void ChangePage(int delta);
  int PageCount() const;
  SDL_Texture* FaceFor(SDL_Renderer* r, const CardInfo& card);
  SDL_FRect CardRect(int index) const;
  SDL_FRect PageCardRect(int index, float offset_x, float scale) const;
  void RenderPage(SDL_Renderer* r, int page, float offset_x, float alpha, float scale,
                  bool with_hover);
  // 3D path (with gpu_): the same page drawn as lit card models, foil on hierarchy 1.
  SDL_GPUTexture* GpuFaceFor(const CardInfo& card);
  math::Mat4 PoseForRect(const SDL_FRect& rect, int width, int height, float z,
                         float yaw = 0.0f, float pitch = 0.0f) const;
  void RenderPage3D(int page, float offset_x, float alpha, float scale, bool with_hover,
                    int width, int height);
  void RenderPages(SDL_Renderer* r, const SceneContext& ctx);
  void UpdateInspectFront();
  void OpenInspect(int index, const SceneContext& ctx);
  void EndInspect();
  bool Unlocked(const CardInfo& card) const { return unlocked_.count(card.code) > 0; }
  // Unlocked cards use their own front; the rest share the hidden one (kHiddenFace).
  const std::string& FaceKey(const CardInfo& card) const;
  // Deck building: over the bottom of each unlocked card a strip with its unlocked copies (left)
  // and a field with its copies in the battle deck (right); a click selects, digits edit.
  SDL_FRect StripRect(const SDL_FRect& card) const;
  SDL_FRect CountRect(const SDL_FRect& strip) const;
  int SlotAt(float x, float y) const;  // card index on the page, -1 when none
  void SetCopies(SceneContext& ctx, int copies);
  void RenderDeckBuilder(SDL_Renderer* r, const SceneContext& ctx) const;  // deck total
  void RenderCardOverlay(SDL_Renderer* r, const SceneContext& ctx) const;

  bool preloaded_ = false;
  std::vector<CardInfo> cards_;
  std::unordered_set<std::string> unlocked_;  // codes the save has unlocked
  std::unordered_map<std::string, Image> faces_;  // composed fronts, half size, by FaceKey
  std::unordered_map<std::string, SDL_Texture*> textures_;  // 2D fallback without gpu_
  std::unordered_map<std::string, SDL_GPUTexture*> gpu_faces_;
  GpuMesh veil_quad_;
  std::vector<const CardInfo*> visible_;  // after the category filter
  bool selected_[kCategories] = {true, true, true, true};
  std::unique_ptr<Button> filters_[kCategories];
  std::unique_ptr<Button> back_;
  std::unique_ptr<Button> forward_;
  std::unique_ptr<Button> exit_;
  int page_ = 0;
  const CardInfo* editing_ = nullptr;  // selected card: the keyboard edits its copies
  bool typed_ = false;  // after selecting, the first digit replaces the count
  int hovered_ = -1;  // card index on the page under the mouse
  // The last hovered card keeps easing (grow, lift, tilt) after the mouse leaves it.
  int lift_card_ = -1;
  float hover_t_ = 0.0f;  // 0 = resting, 1 = fully raised
  float tilt_x_ = 0.0f;  // leans toward the mouse, like the battle hand
  float tilt_y_ = 0.0f;
  // Page slide: the old page leaves and the new one enters from the side of the arrow.
  int slide_from_ = -1;  // page leaving; -1 when idle
  int slide_dir_ = 1;  // +1 forward (new page comes from the right), -1 back
  float slide_t_ = 0.0f;
  // Double-click inspect, like the battle hand: a one-card 3D Hand rises from below the screen
  // to the center; drag rotates, click flips, wheel zooms. The front starts at half size and
  // is swapped for the full-resolution one composed on another thread.
  std::unique_ptr<Gpu> gpu_;
  std::unique_ptr<Model> card_model_;
  CardAura card_aura_;
  SDL_GPUTexture* card_back_ = nullptr;
  SDL_GPUTexture* front_normal_ = nullptr;
  SDL_GPUTexture* back_normal_ = nullptr;
  Hand hand_;
  math::Mat4 hand_view_proj_;
  Atmosphere hand_atmosphere_;
  math::Vec2 mouse_{-1.0f, -1.0f};
  const CardInfo* inspect_ = nullptr;
  SDL_GPUTexture* inspect_front_ = nullptr;
  std::future<Image> inspect_full_;
  SDL_FRect grid_{};
  float card_w_ = 0.0f;
  float card_h_ = 0.0f;
  float gap_ = 0.0f;
};
