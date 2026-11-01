#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

// Trilha sonora de fundo: lê assets/ost.json (volume e lista de MP3 relativos ao binário),
// decodifica com dr_mp3 e toca em sequência, em loop, por um SDL_AudioStream. Cada faixa
// entra e sai em fade (kFadeSeconds) e há um intervalo de silêncio (kGapSeconds) entre uma e
// outra. A decodificação acontece sob demanda no callback do stream (thread de áudio); a
// thread principal só chama Init e Shutdown.
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

 private:
  struct Decoder;  // esconde dr_mp3 do header

  static void SDLCALL Feed(void* userdata, SDL_AudioStream* stream, int additional_bytes,
                           int total_bytes);
  bool OpenTrack(size_t index);  // thread de áudio (ou Init, antes de o stream rodar)
  bool OpenNextTrack();          // tenta as faixas seguintes até uma abrir
  void CloseTrack();
  void ApplyFade(float* samples, size_t frames, int channels) const;

  std::string base_;
  std::vector<std::string> tracks_;
  float volume_ = 0.3f;
  size_t current_ = 0;
  Decoder* decoder_ = nullptr;
  SDL_AudioStream* stream_ = nullptr;
  std::vector<float> buffer_;  // amostras intercaladas decodificadas em um Feed
  uint64_t position_ = 0;      // quadros já tocados da faixa atual
  uint64_t total_frames_ = 0;  // duração da faixa atual em quadros (0 = desconhecida)
};
