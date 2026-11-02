#include "scenes/battle/BattleScene.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

#include "scripts/cardface/CardFace.h"
#include "scripts/colors/Colors.h"
#include "scripts/config/Config.h"
#include "scripts/frame/Frame.h"
#include "scripts/i18n/I18n.h"
#include "scripts/image/Image.h"
#include "scripts/music/Music.h"
#include "scripts/text/Text.h"
#include "scripts/ui/Ui.h"

namespace {

std::string BasePath() {
  const char* base = SDL_GetBasePath();
  return base != nullptr ? std::string(base) : std::string("./");
}

// A arena é um prisma trapezoidal: topo 1,9 x 1,9 (tabuleiro), base 2,5 x 2,5, altura 0,36 unidades no
// modelo (um ladrilho); escalada para caber na câmera.
constexpr float kArenaScale = 4.2f;
// Alturas da arena no mundo (bounds do glb y[-0.061, 0.3] vezes a escala).
constexpr float kArenaBaseY = -0.061f * kArenaScale;
constexpr float kArenaTopY = 0.3f * kArenaScale;

// Camadas de névoa (altura acima da base da arena e opacidade), de assets/Fog.json:
// battle.mist.layers (base, sempre) e battle.introMist.layers (manto da abertura).
std::vector<BattleScene::MistLayer> LoadMistLayers(const char* path) {
  std::vector<BattleScene::MistLayer> layers;
  const json::Value* list = config::Node("fog", path);
  if (list != nullptr && list->IsArray()) {
    for (const json::Value& item : list->array) {
      if (!item.IsObject()) continue;
      layers.push_back({kArenaBaseY + config::Float(item, "y", 0.0f),
                        config::Float(item, "alpha", 0.5f)});
    }
  }
  return layers;
}

// Tochas fincadas no chão de cada lado da arena; são a única luz da arena.
constexpr float kTorchX = 6.4f;
constexpr float kTorchZ = -1.0f;
constexpr float kTorchScale = 2.6f;
constexpr float kTorchTilt = 0.10f;   // rad, levemente inclinadas para fora

// Abertura da batalha: a arena sobe de dentro de uma névoa densa enquanto a câmera se
// aproxima. A névoa extra fica em alturas fixas do mundo, por isso a arena "fura" camada por
// camada ao subir, e vai rareando até restarem só as camadas da base.
constexpr float kIntroSeconds = 1.6f;
constexpr float kIntroDrop = 3.2f;                     // quanto a arena começa abaixo do lugar
// Tochas emergem depois da arena: começam a subir quando ela já está quase no lugar.
constexpr float kTorchIntroStart = 1.1f;   // s depois da entrada
constexpr float kTorchIntroSeconds = 1.2f;
constexpr float kTorchIntroDrop = 4.5f;    // começam bem abaixo do chão
constexpr float kIgniteSeconds = 0.6f;     // o fogo acende depois de a tocha ter surgido
constexpr float kMatchStart = 3.2f;        // s: sorteio e primeiro turno, com as tochas acesas
constexpr math::Vec3 kIntroEyeStart{0.0f, 16.0f, 22.0f};  // câmera longe e alta
constexpr math::Vec3 kArenaTarget{0.0f, -0.1f, -0.3f};

// Virada de campo (docs/detalhes_tecnicos.md): a câmera orbita 180° em torno do centro da
// arena no turno do computador e volta no seu.
constexpr float kOrbitSeconds = 1.0f;

// Layout 2D na grade de 16 px (ui::Grid). Botão "Passar turno": à direita, acima do painel
// do cemitério, fora da mão.
constexpr float kButtonWidth = ui::Grid(21);   // 336 px
constexpr float kButtonHeight = ui::Grid(5);   // 80 px
constexpr float kButtonMargin = ui::Grid(1);
constexpr float kButtonBottom = ui::Grid(14);  // do rodapé: cemitério (3) + folga
constexpr float kHintScale = 2.0f * ui::kScale;
// Botão de som: 3 x 3 células no canto inferior esquerdo; o painel do monte fica à direita.
constexpr float kSoundButtonSize = ui::Grid(3);
constexpr float kSoundButtonGap = ui::Grid(1);
// Tooltip da carta no campo: imagem da carta à esquerda, textos à direita.
constexpr float kTipScale = 2.0f * ui::kScale;        // 24 px
constexpr float kTipDetailScale = 1.5f * ui::kScale;  // 18 px
constexpr float kTipPadding = ui::Grid(1);
constexpr float kTipGap = ui::Grid(0.5f);
constexpr float kTipOffset = ui::Grid(1);     // distância do cursor
constexpr float kTipImageH = ui::Grid(14);    // altura da frente da carta (224 px)
constexpr float kTipTextW = ui::Grid(24);     // largura da coluna de texto (quebra da descrição)
// Topo: linha de situação e dica (centros em y).
constexpr float kStatusY = ui::Grid(2);
constexpr float kHintY = ui::Grid(4);
constexpr float kCardAspect = 1600.0f / 2230.0f;

// Quebra `str` em linhas de até `max_w` px na escala dada (por palavras).
std::vector<std::string> WrapText(const std::string& str, float max_w, float scale) {
  std::vector<std::string> lines;
  std::string line;
  size_t pos = 0;
  while (pos < str.size()) {
    size_t end = str.find(' ', pos);
    if (end == std::string::npos) end = str.size();
    const std::string word = str.substr(pos, end - pos);
    const std::string candidate = line.empty() ? word : line + " " + word;
    if (!line.empty() && text::Width(candidate.c_str(), scale) > max_w) {
      lines.push_back(line);
      line = word;
    } else {
      line = candidate;
    }
    pos = end + 1;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}
constexpr float kStatusScale = 2.0f * ui::kScale;
constexpr float kClickSlop = 6.0f;      // px lógicos: até aqui um arrasto conta como clique
constexpr float kAttackDragPx = 24.0f;  // px lógicos de arrasto para virar ataque
constexpr int kHandLimit = 6;
// Pausas do turno do computador (s), para o jogador acompanhar cada jogada.
constexpr float kEnemyPause = 0.7f;
constexpr float kAttackHit = 0.25f;  // momento do impacto dentro da investida

float SmoothStep(float t) {
  t = math::Clamp(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

const char* SlotName(int slot, char* buffer) {
  buffer[0] = slot < Board::kCols ? 'A' : 'B';
  buffer[1] = static_cast<char>('1' + slot % Board::kCols);
  buffer[2] = '\0';
  return buffer;
}

}  // namespace

BattleScene::~BattleScene() {
  if (field_front_2d_ != nullptr) SDL_DestroyTexture(field_front_2d_);
  for (auto& [code, texture] : hud_faces_) SDL_DestroyTexture(texture);
  if (gpu_ == nullptr) return;
  hand_.Clear();
  board_.Destroy(*gpu_);
  enemy_board_.Destroy(*gpu_);
  sword_.Destroy(*gpu_);
  card_model_.reset();
  arena_model_.reset();
  torch_.reset();
  gpu_->DestroyMesh(mist_quad_);
  for (SDL_GPUTexture* t : textures_) gpu_->DestroyTexture(t);
  textures_.clear();
}

void BattleScene::Preload(const PreloadInfo& info) {
  preloaded_ = true;
  const std::string base = BasePath();
  // Frente da carta de campo para o HUD: aparece com ~120 px de altura, então vai reduzida 4x
  // (400x557); vira SDL_Texture em OnEnter, na thread principal.
  hud_field_ = Image::LoadFile(base + "sprites/CardFrontField.png").Downscaled(4);
  if (info.gpu_device == nullptr) return;

  gpu_ = std::make_unique<Gpu>();
  if (!gpu_->InitDevice(info.gpu_device, info.logical_width, info.logical_height,
                        base + "shaders")) {
    gpu_.reset();
    return;
  }
  card_model_ = std::make_unique<Model>();
  if (!card_model_->Upload(*gpu_, Model::Parse(base + "models/Card.glb"))) {
    SDL_Log("Falha ao carregar models/Card.glb");
    card_model_.reset();
    return;
  }
  back_ = UploadSprite(Image::LoadFile(base + "sprites/CardBack.png"));
  front_normal_ = UploadSprite(Image::LoadFile(base + "sprites/CardFrontNormalMap.png"));
  back_normal_ = UploadSprite(Image::LoadFile(base + "sprites/CardBackNormalMap.png"));
  for (Board* board : {&board_, &enemy_board_}) {
    board->GetLayout().arena_scale = kArenaScale;
    board->SetModel(card_model_.get());
    board->SetBack(back_);
    board->SetNormalMaps(front_normal_, back_normal_);
    board->Init(*gpu_);
  }
  enemy_board_.SetSide(-1.0f);
  sword_.Init(*gpu_);
  // Molduras reduzidas 2x (800x1115): a carta na mão é pequena e a frente de cada carta é
  // composta a partir delas com os textos (cardface).
  frames_[0] = Image::LoadFile(base + "sprites/CardFrontGood.png").Downscaled(2);
  frames_[1] = Image::LoadFile(base + "sprites/CardFrontEvil.png").Downscaled(2);
  frames_[2] = Image::LoadFile(base + "sprites/CardFrontField.png").Downscaled(2);

  // Montes: 40 cartas sorteadas por peso e embaralhadas (deck::Deck) para cada lado, ANTES de
  // qualquer carta aparecer; as 5 da sua mão inicial são compradas e compostas já aqui. O
  // computador também compra 5 (só os dados; a frente é composta quando a carta entra).
  cards_ = cards::LoadAll(base);
  rng_.seed(std::random_device{}());
  deck_ = deck::Deck(cards::BuildDeck(cards_, Hud::kDeckSize, rng_));
  deck_.Shuffle();
  // Monte do computador conforme o nível (menu; PONTIFICE_DIFFICULTY=easy|medium|hard|impossible
  // força, para depuração).
  int level = info.difficulty;
  if (const char* env = SDL_getenv("PONTIFICE_DIFFICULTY")) {
    if (std::strcmp(env, "easy") == 0) level = 0;
    if (std::strcmp(env, "medium") == 0) level = 1;
    if (std::strcmp(env, "hard") == 0) level = 2;
    if (std::strcmp(env, "impossible") == 0) level = 3;
  }
  difficulty_ = static_cast<ai::Difficulty>(std::clamp(level, 0, 3));
  enemy_deck_ = deck::Deck(cards::BuildDeck(cards_, Hud::kDeckSize, rng_,
                                            [this](const CardInfo& card) {
                                              return ai::DeckWeight(card, difficulty_);
                                            }));
  enemy_deck_.Shuffle();
  SDL_Log("Nivel do oponente: %s", ai::DifficultyKey(difficulty_));
  // Depuração: PONTIFICE_HAND=G101,E101,... força os códigos da mão inicial (o monte continua
  // embaralhado; essas cartas não saem dele).
  std::vector<std::string> forced;
  if (const char* env = SDL_getenv("PONTIFICE_HAND")) {
    std::string spec = env;
    size_t pos = 0;
    while (pos <= spec.size()) {
      size_t end = spec.find(',', pos);
      if (end == std::string::npos) end = spec.size();
      if (end > pos) forced.push_back(spec.substr(pos, end - pos));
      pos = end + 1;
    }
  }
  for (int i = 0; i < 5; ++i) {
    std::optional<std::string> code;
    if (!forced.empty()) {
      if (i < static_cast<int>(forced.size())) code = forced[i];
    } else {
      code = deck_.Draw();
    }
    if (!code) break;
    const Face face = MakeCardFace(*code);
    if (face.texture != nullptr) initial_hand_.push_back(face);
  }
  for (int i = 0; i < 5; ++i) EnemyDraw();
  arena_model_ = std::make_unique<Model>();
  if (!arena_model_->Upload(*gpu_, Model::Parse(base + "models/Arena.glb"))) {
    SDL_Log("Arena.glb nao carregado; gere com tools/texture_arena.py");
    arena_model_.reset();
  }
  torch_ = std::make_unique<Torch>();
  if (!torch_->Upload(*gpu_, Model::Parse(base + "models/Torch.glb"))) {
    SDL_Log("Torch.glb nao carregado; gere com tools/make_torch.py");
    torch_.reset();
  }
  // Quad horizontal em y = 0; cada camada de névoa o desenha transladado para sua altura.
  const float h = config::Float("fog", "battle.mist.halfSize", 40.0f);
  mist_quad_ = gpu_->CreateMesh(
      {{-h, 0, -h, 0, 0}, {h, 0, -h, 1, 0}, {h, 0, h, 1, 1}, {-h, 0, h, 0, 1}},
      {0, 1, 2, 0, 2, 3});
  mist_layers_ = LoadMistLayers("battle.mist.layers");
  intro_mist_layers_ = LoadMistLayers("battle.introMist.layers");
  if (mist_layers_.empty()) mist_layers_ = {{kArenaBaseY + 0.05f, 0.8f}, {kArenaBaseY + 0.45f, 0.5f}};
}

SDL_GPUTexture* BattleScene::UploadSprite(const Image& image) {
  SDL_GPUTexture* texture = gpu_->CreateTexture(image);
  if (texture != nullptr) textures_.push_back(texture);
  return texture;
}

SDL_Texture* BattleScene::UploadHudSprite(SDL_Renderer* renderer, const Image& image) {
  if (!image.Valid()) return nullptr;
  SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                           SDL_TEXTUREACCESS_STATIC, image.width, image.height);
  if (texture == nullptr) return nullptr;
  SDL_UpdateTexture(texture, nullptr, image.rgba.data(), image.width * 4);
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
  return texture;
}

void BattleScene::ToggleSampleFieldCard() {
  if (hud_.HasFieldCard()) {
    hud_.ClearFieldCard();
    field_ = rules::Field{};
    return;
  }
  // Primeira carta de assets/CardsField.json.
  for (const CardInfo& card : cards_) {
    if (card.IsField()) {
      PlayField(Side::kPlayer, card);
      return;
    }
  }
}

BattleScene::Face BattleScene::MakeCardFace(const std::string& code) {
  const CardInfo* card = cards::Find(cards_, code);
  if (card == nullptr || gpu_ == nullptr) return {};
  const Image& frame = card->category == 'G' ? frames_[0]
                       : card->category == 'E' ? frames_[1]
                                               : frames_[2];
  // Hierarquia 1 (Divindades, Soberanos): carta foil.
  return {UploadSprite(cardface::Compose(frame, *card)), card->hierarchy == 1, card};
}

SDL_Texture* BattleScene::HudFace(const CardInfo& card) {
  if (renderer_ == nullptr) return field_front_2d_;
  auto it = hud_faces_.find(card.code);
  if (it != hud_faces_.end()) return it->second;
  // Composta na moldura da categoria reduzida 2x e reduzida mais 2x (400x557): o HUD e o
  // tooltip mostram de 120 a 300 px de altura.
  const Image& frame = card.category == 'G' ? frames_[0]
                       : card.category == 'E' ? frames_[1]
                                              : frames_[2];
  if (!frame.Valid()) return field_front_2d_;
  SDL_Texture* texture = UploadHudSprite(renderer_, cardface::Compose(frame, card).Downscaled(2));
  if (texture == nullptr) return field_front_2d_;
  hud_faces_[card.code] = texture;
  return texture;
}

void BattleScene::DrawCard() {
  const std::optional<std::string> code = deck_.Draw();
  if (!code) return;
  hud_.SetDeck(Hud::kDeckSize - static_cast<int>(deck_.Size()), static_cast<int>(deck_.Size()));
  const Face face = MakeCardFace(*code);
  if (face.texture != nullptr) hand_.Add(face.texture, face.foil, face.info);
}

void BattleScene::EnemyDraw() {
  const std::optional<std::string> code = enemy_deck_.Draw();
  if (!code) return;
  if (const CardInfo* info = cards::Find(cards_, *code)) enemy_hand_.push_back(info);
}

// ---------------------------------------------------------------------------------------------
// Fluxo da partida

void BattleScene::Schedule(float delay, std::function<void()> fn) {
  steps_.emplace_back(delay, std::move(fn));
}

bool BattleScene::Busy() const {
  return !steps_.empty() || board_.Animating() || enemy_board_.Animating() || sword_.Active() ||
         std::fabs(orbit_ - orbit_target_) > 0.001f;
}

bool BattleScene::AttacksAllowed(Side side) const {
  return !(starter_ == side && turns_played_[Index(side)] == 0);
}

void BattleScene::StartMatch() {
  started_ = true;
  // Sorteio de quem começa (PONTIFICE_STARTER força, para depuração).
  starter_ = std::uniform_int_distribution<int>(0, 1)(rng_) == 0 ? Side::kPlayer : Side::kEnemy;
  if (const char* env = SDL_getenv("PONTIFICE_STARTER")) {
    if (std::strcmp(env, "player") == 0) starter_ = Side::kPlayer;
    if (std::strcmp(env, "enemy") == 0) starter_ = Side::kEnemy;
  }
  SDL_Log("Sorteio: %s", starter_ == Side::kPlayer ? "voce comeca" : "o oponente comeca");
  if (starter_ == Side::kPlayer) {
    BeginPlayerTurn();
  } else {
    hand_.SetAway(true);
    orbit_target_ = 1.0f;
    Schedule(kOrbitSeconds + 0.2f, [this] { RunEnemyTurn(); });
  }
}

void BattleScene::BeginPlayerTurn() {
  if (hud_.Over()) return;
  turn_side_ = Side::kPlayer;
  player_turn_ = true;
  summoned_ = false;
  attack_from_ = -1;
  attack_drag_ = false;
  board_.ResetAttacks();
  hand_.SetAway(false);
  orbit_target_ = 0.0f;
  // Fase de compra: quem começou não compra no primeiro turno.
  if (!(starter_ == Side::kPlayer && turns_played_[0] == 0)) DrawCard();
  SDL_Log("Turno %d: sua vez", turn_);
}

void BattleScene::EndTurn() {
  if (!player_turn_ || Busy() || hud_.Over()) return;
  player_turn_ = false;
  ++turns_played_[0];
  hand_.Deselect();
  attack_from_ = -1;
  attack_drag_ = false;
  DiscardExtraCards();
  // Virada de campo e turno do computador.
  hand_.SetAway(true);
  orbit_target_ = 1.0f;
  Schedule(kOrbitSeconds + 0.2f, [this] { RunEnemyTurn(); });
}

void BattleScene::DiscardExtraCards() {
  // Fase final: com mais de 6 cartas, descarta até 6 (aqui, as de menor ataque).
  while (hand_.Size() > static_cast<size_t>(kHandLimit)) {
    size_t worst = 0;
    int worst_value = 1 << 30;
    for (size_t i = 0; i < hand_.Size(); ++i) {
      const CardInfo* info = hand_.CardAt(i).info;
      const int value = info == nullptr ? 0 : (info->IsField() ? 0 : info->attack);
      if (value < worst_value) {
        worst_value = value;
        worst = i;
      }
    }
    hand_.Remove(worst);
    hud_.SetGraveyard(hud_.Graveyard() + 1);
  }
}

void BattleScene::RunEnemyTurn() {
  if (hud_.Over()) return;
  turn_side_ = Side::kEnemy;
  enemy_board_.ResetAttacks();
  if (!(starter_ == Side::kEnemy && turns_played_[1] == 0)) EnemyDraw();
  SDL_Log("Turno %d: oponente (%zu na mao, %zu no monte)", turn_, enemy_hand_.size(),
          enemy_deck_.Size());
  // Fase principal: trocas de posição, carta de campo e invocação, com pausas.
  Schedule(kEnemyPause, [this] {
    for (int slot : ai::ChoosePositionChanges(BuildEnemyView())) {
      enemy_board_.TogglePosition(slot);
    }
  });
  Schedule(kEnemyPause, [this] {
    const int index = ai::ChooseFieldCard(BuildEnemyView());
    if (index < 0) return;
    const CardInfo* card = enemy_hand_[static_cast<size_t>(index)];
    enemy_hand_.erase(enemy_hand_.begin() + index);
    PlayField(Side::kEnemy, *card);
  });
  Schedule(kEnemyPause, [this] {
    const std::optional<ai::Summon> summon = ai::ChooseSummon(BuildEnemyView());
    if (summon) EnemySummon(summon->hand_index, summon->defense);
  });
  // Fase de batalha: um ataque por passo, reavaliando o campo a cada vez.
  Schedule(kEnemyPause + 0.3f, [this] { EnemyAttackStep(); });
}

void BattleScene::EnemyAttackStep() {
  if (hud_.Over()) return;
  const std::optional<ai::Attack> attack = ai::ChooseAttack(BuildEnemyView());
  if (!attack) {
    Schedule(kEnemyPause, [this] { FinishEnemyTurn(); });
    return;
  }
  PerformAttack(Side::kEnemy, attack->slot, attack->target_slot);
  Schedule(0.9f, [this] { EnemyAttackStep(); });
}

void BattleScene::FinishEnemyTurn() {
  ++turns_played_[1];
  // Fase final do computador: mão com mais de 6 cartas descarta as de menor ataque.
  while (enemy_hand_.size() > static_cast<size_t>(kHandLimit)) {
    auto worst = std::min_element(enemy_hand_.begin(), enemy_hand_.end(),
                                  [](const CardInfo* a, const CardInfo* b) {
                                    return a->attack < b->attack;
                                  });
    enemy_hand_.erase(worst);
  }
  ++turn_;
  orbit_target_ = 0.0f;
  Schedule(kOrbitSeconds + 0.2f, [this] { BeginPlayerTurn(); });
}

ai::View BattleScene::BuildEnemyView() const {
  ai::View view;
  view.hand = enemy_hand_;
  for (int slot : enemy_board_.Occupied()) {
    const Board::Card* c = enemy_board_.At(slot);
    view.mine.push_back(ai::FieldCard{slot, c->info, c->defense,
                                      !c->defense && !c->attacked, c->summoned_turn == turn_});
  }
  for (int slot : board_.Occupied()) {
    const Board::Card* c = board_.At(slot);
    view.theirs.push_back(
        ai::FieldCard{slot, c->info, c->defense, false, c->summoned_turn == turn_});
  }
  view.field = field_;
  view.my_life = hud_.OpponentLife();
  view.their_life = hud_.PlayerLife();
  view.can_summon = true;  // uma invocação por turno: ChooseSummon é chamado uma vez
  view.can_attack = AttacksAllowed(Side::kEnemy);
  view.free_slots = static_cast<int>(enemy_board_.Free().size());
  return view;
}

void BattleScene::EnemySummon(int hand_index, bool defense) {
  if (hand_index < 0 || hand_index >= static_cast<int>(enemy_hand_.size())) return;
  const CardInfo* info = enemy_hand_[static_cast<size_t>(hand_index)];
  std::vector<int> free = enemy_board_.Free();
  int slot;
  if (free.empty()) {
    // Campo cheio: substitui a carta de menor ataque.
    slot = -1;
    int worst = 1 << 30;
    for (int s : enemy_board_.Occupied()) {
      const Board::Card* c = enemy_board_.At(s);
      const int value = c->info != nullptr ? c->info->attack : 0;
      if (value < worst) {
        worst = value;
        slot = s;
      }
    }
    if (slot < 0) return;
    enemy_board_.Discard(slot);
  } else {
    slot = ai::ChooseSlot(free, Board::kCols);
  }
  enemy_hand_.erase(enemy_hand_.begin() + hand_index);
  const Face face = MakeCardFace(info->code);
  if (face.texture == nullptr) return;
  enemy_board_.Place(slot, face.texture, face.foil, defense, info, turn_);
  char name[3];
  SDL_Log("Oponente invocou %s em %s (%s)", info->name.c_str(), SlotName(slot, name),
          defense ? "defesa" : "ataque");
}

// ---------------------------------------------------------------------------------------------
// Ações do jogador

bool BattleScene::CanAct() const {
  return ready_ && started_ && player_turn_ && !hud_.Over() && !hand_.Inspecting() && !Busy();
}

bool BattleScene::CanSummon() const {
  if (!CanAct() || hand_.Candidate() < 0) return false;
  // Carta de campo não gasta a invocação normal do turno.
  return CandidateIsField() || !summoned_;
}

bool BattleScene::CandidateIsField() const {
  const int index = hand_.Candidate();
  if (index < 0) return false;
  const CardInfo* info = hand_.CardAt(static_cast<size_t>(index)).info;
  return info != nullptr && info->IsField();
}

void BattleScene::PlayField(Side owner, const CardInfo& card) {
  // Só uma carta de campo ativa: a anterior vai para o cemitério do dono.
  if (field_.active && field_owner_ == Side::kPlayer) hud_.SetGraveyard(hud_.Graveyard() + 1);
  field_ = rules::Field::From(card);
  field_owner_ = owner;
  FieldCardInfo info;
  info.name = card.name;
  info.favored = field_.favored == 'E' ? Category::kEvil : Category::kGood;
  info.buff = card.buff;
  info.nerf = card.nerf;
  info.image = HudFace(card);
  hud_.SetFieldCard(info);
  SDL_Log("%s ativou a carta de campo %s", owner == Side::kPlayer ? "Voce" : "Oponente",
          card.name.c_str());
}

void BattleScene::Summon(int slot) {
  const int index = hand_.Candidate();
  if (index < 0 || slot < 0 || slot >= Board::kSlots) return;
  const Hand::Card card = hand_.Remove(static_cast<size_t>(index));
  if (card.info != nullptr && card.info->IsField()) {
    PlayField(Side::kPlayer, *card.info);
    return;
  }
  // Slot ocupado: a carta anterior é descartada para o cemitério (docs/campo.md).
  if (!board_.Empty(slot) && board_.Discard(slot)) hud_.SetGraveyard(hud_.Graveyard() + 1);
  // Entra virada para cima, em modo de ataque (docs/batalha.md).
  if (!board_.Place(slot, card.front, card.foil, false, card.info, turn_)) return;
  summoned_ = true;
  board_.SetHighlight(false);
  board_.SetHoverSlot(-1);
  char name[3];
  SDL_Log("Turno %d: carta invocada em %s", turn_, SlotName(slot, name));
}

// ---------------------------------------------------------------------------------------------
// Combate

void BattleScene::PerformAttack(Side attacker, int slot, int target_slot) {
  Board& mine = BoardOf(attacker);
  Board& theirs = BoardOf(Other(attacker));
  Board::Card* card = mine.At(slot);
  if (card == nullptr || card->attacked || card->defense) return;
  if (target_slot >= 0 && theirs.Empty(target_slot)) return;
  if (target_slot < 0 && theirs.Count() > 0) return;  // direto só com o campo vazio
  card->attacked = true;
  // Investida até a carta alvo ou, no ataque direto, até o meio do lado do oponente.
  const math::Vec3 target = target_slot >= 0
                                ? theirs.CardPosition(target_slot)
                                : theirs.SlotPosition(Board::kCols / 2 + Board::kCols);
  mine.Lunge(slot, target);
  Schedule(kAttackHit, [this, attacker, slot, target_slot] {
    ResolveAttack(attacker, slot, target_slot);
  });
}

void BattleScene::ResolveAttack(Side attacker, int slot, int target_slot) {
  Board& mine = BoardOf(attacker);
  Board& theirs = BoardOf(Other(attacker));
  const Board::Card* card = mine.At(slot);
  if (card == nullptr || card->info == nullptr) return;
  const int attack = rules::EffectiveAttack(*card->info, field_);
  rules::Outcome outcome;
  char a_name[3], d_name[3];
  // Golpe de espada no alvo (carta ou, no ataque direto, o meio do lado do oponente).
  const math::Vec3 hit = target_slot >= 0
                             ? theirs.CardPosition(target_slot)
                             : theirs.SlotPosition(Board::kCols / 2 + Board::kCols);
  sword_.Play(hit, mine.CardPosition(slot));
  if (target_slot >= 0) theirs.Flash(target_slot);
  if (target_slot < 0) {
    outcome = rules::ResolveDirect(attack);
    SDL_Log("%s: ataque direto de %s (%d)", attacker == Side::kPlayer ? "Voce" : "Oponente",
            card->info->name.c_str(), attack);
  } else {
    Board::Card* target = theirs.At(target_slot);
    if (target == nullptr || target->info == nullptr) return;
    outcome = rules::Resolve(attack, target->defense,
                             rules::EffectiveAttack(*target->info, field_),
                             rules::EffectiveDefense(*target->info, field_));
    SDL_Log("%s: %s (%s, %d) ataca %s (%s, %s)", attacker == Side::kPlayer ? "Voce" : "Oponente",
            card->info->name.c_str(), SlotName(slot, a_name), attack,
            target->info->name.c_str(), SlotName(target_slot, d_name),
            target->defense ? "defesa" : "ataque");
  }
  if (outcome.defender_destroyed) Destroy(Other(attacker), target_slot);
  if (outcome.attacker_destroyed) Destroy(attacker, slot);
  if (outcome.defender_damage > 0) ApplyDamage(Other(attacker), outcome.defender_damage);
  if (outcome.attacker_damage > 0) ApplyDamage(attacker, outcome.attacker_damage);
  if (hud_.Over()) {
    // Fim da batalha: nada mais acontece; o HUD mostra o resultado.
    steps_.clear();
    player_turn_ = false;
    hand_.SetAway(true);
    SDL_Log("Fim: %s", hud_.PlayerLife() > 0 ? "vitoria" : "derrota");
  }
}

void BattleScene::Destroy(Side owner, int slot) {
  if (!BoardOf(owner).Discard(slot)) return;
  if (owner == Side::kPlayer) hud_.SetGraveyard(hud_.Graveyard() + 1);
}

void BattleScene::ApplyDamage(Side side, int damage) {
  if (side == Side::kPlayer) {
    hud_.SetLife(hud_.OpponentLife(), hud_.PlayerLife() - damage);
  } else {
    hud_.SetLife(hud_.OpponentLife() - damage, hud_.PlayerLife());
  }
}

math::Vec2 BattleScene::ScreenOf(const math::Vec3& world, int width, int height) const {
  const math::Vec4 clip = arena_view_proj_ * math::Vec4{world.x, world.y, world.z, 1.0f};
  const float w = clip.w > 1e-6f ? clip.w : 1e-6f;
  return {(clip.x / w * 0.5f + 0.5f) * width, (1.0f - (clip.y / w * 0.5f + 0.5f)) * height};
}

// ---------------------------------------------------------------------------------------------
// Cena

void BattleScene::OnEnter(SceneContext& ctx) {
  // Sem Preload antes (entrada direta na batalha), carrega tudo aqui mesmo.
  if (!preloaded_) Preload(ctx.Preload());
  renderer_ = ctx.renderer;

  end_turn_ = std::make_unique<ButtonAction>(i18n::Get("battle.endTurn"),
                                             [this](SceneContext&) { EndTurn(); });
  SDL_FRect rect{ctx.logical_width - kButtonMargin - kButtonWidth,
                 ctx.logical_height - kButtonBottom - kButtonHeight, kButtonWidth, kButtonHeight};
  end_turn_->SetRect(rect);
  sound_button_.SetRect(SDL_FRect{kButtonMargin, ctx.logical_height - kButtonMargin - kSoundButtonSize,
                                  kSoundButtonSize, kSoundButtonSize});
  hud_.SetDeckPanelLeft(kButtonMargin + kSoundButtonSize + kSoundButtonGap);
  started_ = false;
  player_turn_ = false;
  summoned_ = false;
  turn_ = 1;
  turns_played_[0] = turns_played_[1] = 0;
  steps_.clear();
  field_ = rules::Field{};

  // HUD (docs/hud.md): vida cheia, sem carta de campo, cemitério vazio e monte com a mão
  // inicial já comprada. Não depende do render 3D.
  hud_.SetLife(Hud::kMaxLife, Hud::kMaxLife);
  hud_.ClearFieldCard();
  hud_.SetGraveyard(0);
  // Fundo = cor da névoa: o que some na distância se funde com ele (assets/Fog.json, que
  // por padrão aponta para os papéis background.fog e background.mist de colors.json).
  fog_color_ = config::Color("fog", "battle.fog.color", colors::GetF("background.fog"));
  mist_color_ = config::Color("fog", "battle.mist.color", colors::GetF("background.mist"));
  if (field_front_2d_ == nullptr) {
    field_front_2d_ = UploadHudSprite(ctx.renderer, hud_field_);
    hud_field_ = Image{};
  }

  if (gpu_ == nullptr || card_model_ == nullptr) {
    SDL_Log("Falha ao iniciar o render 3D");
    return;
  }
  gpu_->SetRenderer(ctx.renderer);

  hand_.SetModel(card_model_.get());
  hand_.SetBack(back_);
  hand_.SetNormalMaps(front_normal_, back_normal_);
  // Mão inicial: as 5 cartas compradas do monte embaralhado no Preload (ver docs/batalha.md).
  // Entram pela esquerda, uma a uma, depois de o fade in da entrada terminar.
  hand_.DelayDeal(0.8f);
  for (const Face& face : initial_hand_) hand_.Add(face.texture, face.foil, face.info);
  initial_hand_.clear();
  hud_.SetDeck(Hud::kDeckSize - static_cast<int>(deck_.Size()), static_cast<int>(deck_.Size()));
  arena_world_ = math::Mat4::Scale(kArenaScale);

  const float aspect = static_cast<float>(ctx.logical_width) / ctx.logical_height;
  proj_ = math::Mat4::Perspective(math::Radians(35.0f), aspect, 0.1f, 100.0f);
  hand_view_proj_ = proj_ * math::Mat4::LookAt({0, 0, 10}, {0, 0, 0}, {0, 1, 0});
  // Vista do jogador: acima e à frente do seu lado da arena, olhando para o centro.
  math::Vec3 eye{0, 10.5f, 12.2f};
  if (const char* env = SDL_getenv("PONTIFICE_ARENA_EYE")) {  // depuração: "x,y,z"
    SDL_sscanf(env, "%f,%f,%f", &eye.x, &eye.y, &eye.z);
  }
  eye_final_ = eye;
  arena_view_proj_ = proj_ * math::Mat4::LookAt(eye, kArenaTarget, {0, 1, 0});

  // Clima medieval: a luz da arena vem só das duas tochas (luzes pontuais nas chamas, cor
  // amarelo-branca de Torch::LightColor, com tremular); sem luz direcional, ambiente frio e
  // fraco, névoa na base e vinheta. Valores em assets/Light.json (battle.arena, torch),
  // Fog.json (battle.fog, battle.mist) e Shadow.json (battle).
  arena_atmosphere_.light_dir = config::Vec3("light", "battle.arena.lightDir", {-0.35f, 1.0f, 0.45f});
  arena_atmosphere_.light_intensity = config::Float("light", "battle.arena.lightIntensity", 0.0f);
  arena_atmosphere_.light_color = Torch::LightColor();
  arena_atmosphere_.ambient = config::Color("light", "battle.arena.ambient", {0.07f, 0.08f, 0.13f, 1.0f});
  arena_atmosphere_.point_intensity = 0.0f;  // acendem quando o fogo surge nas tochas
  arena_atmosphere_.point2_intensity = 0.0f;
  arena_atmosphere_.fog_color = fog_color_;
  arena_atmosphere_.fog_density = config::Float("fog", "battle.fog.density", 0.05f);
  arena_atmosphere_.fog_start = config::Float("fog", "battle.fog.start", 15.0f);
  arena_atmosphere_.fog_full_y = kArenaBaseY + config::Float("fog", "battle.fog.fullYOffset", 0.0f);
  arena_atmosphere_.fog_clear_y = kArenaTopY + config::Float("fog", "battle.fog.clearYOffset", -0.2f);
  arena_atmosphere_.fog_height_strength = config::Float("fog", "battle.fog.heightStrength", 0.9f);
  arena_atmosphere_.camera_pos = eye;
  arena_atmosphere_.vignette = config::Float("light", "battle.arena.vignette", 0.55f);
  arena_atmosphere_.shadow_strength = config::Float("shadow", "battle.strength", 0.85f);
  arena_atmosphere_.shadow_bias = config::Float("shadow", "bias", 0.0015f);
  arena_atmosphere_.mist_fade_start = config::Float("fog", "battle.mist.fadeStart", 8.0f);
  arena_atmosphere_.mist_fade_end = config::Float("fog", "battle.mist.fadeEnd", 28.0f);
  arena_atmosphere_.mist_noise_scale = config::Float("fog", "battle.mist.noiseScale", 1.0f);
  torch_light_ = config::Float("light", "torch.battle.intensity", 2.6f);
  shadow_proj_ = math::Mat4::Perspective(
      math::Radians(config::Float("shadow", "battle.fov", 130.0f)), 1.0f,
      config::Float("shadow", "battle.near", 0.3f), config::Float("shadow", "battle.far", 60.0f));
  shadow_target_ = config::Vec3("shadow", "battle.target", {0.0f, 0.3f, 0.0f});

  // Mão: mesma luz da tocha (principal e pontual, com o mesmo tremular), ambiente um
  // pouco mais claro para as cartas ficarem legíveis. Sem névoa nem vinheta.
  hand_atmosphere_.light_dir = config::Vec3("light", "battle.hand.lightDir", {0.3f, 0.6f, 1.0f});
  hand_atmosphere_.light_intensity = config::Float("light", "battle.hand.lightIntensity", 0.5f);
  hand_atmosphere_.light_color = arena_atmosphere_.light_color;
  hand_atmosphere_.ambient = config::Color("light", "battle.hand.ambient", {0.34f, 0.36f, 0.46f, 1.0f});
  hand_atmosphere_.point_pos = config::Vec3("light", "battle.hand.pointPos", {0.0f, 2.5f, 4.0f});
  hand_atmosphere_.point_intensity = config::Float("light", "battle.hand.pointIntensity", 1.0f);
  hand_atmosphere_.camera_pos = {0, 0, 10};
  ready_ = true;
  UpdateIntro(0.0f);  // primeiro quadro já com a arena afundada e a câmera longe
}

void BattleScene::UpdateIntro(float dt_seconds) {
  intro_time_ += dt_seconds;
  if (intro_ >= 1.0f && torch_intro_ >= 1.0f && ignite_ >= 1.0f) return;
  intro_ = SmoothStep(intro_time_ / kIntroSeconds);
  torch_intro_ = SmoothStep((intro_time_ - kTorchIntroStart) / kTorchIntroSeconds);
  ignite_ = SmoothStep((intro_time_ - kTorchIntroStart - kTorchIntroSeconds) / kIgniteSeconds);
  // Depuração: PONTIFICE_ARENA_EYE fixa a câmera, então pula a abertura.
  if (SDL_getenv("PONTIFICE_ARENA_EYE") != nullptr) intro_ = torch_intro_ = ignite_ = 1.0f;
  const float drop = kIntroDrop * (1.0f - intro_);
  arena_world_ = math::Mat4::Translate({0.0f, -drop, 0.0f}) * math::Mat4::Scale(kArenaScale);
  // Tochas emergem do chão depois da arena; o fogo acende depois, e a luz vem com ele.
  const float torch_drop = kTorchIntroDrop * (1.0f - torch_intro_);
  torch_world_[0] = math::Mat4::Translate({-kTorchX, kArenaBaseY - torch_drop, kTorchZ}) *
                    math::Mat4::RotateZ(kTorchTilt) * math::Mat4::Scale(kTorchScale);
  torch_world_[1] = math::Mat4::Translate({kTorchX, kArenaBaseY - torch_drop, kTorchZ}) *
                    math::Mat4::RotateZ(-kTorchTilt) * math::Mat4::Scale(kTorchScale);
  arena_atmosphere_.point_pos = Torch::FlamePosition(torch_world_[0]);
  arena_atmosphere_.point2_pos = Torch::FlamePosition(torch_world_[1]);
  // Sombras: cada chama olha para o centro da arena com campo bem aberto.
  for (int k = 0; k < 2; ++k) {
    const math::Vec3 flame = k == 0 ? arena_atmosphere_.point_pos : arena_atmosphere_.point2_pos;
    torch_light_vp_[k] = shadow_proj_ * math::Mat4::LookAt(flame, shadow_target_, {0, 1, 0});
  }
  arena_atmosphere_.point_intensity = torch_light_ * ignite_;
  arena_atmosphere_.point2_intensity = torch_light_ * ignite_;
  const math::Vec3 eye = kIntroEyeStart + (eye_final_ - kIntroEyeStart) * intro_;
  arena_view_proj_ = proj_ * math::Mat4::LookAt(eye, kArenaTarget, {0, 1, 0});
  arena_atmosphere_.camera_pos = eye;
}

void BattleScene::UpdateCamera() {
  // Virada de campo: a câmera gira em torno do eixo vertical pelo centro da arena.
  if (intro_ < 1.0f) return;
  const float angle = math::kPi * SmoothStep(orbit_);
  const float c = std::cos(angle), s = std::sin(angle);
  const math::Vec3 e = eye_final_;
  const math::Vec3 eye{e.x * c + e.z * s, e.y, -e.x * s + e.z * c};
  arena_view_proj_ = proj_ * math::Mat4::LookAt(eye, kArenaTarget, {0, 1, 0});
  arena_atmosphere_.camera_pos = eye;
}

void BattleScene::Pause(SceneContext& ctx) {
  paused_ = true;
  // Solta o que estava em andamento com o mouse (arrasto de ataque).
  attack_from_ = -1;
  attack_drag_ = false;
  press_ = {-1.0f, -1.0f};
  if (ctx.music != nullptr) {
    ctx.music->SetScene("menu");
    ctx.music->SetCategory("ost");
  }
  // Sim = abandonar: ação destrutiva, placa vermelha.
  abandon_dialog_.Open(i18n::Get("battle.abandon.question"), i18n::Get("battle.abandon.no"),
                       i18n::Get("battle.abandon.yes"), ctx.logical_width, ctx.logical_height,
                       /*yes_danger=*/true);
}

void BattleScene::Resume(SceneContext& ctx) {
  paused_ = false;
  abandon_dialog_.Close();
  if (ctx.music != nullptr) {
    ctx.music->SetScene("battle");
    ctx.music->SetCategory("battle", /*resume=*/true);  // continua a faixa de onde parou
  }
}

void BattleScene::HandleEvent(SceneContext& ctx, const SDL_Event& event) {
  // Pausado: só a janela de confirmação recebe eventos.
  if (paused_) {
    switch (abandon_dialog_.HandleEvent(event)) {
      case ConfirmDialog::Result::kNo:
        Resume(ctx);
        break;
      case ConfirmDialog::Result::kYes:
        abandon_dialog_.Close();
        ctx.next_scene = SceneId::kMenu;  // o menu já toca a trilha dele
        break;
      case ConfirmDialog::Result::kNone:
        break;
    }
    return;
  }
  if (event.type == SDL_EVENT_KEY_DOWN) {
    switch (event.key.key) {
      case SDLK_ESCAPE:
        if (hand_.Inspecting()) {  // primeiro sai da inspeção; só depois pausa
          hand_.StopInspect();
          return;
        }
        // Batalha encerrada (vitória/derrota): Esc volta direto ao menu. Em andamento: pausa
        // e pergunta se abandona.
        if (hud_.Over()) {
          ctx.next_scene = SceneId::kMenu;
        } else {
          Pause(ctx);
        }
        return;
      case SDLK_SPACE:  // vira a carta inspecionada
        hand_.Flip();
        return;
      case SDLK_F:  // depuração: carta de campo de exemplo
        ToggleSampleFieldCard();
        return;
      case SDLK_1:  // depuração: dano no oponente
        if (!hud_.Over()) hud_.SetLife(hud_.OpponentLife() - 700, hud_.PlayerLife());
        return;
      case SDLK_2:  // depuração: dano em você
        if (!hud_.Over()) hud_.SetLife(hud_.OpponentLife(), hud_.PlayerLife() - 700);
        return;
      case SDLK_D:  // depuração: compra do monte
        DrawCard();
        return;
      case SDLK_G:  // depuração: carta para o cemitério
        hud_.SetGraveyard(hud_.Graveyard() + 1);
        return;
      default:
        break;
    }
  }
  if (event.type == SDL_EVENT_MOUSE_MOTION) {
    mouse_ = {event.motion.x, event.motion.y};
    if (attack_from_ >= 0 && !attack_drag_ && press_.x >= 0) {
      const float moved = std::fabs(mouse_.x - press_.x) + std::fabs(mouse_.y - press_.y);
      if (moved >= kAttackDragPx) attack_drag_ = true;
    }
  }
  // Botão de som: sempre disponível; fica com os eventos de mouse sobre ele.
  if (sound_button_.HandleEvent(event)) {
    if (ctx.music != nullptr) ctx.music->ToggleMuted();
    return;
  }
  if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
      sound_button_.Contains(event.button.x, event.button.y)) {
    return;
  }
  // "Passar turno" (visível no seu turno) fica com os eventos de mouse sobre ele.
  if (CanAct()) {
    if (end_turn_->HandleEvent(event)) {
      end_turn_->OnClick(ctx);
      return;
    }
    if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
        end_turn_->Contains(event.button.x, event.button.y)) {
      return;
    }
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    const math::Vec2 pos{event.button.x, event.button.y};
    if (event.button.button == SDL_BUTTON_LEFT) {
      press_ = pos;
      attack_from_ = -1;
      attack_drag_ = false;
      // Pressionar uma carta sua em ataque que ainda não atacou: começa um arrasto de ataque.
      if (CanAct() && AttacksAllowed(Side::kPlayer) && hand_.Hovered() < 0) {
        const int slot = board_.PickSlot(pos, arena_view_proj_, ctx.logical_width,
                                         ctx.logical_height);
        const Board::Card* card = board_.At(slot);
        if (card != nullptr && !card->defense && !card->attacked) {
          attack_from_ = slot;
        }
      }
    }
    hand_.MouseDown(pos, event.button.button == SDL_BUTTON_LEFT);
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
    const math::Vec2 pos{event.button.x, event.button.y};
    const bool inspecting = hand_.Inspecting();
    hand_.MouseUp(pos);
    const float moved = std::fabs(pos.x - press_.x) + std::fabs(pos.y - press_.y);
    if (attack_from_ >= 0 && attack_drag_) {
      // Soltar sobre uma carta do oponente a ataca; sobre o lado vazio dele, ataque direto.
      if (CanAct()) {
        const int target = enemy_board_.PickSlot(pos, arena_view_proj_, ctx.logical_width,
                                                 ctx.logical_height);
        if (target >= 0 && !enemy_board_.Empty(target)) {
          PerformAttack(Side::kPlayer, attack_from_, target);
        } else if (target >= 0 && enemy_board_.Count() == 0) {
          PerformAttack(Side::kPlayer, attack_from_, -1);
        }
      }
    } else if (!inspecting && press_.x >= 0 && moved < kClickSlop && hand_.Hovered() < 0 &&
               CanAct()) {
      // Clique (sem arrasto) num slot do seu lado: com carta selecionada, invoca (num slot
      // ocupado a carta anterior é descartada); sem seleção, numa carta sua, alterna ataque e
      // defesa. Com o mouse sobre uma carta da mão o clique é dela, não do slot atrás.
      const int slot =
          board_.PickSlot(pos, arena_view_proj_, ctx.logical_width, ctx.logical_height);
      if (CanSummon()) {
        const bool field = CandidateIsField();
        // Carta de campo: qualquer slot da arena serve (o slot de campo não está modelado).
        const int enemy_slot = enemy_board_.PickSlot(pos, arena_view_proj_, ctx.logical_width,
                                                     ctx.logical_height);
        if (slot >= 0 || (field && enemy_slot >= 0)) Summon(field ? 0 : slot);
      } else if (slot >= 0 && board_.TogglePosition(slot)) {
        char name[3];
        SDL_Log("Turno %d: carta de %s em modo de %s", turn_, SlotName(slot, name),
                board_.At(slot)->defense ? "defesa" : "ataque");
      }
    }
    press_ = {-1, -1};
    attack_from_ = -1;
    attack_drag_ = false;
  }
  if (event.type == SDL_EVENT_MOUSE_WHEEL) {
    hand_.Wheel(event.wheel.y);
  }
  if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE) {
    mouse_ = {-1, -1};
  }
}

void BattleScene::Update(SceneContext& ctx, float dt_seconds) {
  if (!ready_ || paused_) return;  // pausa: abertura, câmera, IA e animações congelam
  UpdateIntro(dt_seconds);
  // Virada de campo.
  if (orbit_ < orbit_target_) orbit_ = std::min(orbit_target_, orbit_ + dt_seconds / kOrbitSeconds);
  if (orbit_ > orbit_target_) orbit_ = std::max(orbit_target_, orbit_ - dt_seconds / kOrbitSeconds);
  UpdateCamera();
  // Partida começa quando as tochas já estão acesas.
  if (!started_ && intro_time_ >= kMatchStart) StartMatch();
  // Passos agendados (turno do computador, impacto dos ataques): um por vez, com pausa.
  if (!steps_.empty()) {
    step_timer_ += dt_seconds;
    if (step_timer_ >= steps_.front().first) {
      step_timer_ = 0.0f;
      auto fn = std::move(steps_.front().second);
      steps_.pop_front();
      fn();
    }
  } else {
    step_timer_ = 0.0f;
  }

  hand_.Update(dt_seconds, mouse_, hand_view_proj_, ctx.logical_width, ctx.logical_height);
  // Destaques do campo: slots vazios em modo de invocação; moldura amarela no slot sob o
  // mouse quando dá para invocar nele, trocar a posição da carta ou, num arrasto de ataque,
  // no alvo do oponente.
  const bool summoning = CanSummon();
  board_.SetHighlight(summoning);
  int hover = -1;
  int enemy_hover = -1;
  if (attack_from_ >= 0 && attack_drag_) {
    hover = attack_from_;
    const int target = enemy_board_.PickSlot(mouse_, arena_view_proj_, ctx.logical_width,
                                             ctx.logical_height);
    if (target >= 0 && (!enemy_board_.Empty(target) || enemy_board_.Count() == 0)) {
      enemy_hover = target;
    }
  } else if (CanAct() && hand_.Hovered() < 0) {
    hover = board_.PickSlot(mouse_, arena_view_proj_, ctx.logical_width, ctx.logical_height);
    if (!summoning && board_.Empty(hover)) hover = -1;
  }
  board_.SetHoverSlot(hover);
  enemy_board_.SetHoverSlot(enemy_hover);
  board_.Update(dt_seconds);
  enemy_board_.Update(dt_seconds);
  sword_.Update(dt_seconds);
  // Tooltip e ampliação: carta de qualquer lado sob o mouse (fora da mão e sem inspeção).
  tooltip_slot_ = -1;
  if (intro_ >= 1.0f && hand_.Hovered() < 0 && !hand_.Inspecting() && mouse_.x >= 0) {
    for (Side side : {Side::kPlayer, Side::kEnemy}) {
      const Board& board = side == Side::kPlayer ? board_ : enemy_board_;
      const int slot =
          board.PickSlot(mouse_, arena_view_proj_, ctx.logical_width, ctx.logical_height);
      if (slot >= 0 && !board.Empty(slot)) {
        tooltip_slot_ = slot;
        tooltip_side_ = side;
        break;
      }
    }
  }
  board_.SetHoverCard(tooltip_slot_ >= 0 && tooltip_side_ == Side::kPlayer ? tooltip_slot_ : -1);
  enemy_board_.SetHoverCard(tooltip_slot_ >= 0 && tooltip_side_ == Side::kEnemy ? tooltip_slot_
                                                                                : -1);
}

void BattleScene::RenderTooltip(SDL_Renderer* r, int width, int height) const {
  if (tooltip_slot_ < 0) return;
  const Board& board = tooltip_side_ == Side::kPlayer ? board_ : enemy_board_;
  const Board::Card* card = board.At(tooltip_slot_);
  if (card == nullptr || card->info == nullptr) return;
  const CardInfo& info = *card->info;
  const int attack = rules::EffectiveAttack(info, field_);
  const int defense = rules::EffectiveDefense(info, field_);

  // Linhas de texto: nome; categoria e hierarquia; ATK/DEF efetivos (e base, se alterados);
  // descrição quebrada; dono e posição.
  struct Line {
    std::string text;
    float scale;
    SDL_Color color;
    float gap_after;
  };
  std::vector<Line> lines;
  const SDL_Color title = colors::Get("panel.title");
  const SDL_Color label = colors::Get("panel.label");
  const SDL_Color value = colors::Get("panel.value");
  lines.push_back({info.name, kTipScale, title, kTipGap});
  std::string kind = info.CategoryLabel();
  if (info.hierarchy > 0) {
    kind += " - " + i18n::Format("battle.tooltip.hierarchy", {std::to_string(info.hierarchy)});
    if (!info.hierarchy_name.empty()) kind += " (" + info.hierarchy_name + ")";
  }
  lines.push_back({kind, kTipDetailScale, label, kTipGap * 2});
  auto stat_color = [&](int effective, int base) {
    return effective > base ? colors::Get("status.buff")
           : effective < base ? colors::Get("status.nerf")
                              : value;
  };
  std::string atk = i18n::Get("cardface.atk") + " " + std::to_string(attack);
  if (attack != info.attack) {
    atk += " " + i18n::Format("battle.tooltip.base", {std::to_string(info.attack)});
  }
  std::string def = i18n::Get("cardface.def") + " " + std::to_string(defense);
  if (defense != info.defense) {
    def += " " + i18n::Format("battle.tooltip.base", {std::to_string(info.defense)});
  }
  lines.push_back({atk, kTipScale, stat_color(attack, info.attack), kTipGap});
  lines.push_back({def, kTipScale, stat_color(defense, info.defense), kTipGap * 2});
  for (const std::string& part : WrapText(info.description, kTipTextW, kTipDetailScale)) {
    lines.push_back({part, kTipDetailScale, colors::Get("text.secondary"), kTipGap * 0.6f});
  }
  if (!info.description.empty()) lines.back().gap_after = kTipGap * 2;
  std::string owner = i18n::Get(tooltip_side_ == Side::kPlayer ? "battle.tooltip.yourCard"
                                                                 : "battle.tooltip.opponentCard");
  owner += " - " + i18n::Get(card->defense ? "battle.tooltip.inDefense" : "battle.tooltip.inAttack");
  if (tooltip_side_ == Side::kPlayer && !card->defense && card->attacked) {
    owner += " " + i18n::Get("battle.tooltip.attacked");
  }
  lines.push_back({owner, kTipDetailScale, label, 0.0f});

  // Tamanho: imagem à esquerda (kTipImageH de altura), coluna de texto à direita.
  float text_w = 0.0f;
  float text_h = 0.0f;
  for (const Line& line : lines) {
    text_w = std::max(text_w, text::Width(line.text.c_str(), line.scale));
    text_h += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * line.scale + line.gap_after;
  }
  const float image_w = kTipImageH * kCardAspect;
  const float w = ui::SnapUp(3 * kTipPadding + image_w + std::max(text_w, kTipTextW * 0.6f));
  const float h = ui::SnapUp(2 * kTipPadding + std::max(kTipImageH, text_h));
  // Perto do cursor, sem sair da tela.
  float x = mouse_.x + kTipOffset;
  float y = mouse_.y + kTipOffset;
  if (x + w > width) x = mouse_.x - kTipOffset - w;
  if (y + h > height) y = std::max(0.0f, mouse_.y - kTipOffset - h);
  const SDL_FRect panel{x, y, w, h};
  SDL_Color c;
  if (frame::Ready()) {
    frame::Draw(r, panel, frame::Kind::kWindow);
  } else {
    c = colors::Get("panel.fill");
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &panel);
    c = colors::Get("panel.border");
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderRect(r, &panel);
  }

  // Imagem: frente composta da carta (mesma do HUD), em cache por código.
  const SDL_FRect image{x + kTipPadding, y + kTipPadding, image_w, kTipImageH};
  SDL_Texture* face = const_cast<BattleScene*>(this)->HudFace(info);
  if (face != nullptr) {
    SDL_RenderTexture(r, face, nullptr, &image);
  } else {
    c = colors::Get("border.subtle");
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &image);
  }
  c = colors::Get("panel.border");
  SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
  SDL_RenderRect(r, &image);

  const float tx = image.x + image.w + kTipPadding;
  float ty = y + kTipPadding;
  for (const Line& line : lines) {
    text::Draw(r, line.text.c_str(), tx, ty, line.scale, line.color);
    ty += SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE * line.scale + line.gap_after;
  }
}

void BattleScene::Render(SceneContext& ctx) {
  SDL_Renderer* r = ctx.renderer;
  const SDL_Color fg = colors::Get("text.primary");
  const SDL_Color background = colors::Get("background.fog");
  const float cx = ctx.logical_width / 2.0f;

  if (ready_) {
    // Passes de sombra (um por tocha): arena, corpos das tochas e cartas em campo são os
    // ocultadores.
    for (int k = 0; k < 2; ++k) {
      if (!gpu_->BeginShadowPass(k, torch_light_vp_[k])) break;
      if (arena_model_ != nullptr) arena_model_->DrawDepth(*gpu_, arena_world_);
      if (torch_ != nullptr) {
        for (const math::Mat4& world : torch_world_) torch_->DrawBodyDepth(*gpu_, world);
      }
      board_.RenderDepth(*gpu_);
      enemy_board_.RenderDepth(*gpu_);
      gpu_->EndShadowPass();
    }
  }
  if (ready_ && gpu_->BeginFrame(fog_color_)) {
    gpu_->SetAtmosphere(arena_atmosphere_);
    if (arena_model_ != nullptr) {
      arena_model_->Draw(*gpu_, arena_world_, arena_view_proj_, SDL_FColor{1, 1, 1, 1});
    }
    if (torch_ != nullptr) {
      for (const math::Mat4& world : torch_world_) {
        torch_->DrawBody(*gpu_, world, arena_view_proj_);
      }
    }
    // Cartas dos dois lados do campo e destaques dos slots.
    board_.Render(*gpu_, arena_view_proj_);
    enemy_board_.Render(*gpu_, arena_view_proj_);
    sword_.Render(*gpu_, arena_view_proj_);
    // Névoa depois da arena (transparente): de baixo para cima.
    for (const MistLayer& layer : mist_layers_) {
      gpu_->Draw(mist_quad_, nullptr, math::Mat4::Translate({0, layer.y, 0}), arena_view_proj_,
                 SDL_FColor{mist_color_.r, mist_color_.g, mist_color_.b, layer.alpha},
                 Gpu::DrawMode::kMist);
    }
    // Abertura: névoa densa acima da base, que se dissipa conforme a arena emerge.
    if (intro_ < 1.0f) {
      const float remain = 1.0f - intro_;
      Atmosphere dense = arena_atmosphere_;
      // Manto fechado no início, fiapos no fim (Fog.json, battle.introMist).
      dense.mist_density = config::Float("fog", "battle.introMist.density", 0.3f) * remain;
      dense.mist_fade_start = config::Float("fog", "battle.introMist.fadeStart", 10.0f);
      dense.mist_fade_end = config::Float("fog", "battle.introMist.fadeEnd", 30.0f);
      gpu_->SetAtmosphere(dense);
      for (const MistLayer& layer : intro_mist_layers_) {
        gpu_->Draw(mist_quad_, nullptr, math::Mat4::Translate({0, layer.y, 0}),
                   arena_view_proj_,
                   SDL_FColor{mist_color_.r, mist_color_.g, mist_color_.b,
                              layer.alpha * std::sqrt(remain)},
                   Gpu::DrawMode::kMist);
      }
    }
    // Chamas por cima da névoa (transparentes, escrevem profundidade).
    if (torch_ != nullptr) {
      gpu_->SetAtmosphere(arena_atmosphere_);
      for (const math::Mat4& world : torch_world_) {
        torch_->DrawFlame(*gpu_, world, arena_view_proj_, arena_atmosphere_.camera_pos,
                          ignite_);
      }
    }
    gpu_->NewLayer();  // a mão fica sempre por cima da arena
    gpu_->SetAtmosphere(hand_atmosphere_);
    hand_.Render(*gpu_, hand_view_proj_);
    gpu_->EndFrame();

    SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
    SDL_RenderClear(r);
    SDL_RenderTexture(r, gpu_->Target(), nullptr, nullptr);
  } else {
    SDL_SetRenderDrawColor(r, background.r, background.g, background.b, background.a);
    SDL_RenderClear(r);
    text::DrawCentered(r, i18n::Get("battle.noRender").c_str(), cx, ctx.logical_height * 0.5f,
                       2.0f * ui::kScale, fg);
  }

  // Topo: situação do turno e, abaixo, a dica do que fazer.
  if (started_ && !hud_.Over()) {
    const std::string status = i18n::Format(
        "battle.status",
        {std::to_string(turn_),
         i18n::Get(turn_side_ == Side::kPlayer && player_turn_ ? "battle.status.yourTurn"
                                                               : "battle.status.opponent"),
         i18n::Get(ai::DifficultyKey(difficulty_)), std::to_string(enemy_hand_.size())});
    text::DrawCentered(r, status.c_str(), cx, kStatusY, kStatusScale,
                       colors::Get("text.secondary"));
  }
  // Arrasto de ataque: linha da carta atacante até o mouse.
  if (attack_from_ >= 0 && attack_drag_ && mouse_.x >= 0) {
    const math::Vec2 from =
        ScreenOf(board_.CardPosition(attack_from_), ctx.logical_width, ctx.logical_height);
    const SDL_Color line = colors::Get("board.slotHover");
    SDL_SetRenderDrawColor(r, line.r, line.g, line.b, line.a);
    for (int k = -1; k <= 1; ++k) {
      SDL_RenderLine(r, from.x + k, from.y, mouse_.x + k, mouse_.y);
      SDL_RenderLine(r, from.x, from.y + k, mouse_.x, mouse_.y + k);
    }
  }
  // Dicas do seu turno.
  const char* hint = nullptr;  // chave em translate.json
  if (attack_from_ >= 0 && attack_drag_) {
    hint = enemy_board_.Count() == 0 ? "battle.hint.directAttack" : "battle.hint.attackCard";
  } else if (CanSummon()) {
    hint = CandidateIsField() ? "battle.hint.playField" : "battle.hint.summon";
  } else if (CanAct() && AttacksAllowed(Side::kPlayer)) {
    for (int slot : board_.Occupied()) {
      const Board::Card* card = board_.At(slot);
      if (!card->defense && !card->attacked) {
        hint = "battle.hint.attack";
        break;
      }
    }
  }
  if (hint != nullptr) {
    text::DrawCentered(r, i18n::Get(hint).c_str(), cx, kHintY, kHintScale,
                       colors::Get("board.hint"));
  }
  hud_.Render(r, ctx.logical_width, ctx.logical_height);
  if (CanAct()) end_turn_->Render(r);
  sound_button_.Render(r, ctx.music != nullptr && ctx.music->Muted());
  if (!paused_) RenderTooltip(r, ctx.logical_width, ctx.logical_height);
  abandon_dialog_.Render(r, ctx.logical_width, ctx.logical_height);
}
