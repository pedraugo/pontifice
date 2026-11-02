#include "scripts/button/ButtonLanguage.h"

#include <utility>

#include "scenes/Scene.h"
#include "scripts/colors/Colors.h"
#include "scripts/i18n/I18n.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

constexpr float kFlagHeight = ui::Grid(2.5f);        // 40 px
constexpr float kFlagWidth = kFlagHeight * 1.5f;     // 3:2
constexpr float kFlagGap = ui::Grid(1);

}  // namespace

ButtonLanguage::ButtonLanguage(std::string code, std::string name, flag::Country country)
    : Button(std::move(name)), code_(std::move(code)), country_(country) {}

void ButtonLanguage::OnClick(SceneContext& ctx) {
  i18n::SetLanguage(code_);
  ctx.next_scene = SceneId::kMenu;
}

void ButtonLanguage::RenderContent(SDL_Renderer* r) const {
  const SDL_FRect& rect = Rect();
  const float text_w = text::Width(Label().c_str(), LabelScale());
  const float total = kFlagWidth + kFlagGap + text_w;
  const float left = rect.x + (rect.w - total) / 2.0f;
  const float cy = rect.y + rect.h / 2.0f;
  flag::Draw(r, country_, SDL_FRect{left, cy - kFlagHeight / 2.0f, kFlagWidth, kFlagHeight});
  text::DrawCentered(r, Label().c_str(), left + kFlagWidth + kFlagGap + text_w / 2.0f, cy,
                     LabelScale(), colors::Get("button.text"));
}
