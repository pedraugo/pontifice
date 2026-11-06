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
    : ButtonText(std::move(name),
                 [code](SceneContext& ctx) {
                   i18n::SetLanguage(code);
                   ctx.next_scene = SceneId::kMenu;
                 }),
      code_(std::move(code)),
      country_(country) {}

float ButtonLanguage::ContentWidth() const {
  return kFlagWidth + kFlagGap + LabelWidth();
}

void ButtonLanguage::Render(SDL_Renderer* r) const {
  const SDL_FRect& rect = Rect();
  const float cy = rect.y + rect.h / 2.0f;
  const bool highlight = Highlighted();
  // Bandeira em preto e branco; ganha cor com hover/foco, junto com o texto.
  flag::Draw(r, country_, SDL_FRect{rect.x, cy - kFlagHeight / 2.0f, kFlagWidth, kFlagHeight},
             highlight ? 1.0f : 0.0f);
  const float text_w = LabelWidth();
  text::DrawCentered(r, Label().c_str(), rect.x + kFlagWidth + kFlagGap + text_w / 2.0f, cy,
                     LabelScale(),
                     colors::Get(highlight ? "buttonText.highlight" : "buttonText.text"));
}
