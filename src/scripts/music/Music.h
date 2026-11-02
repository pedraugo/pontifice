#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

// Trilha sonora de fundo: lê assets/ost.json (volume por cena e categorias de MP3 relativos
// ao binário), decodifica com dr_mp3 e toca em sequência, em loop, por um SDL_AudioStream.
// Cada faixa entra e sai em fade (kFadeSeconds) e há um intervalo de silêncio (kGapSeconds)
// entre uma e outra. A decodificação acontece sob demanda no callback do stream (thread de
// áudio); a thread principal só chama Init, Shutdown, SetVolume e SetCategory (volume em
// rampa e troca de categoria com fade out da faixa atual e fade in da nova, na thread de
// áudio).
// ost.json: "volume" número ou objeto por cena {"menu": 0.15, "battle": 0.25};
// "categories" {"ost": [...], "battle": [...]} ("ost" toca no menu e no loading, "battle" na
// batalha). "random": ["battle"] lista as categorias tocadas em ordem aleatória (cada faixa
// sorteada, sem repetir a que acabou de tocar); as outras tocam em sequência.
// "tracks" (lista única) continua aceito como categoria "ost". Cada faixa é uma
// string ou {"file": "...", "gain": 1.3}: o ganho normaliza o volume entre faixas
// masterizadas em níveis diferentes (PONTIFICE_AUDIO_ANALYZE=1 mede o RMS de cada uma e
// sugere o ganho para uma referência comum).
class Music {
 public:
  Music() = default;
  ~Music();
  Music(const Music&) = delete;
  Music& operator=(const Music&) = delete;

  // base_path: diretório do binário (SDL_GetBasePath). Falha sem áudio é só registrada: o
  // jogo segue em silêncio.
  bool Init(const std::string& base_path);
  void Shutdown();

  // Volume alvo (0 a 1); a rampa até ele leva kRampSeconds. SetScene usa o volume da chave
  // em ost.json ("menu", "battle"); chave desconhecida mantém o atual.
  void SetVolume(float volume);
  void SetScene(const std::string& key);
  float Volume() const { return target_volume_.load(); }
  // Música ligada/desligada (botão de alto-falante): desligar leva o volume a zero em rampa
  // e ligar volta ao volume da cena.
  void SetMuted(bool muted) { muted_.store(muted); }
  bool Muted() const { return muted_.load(); }
  void ToggleMuted() { muted_.store(!muted_.load()); }
  // Troca a categoria de faixas ("ost", "battle"): a atual sai em fade out (kSwitchSeconds)
  // e a primeira da nova entra em fade in. Mesma categoria ou desconhecida: nada. A faixa e a
  // posição de onde a categoria que sai parou ficam guardadas: com `resume`, a nova categoria
  // continua de onde parou da última vez (em fade in), como uma pausa; sem histórico, começa
  // da primeira faixa. Pedir a categoria atual durante o fade out de uma troca cancela a
  // troca e a faixa volta em fade in.
  void SetCategory(const std::string& name, bool resume = false);

 private:
  struct Decoder;  // esconde dr_mp3 do header

  static void SDLCALL Feed(void* userdata, SDL_AudioStream* stream, int additional_bytes,
                           int total_bytes);
  bool OpenTrack(size_t index);  // thread de áudio (ou Init, antes de o stream rodar)
  bool OpenNextTrack();          // tenta as faixas seguintes até uma abrir
  void CloseTrack();
  void ApplyFade(float* samples, size_t frames, int channels);

  struct Track {
    std::string file;
    float gain = 1.0f;  // normalização de volume da faixa
  };
  struct Category {
    std::string name;
    std::vector<Track> tracks;
    bool random = false;  // ordem sorteada (ost.json "random")
  };
  void Analyze();  // PONTIFICE_AUDIO_ANALYZE=1: RMS e ganho sugerido de cada faixa (log)

  std::string base_;
  std::vector<Category> categories_;
  size_t active_ = 0;                  // categoria em uso (thread de áudio)
  // Pedido da thread principal: índice * 2 + (1 se retoma), ou -1. Um só atômico para o
  // índice e a retomada chegarem juntos à thread de áudio.
  std::atomic<int> pending_category_{-1};
  int switching_to_ = -1;              // troca em andamento: categoria destino
  bool switch_resume_ = false;         // a troca em andamento retoma a categoria destino
  // Onde cada categoria parou quando saiu (para SetCategory com resume).
  struct Saved {
    size_t track = 0;
    uint64_t position = 0;  // quadros
    bool valid = false;
  };
  std::vector<Saved> saved_;
  uint64_t fade_in_left_ = 0;          // fade in de retomada (quadros restantes / total)
  uint64_t fade_in_total_ = 0;
  uint64_t switch_left_ = 0;           // quadros restantes do fade out da troca
  uint64_t switch_total_ = 0;
  std::vector<Track> tracks_;          // faixas da categoria ativa
  bool random_ = false;                // a categoria ativa toca em ordem sorteada
  std::mt19937 rng_;                   // sorteio das faixas (thread de áudio, após Init)
  float track_gain_ = 1.0f;            // ganho da faixa atual
  std::map<std::string, float> scene_volumes_;  // "menu", "battle"...
  std::atomic<float> target_volume_{0.3f};
  std::atomic<bool> muted_{false};
  float volume_ = 0.3f;  // volume atual aplicado (thread de áudio), em rampa até o alvo
  size_t current_ = 0;
  Decoder* decoder_ = nullptr;
  SDL_AudioStream* stream_ = nullptr;
  std::vector<float> buffer_;  // amostras intercaladas decodificadas em um Feed
  uint64_t position_ = 0;      // quadros já tocados da faixa atual
  // Depuração (PONTIFICE_AUDIO_LOG=1): pico e RMS do que foi enviado no último segundo.
  bool debug_log_ = false;
  double debug_sum_sq_ = 0.0;
  float debug_peak_ = 0.0f;
  uint64_t debug_frames_ = 0;
  uint64_t total_frames_ = 0;  // duração da faixa atual em quadros (0 = desconhecida)
};
