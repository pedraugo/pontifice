#include "scripts/music/Music.h"

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include <dr_mp3.h>

#include "scripts/json/Json.h"

namespace {

constexpr float kFadeSeconds = 2.0f;  // fade in no início e fade out no fim de cada faixa
constexpr float kGapSeconds = 3.0f;   // silêncio entre uma faixa e a próxima

}  // namespace

struct Music::Decoder {
  drmp3 mp3{};
  bool open = false;
  std::vector<uint8_t> file;  // MP3 inteiro em memória: dr_mp3 lê daqui, sem stdio
};

Music::~Music() { Shutdown(); }

bool Music::Init(const std::string& base_path) {
  base_ = base_path;
  json::Value root;
  if (!json::LoadFile(base_ + "assets/ost.json", root)) return false;
  if (const json::Value* volume = root.Find("volume"); volume != nullptr && volume->IsNumber()) {
    volume_ = static_cast<float>(SDL_clamp(volume->number, 0.0, 1.0));
  }
  if (const json::Value* tracks = root.Find("tracks"); tracks != nullptr && tracks->IsArray()) {
    for (const json::Value& track : tracks->array) {
      if (track.IsString()) tracks_.push_back(track.string);
    }
  }
  if (tracks_.empty()) {
    SDL_Log("ost.json sem faixas; jogo segue sem musica");
    return false;
  }

  decoder_ = new Decoder;
  if (!OpenTrack(0)) {
    SDL_Log("Nenhuma faixa de ost.json pode ser aberta; jogo segue sem musica");
    Shutdown();
    return false;
  }

  // O stream converte do formato da faixa (f32, canais e taxa do MP3) para o do dispositivo.
  const SDL_AudioSpec spec{SDL_AUDIO_F32, static_cast<int>(decoder_->mp3.channels),
                           static_cast<int>(decoder_->mp3.sampleRate)};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &Music::Feed,
                                      this);
  if (stream_ == nullptr) {
    SDL_Log("SDL_OpenAudioDeviceStream: %s; jogo segue sem musica", SDL_GetError());
    Shutdown();
    return false;
  }
  SDL_SetAudioStreamGain(stream_, volume_);
  if (!SDL_ResumeAudioStreamDevice(stream_)) {
    SDL_Log("SDL_ResumeAudioStreamDevice: %s", SDL_GetError());
  }
  SDL_AudioSpec device_spec{};
  SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream_), &device_spec, nullptr);
  SDL_Log("Musica: %zu faixa(s), volume %.0f%%, dispositivo %d canais @ %d Hz%s",
          tracks_.size(), volume_ * 100.0f, device_spec.channels, device_spec.freq,
          SDL_AudioDevicePaused(SDL_GetAudioStreamDevice(stream_)) ? " (pausado)" : "");
  return true;
}

void Music::Shutdown() {
  // Destruir o stream primeiro: depois disso o callback não roda mais e o decoder é só nosso.
  if (stream_ != nullptr) {
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
  }
  CloseTrack();
  delete decoder_;
  decoder_ = nullptr;
  tracks_.clear();
}

void Music::CloseTrack() {
  if (decoder_ != nullptr && decoder_->open) {
    drmp3_uninit(&decoder_->mp3);
    decoder_->mp3 = drmp3{};
    decoder_->open = false;
  }
}

bool Music::OpenTrack(size_t index) {
  CloseTrack();
  const std::string path = base_ + tracks_[index];
  size_t size = 0;
  void* data = SDL_LoadFile(path.c_str(), &size);
  if (data == nullptr) {
    SDL_Log("Musica: nao abriu %s: %s", path.c_str(), SDL_GetError());
    return false;
  }
  decoder_->file.assign(static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
  SDL_free(data);
  if (!drmp3_init_memory(&decoder_->mp3, decoder_->file.data(), decoder_->file.size(),
                         nullptr)) {
    SDL_Log("Musica: %s nao e um MP3 valido", path.c_str());
    decoder_->mp3 = drmp3{};
    return false;
  }
  decoder_->open = true;
  current_ = index;
  position_ = 0;
  // Duração: varre os cabeçalhos do arquivo (rápido, arquivo já em memória) e volta ao início.
  total_frames_ = drmp3_get_pcm_frame_count(&decoder_->mp3);
  drmp3_seek_to_pcm_frame(&decoder_->mp3, 0);
  SDL_Log("Musica: tocando %s", tracks_[index].c_str());
  return true;
}

bool Music::OpenNextTrack() {
  for (size_t tries = 0; tries < tracks_.size(); ++tries) {
    const size_t next = (current_ + 1) % tracks_.size();
    if (OpenTrack(next)) return true;
    current_ = next;  // pula a faixa que falhou
  }
  return false;
}

// Ganho por quadro: sobe de 0 a 1 em kFadeSeconds no início e desce a 0 no fim da faixa.
void Music::ApplyFade(float* samples, size_t frames, int channels) const {
  const float fade_frames = kFadeSeconds * static_cast<float>(decoder_->mp3.sampleRate);
  for (size_t i = 0; i < frames; ++i) {
    const float pos = static_cast<float>(position_ + i);
    float gain = SDL_min(pos / fade_frames, 1.0f);
    if (total_frames_ > 0) {
      const float remaining = static_cast<float>(total_frames_ - SDL_min(position_ + i, total_frames_));
      gain = SDL_min(gain, remaining / fade_frames);
    }
    for (int c = 0; c < channels; ++c) samples[i * channels + c] *= gain;
  }
}

void SDLCALL Music::Feed(void* userdata, SDL_AudioStream* stream, int additional_bytes,
                         int /*total_bytes*/) {
  Music* self = static_cast<Music*>(userdata);
  Decoder* dec = self->decoder_;
  if (additional_bytes <= 0 || dec == nullptr || !dec->open) return;

  const int channels = static_cast<int>(dec->mp3.channels);
  drmp3_uint64 frames_wanted =
      static_cast<drmp3_uint64>(additional_bytes) / (sizeof(float) * channels) + 1;
  self->buffer_.resize(static_cast<size_t>(frames_wanted) * channels);

  while (frames_wanted > 0) {
    const drmp3_uint64 got =
        drmp3_read_pcm_frames_f32(&dec->mp3, frames_wanted, self->buffer_.data());
    if (got > 0) {
      self->ApplyFade(self->buffer_.data(), static_cast<size_t>(got), channels);
      SDL_PutAudioStreamData(stream, self->buffer_.data(),
                             static_cast<int>(got * channels * sizeof(float)));
      self->position_ += got;
      frames_wanted -= got;
      continue;
    }
    // Fim da faixa: enfileira o intervalo de silêncio (no formato atual) e, enquanto ele
    // toca, abre a próxima faixa; o próximo Feed já parte dela, em fade in.
    const size_t gap_frames = static_cast<size_t>(kGapSeconds * dec->mp3.sampleRate);
    self->buffer_.assign(gap_frames * channels, 0.0f);
    SDL_PutAudioStreamData(stream, self->buffer_.data(),
                           static_cast<int>(gap_frames * channels * sizeof(float)));
    if (!self->OpenNextTrack()) return;  // nenhuma faixa abre: silêncio daqui em diante
    // Formato da nova faixa (canais/taxa podem diferir): o stream reconverte a partir daqui.
    const SDL_AudioSpec spec{SDL_AUDIO_F32, static_cast<int>(dec->mp3.channels),
                             static_cast<int>(dec->mp3.sampleRate)};
    SDL_SetAudioStreamFormat(stream, &spec, nullptr);
    return;
  }
}
