#include "scripts/board/Board.h"

#include <algorithm>
#include <cmath>

#include "scripts/colors/Colors.h"

namespace {

float SmoothStep(float t) {
  t = math::Clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

}  // namespace

bool Board::Init(Gpu& gpu) {
  // Quad horizontal de lado 1 em y = 0, mesma ordem de vértices da névoa da batalha.
  highlight_quad_ = gpu.CreateMesh({{-0.5f, 0, -0.5f, 0, 0},
                                    {0.5f, 0, -0.5f, 1, 0},
                                    {0.5f, 0, 0.5f, 1, 1},
                                    {-0.5f, 0, 0.5f, 0, 1}},
                                   {0, 1, 2, 0, 2, 3});
  // Moldura: quatro tiras em volta de um quadrado de lado 1 (anel externo de lado 1).
  const float o = 0.5f;
  const float i = 0.5f - layout_.frame_thickness;
  std::vector<Vertex> v;
  std::vector<Uint32> idx;
  auto strip = [&](float x0, float z0, float x1, float z1) {
    const Uint32 b = static_cast<Uint32>(v.size());
    v.push_back({x0, 0, z0, 0, 0});
    v.push_back({x1, 0, z0, 1, 0});
    v.push_back({x1, 0, z1, 1, 1});
    v.push_back({x0, 0, z1, 0, 1});
    idx.insert(idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
  };
  strip(-o, -o, o, -i);  // fundo
  strip(-o, i, o, o);    // frente
  strip(-o, -i, -i, i);  // esquerda
  strip(i, -i, o, i);    // direita
  frame_quad_ = gpu.CreateMesh(v, idx);
  return highlight_quad_.index_count > 0 && frame_quad_.index_count > 0;
}

void Board::Destroy(Gpu& gpu) {
  gpu.DestroyMesh(highlight_quad_);
  gpu.DestroyMesh(frame_quad_);
}

math::Vec3 Board::SlotPosition(int slot) const {
  const int col = slot % kCols;
  const int row = slot / kCols;  // 0 = linha A (junto da canaleta), 1 = linha B
  const float s = layout_.arena_scale;
  // Do ponto de vista do dono as colunas vão da esquerda para a direita; no lado do fundo
  // isso espelha x.
  const float x = (static_cast<float>(col) - (kCols - 1) / 2.0f) * layout_.tile * side_;
  const float z = (layout_.channel_half + (static_cast<float>(row) + 0.5f) * layout_.tile) * side_;
  return {x * s, layout_.top_y * s, z * s};
}

math::Vec3 Board::CardPosition(int slot) const {
  const math::Vec3 p = SlotPosition(slot);
  const float zmin = model_ != nullptr ? model_->BoundsMin().z : 0.0f;
  return {p.x, p.y - zmin * layout_.card_scale, p.z};
}

int Board::Count() const {
  int n = 0;
  for (const Slot& slot : cards_) n += slot.occupied ? 1 : 0;
  return n;
}

std::vector<int> Board::Occupied() const {
  std::vector<int> out;
  for (int slot = 0; slot < kSlots; ++slot) {
    if (cards_[slot].occupied) out.push_back(slot);
  }
  return out;
}

std::vector<int> Board::Free() const {
  std::vector<int> out;
  for (int slot = 0; slot < kSlots; ++slot) {
    if (!cards_[slot].occupied) out.push_back(slot);
  }
  return out;
}

void Board::ResetAttacks() {
  for (Slot& s : cards_) s.card.attacked = false;
}

void Board::Lunge(int slot, const math::Vec3& target) {
  if (Empty(slot)) return;
  cards_[slot].card.lunge = 0.0f;
  cards_[slot].card.lunge_target = target;
}

bool Board::Animating() const {
  if (!leaving_.empty()) return true;
  for (const Slot& s : cards_) {
    if (s.occupied && (s.card.place < 1.0f || s.card.lunge < 1.0f)) return true;
  }
  return false;
}

int Board::PickSlot(math::Vec2 mouse, const math::Mat4& view_proj, int width,
                    int height) const {
  if (mouse.x < 0 || mouse.y < 0) return -1;
  const float half = layout_.tile * layout_.arena_scale * 0.5f;
  for (int slot = 0; slot < kSlots; ++slot) {
    const math::Vec3 c = SlotPosition(slot);
    const math::Vec3 corners[4] = {{c.x - half, c.y, c.z - half},
                                   {c.x + half, c.y, c.z - half},
                                   {c.x + half, c.y, c.z + half},
                                   {c.x - half, c.y, c.z + half}};
    math::Vec2 screen[4];
    bool behind = false;
    for (int k = 0; k < 4; ++k) {
      const math::Vec4 clip =
          view_proj * math::Vec4{corners[k].x, corners[k].y, corners[k].z, 1.0f};
      if (clip.w <= 0.0f) {
        behind = true;
        break;
      }
      screen[k] = {(clip.x / clip.w * 0.5f + 0.5f) * width,
                   (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * height};
    }
    if (!behind && math::PointInConvexQuad(mouse, screen)) return slot;
  }
  return -1;
}

bool Board::Place(int slot, SDL_GPUTexture* front, bool foil, bool defense,
                  const CardInfo* info, int turn) {
  if (!Empty(slot)) return false;
  Slot& s = cards_[slot];
  s.occupied = true;
  s.card = Card{};
  s.card.front = front;
  s.card.foil = foil;
  s.card.info = info;
  s.card.summoned_turn = turn;
  s.card.defense = defense;
  s.card.defense_t = defense ? 1.0f : 0.0f;
  s.card.place = 0.0f;
  s.card.world = CardMatrix(slot, s.card);
  return true;
}

bool Board::Discard(int slot) {
  if (Empty(slot)) return false;
  Slot& s = cards_[slot];
  s.card.place = 1.0f;
  s.card.discard = 0.0f;
  leaving_.push_back(Leaving{slot, s.card});
  s.occupied = false;
  s.card = Card{};
  return true;
}

bool Board::TogglePosition(int slot) {
  if (Empty(slot)) return false;
  cards_[slot].card.defense = !cards_[slot].card.defense;
  return true;
}

math::Mat4 Board::CardMatrix(int slot, const Card& card) const {
  const float t = SmoothStep(card.place);
  const math::Vec3 target = SlotPosition(slot);
  // Deitada: RotateX(-pi/2) leva a frente (+z do modelo) para cima (+y) e o topo da carta
  // (+y) para longe do jogador (-z). A borda de trás do modelo fica em BoundsMin().z.
  const float zmin = model_ != nullptr ? model_->BoundsMin().z : 0.0f;
  // Descarte: sobe e encolhe a partir do lugar.
  const float d = SmoothStep(card.discard);
  const float hover = SmoothStep(card.hover);
  const float scale = layout_.card_scale * math::Lerp(layout_.place_scale, 1.0f, t) *
                      (1.0f - d) * math::Lerp(1.0f, layout_.hover_scale, hover);
  const float lift = -zmin * scale + 0.004f + layout_.discard_rise * d +
                     layout_.hover_lift * hover;
  const math::Vec3 from =
      target + math::Vec3{layout_.place_from.x, layout_.place_from.y, layout_.place_from.z * side_};
  const math::Vec3 rest = target + math::Vec3{0.0f, lift, 0.0f};
  math::Vec3 pos = from + (rest - from) * t;
  // Investida: vai em arco até perto do alvo e volta (seno de 0 a pi).
  if (card.lunge < 1.0f) {
    const float l = std::sin(card.lunge * math::kPi);
    const math::Vec3 to = card.lunge_target - rest;
    pos = pos + to * (layout_.lunge_reach * l) + math::Vec3{0.0f, layout_.lunge_height * l, 0.0f};
  }
  const float tilt = math::Lerp(layout_.place_tilt, -math::kPi / 2.0f, t);
  math::Mat4 m = math::Mat4::Translate(pos);
  // Lado do fundo: a carta fica de pé para o seu dono (meia-volta em torno do eixo vertical).
  if (side_ < 0) m = m * math::Mat4::RotateY(math::kPi);
  // Defesa: giro de 90° no plano da mesa (em torno do eixo vertical), animado.
  m = m * math::Mat4::RotateY(card.defense_t * math::kPi / 2.0f);
  m = m * math::Mat4::RotateX(tilt);
  return m * math::Mat4::Scale(scale);
}

Model::Skin Board::SkinFor(const Card& card) const {
  return Model::Skin{card.front, back_, front_normal_, back_normal_,
                     card.foil ? Gpu::DrawMode::kFoil : Gpu::DrawMode::kMesh};
}

void Board::Update(float dt) {
  time_ += dt;
  const float k = 1.0f - std::exp(-layout_.position_speed * dt);
  for (int slot = 0; slot < kSlots; ++slot) {
    Slot& s = cards_[slot];
    if (!s.occupied) continue;
    if (s.card.place < 1.0f) {
      s.card.place = std::min(1.0f, s.card.place + dt / layout_.place_seconds);
    }
    s.card.defense_t = math::Lerp(s.card.defense_t, s.card.defense ? 1.0f : 0.0f, k);
    const float hover_target = slot == hover_card_ ? 1.0f : 0.0f;
    s.card.hover += (hover_target - s.card.hover) * (1.0f - std::exp(-layout_.hover_speed * dt));
    if (s.card.lunge < 1.0f) {
      s.card.lunge = std::min(1.0f, s.card.lunge + dt / layout_.lunge_seconds);
    }
    s.card.world = CardMatrix(slot, s.card);
  }
  for (Leaving& l : leaving_) {
    l.card.discard = std::min(1.0f, l.card.discard + dt / layout_.discard_seconds);
    l.card.world = CardMatrix(l.slot, l.card);
  }
  leaving_.erase(std::remove_if(leaving_.begin(), leaving_.end(),
                                [](const Leaving& l) { return l.card.discard >= 1.0f; }),
                 leaving_.end());
}

void Board::DrawCard(Gpu& gpu, const math::Mat4& view_proj, const Card& card) const {
  model_->Draw(gpu, card.world, view_proj, SDL_FColor{1, 1, 1, 1}, SkinFor(card));
}

void Board::Render(Gpu& gpu, const math::Mat4& view_proj) const {
  if (model_ != nullptr) {
    // A carta sob o mouse (ampliada) por último, para ficar por cima das vizinhas.
    for (int slot = 0; slot < kSlots; ++slot) {
      if (cards_[slot].occupied && slot != hover_card_) DrawCard(gpu, view_proj, cards_[slot].card);
    }
    for (const Leaving& l : leaving_) DrawCard(gpu, view_proj, l.card);
    if (hover_card_ >= 0 && hover_card_ < kSlots && cards_[hover_card_].occupied) {
      DrawCard(gpu, view_proj, cards_[hover_card_].card);
    }
  }
  if (highlight_quad_.index_count == 0) return;
  // Modo de invocação: slots vazios com quad translúcido um pouco acima do ladrilho,
  // pulsando. O slot sob o mouse (vazio ou com carta) recebe uma moldura quadrada amarela.
  // Cores de assets/colors.json ("board.*").
  const float side = layout_.tile * layout_.arena_scale * layout_.highlight_inset;
  const float y = layout_.highlight_lift;
  if (highlight_) {
    const float pulse = 0.75f + 0.25f * std::sin(time_ * layout_.pulse_speed);
    SDL_FColor slot_color = colors::GetF("board.slot");
    slot_color.a *= pulse;
    for (int slot = 0; slot < kSlots; ++slot) {
      if (!Empty(slot)) continue;
      const math::Vec3 p = SlotPosition(slot);
      gpu.Draw(highlight_quad_, nullptr,
               math::Mat4::Translate({p.x, p.y + y, p.z}) *
                   math::Mat4::Scale({side, 1.0f, side}),
               view_proj, slot_color, Gpu::DrawMode::kEmissive);
    }
  }
  if (hover_slot_ >= 0 && hover_slot_ < kSlots) {
    const math::Vec3 p = SlotPosition(hover_slot_);
    gpu.Draw(frame_quad_, nullptr,
             math::Mat4::Translate({p.x, p.y + y * 1.5f, p.z}) *
                 math::Mat4::Scale({side, 1.0f, side}),
             view_proj, colors::GetF("board.slotHover"), Gpu::DrawMode::kEmissive);
  }
}

void Board::RenderDepth(Gpu& gpu) const {
  if (model_ == nullptr) return;
  for (const Slot& s : cards_) {
    if (s.occupied) model_->DrawDepth(gpu, s.card.world);
  }
  for (const Leaving& l : leaving_) model_->DrawDepth(gpu, l.card.world);
}
