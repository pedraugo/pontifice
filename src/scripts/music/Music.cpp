#include "scripts/music/Music.h"

#include <algorithm>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include <dr_mp3.h>

#include "scripts/json/Json.h"

namespace {

constexpr float kFadeSeconds = 2.0f;  // fade in no início e fade out no fim de cada faixa
constexpr float kGapSeconds = 3.0f;   // silêncio entre uma faixa e a próxima
constexpr float kRampSeconds = 1.0f;  // transição de volume entre cenas
constexpr float kSwitchSeconds = 1.5f;  // fade out da faixa ao trocar de categoria

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
  if (const json::Value* volume = root.Find("volume")) {
    if (volume->IsNumber()) {
      volume_ = static_cast<float>(SDL_clamp(volume->number, 0.0, 1.0));
    } else if (volume->IsObject()) {
      for (const auto& [key, value] : volume->object) {
        if (value.IsNumber()) {
          scene_volumes_[key] = static_cast<float>(SDL_clamp(value.number, 0.0, 1.0));
        }
      }
      // Começa no volume do menu (primeira cena), se houver.
      if (scene_volumes_.count("menu")) volume_ = scene_volumes_["menu"];
    }
  }
  target_volume_.store(volume_);
  debug_log_ = SDL_getenv("PONTIFICE_AUDIO_LOG") != nullptr;
  // Categorias de faixas; "tracks" solto vira a categoria "ost". Faixa: string ou
  // {"file", "gain"}.
  auto read_list = [](const json::Value& list) {
    std::vector<Track> out;
    if (!list.IsArray()) return out;
    for (const json::Value& track : list.array) {
      if (track.IsString()) {
        out.push_back(Track{track.string, 1.0f});
      } else if (track.IsObject()) {
        const json::Value* file = track.Find("file");
        const json::Value* gain = track.Find("gain");
        if (file != nullptr && file->IsString()) {
          out.push_back(Track{file->string, gain != nullptr && gain->IsNumber()
                                                ? static_cast<float>(gain->number)
                                                : 1.0f});
        }
      }
    }
    return out;
  };
  if (const json::Value* categories = root.Find("categories");
      categories != nullptr && categories->IsObject()) {
    for (const auto& [name, list] : categories->object) {
      Category category{name, read_list(list)};
      if (!category.tracks.empty()) categories_.push_back(std::move(category));
    }
  }
  if (const json::Value* tracks = root.Find("tracks"); tracks != nullptr) {
    Category category{"ost", read_list(*tracks)};
    if (!category.tracks.empty()) categories_.push_back(std::move(category));
  }
  // Categorias em ordem aleatória: "random": ["battle"].
  if (const json::Value* random = root.Find("random"); random != nullptr && random->IsArray()) {
    for (const json::Value& name : random->array) {
      if (!name.IsString()) continue;
      for (Category& category : categories_) {
        if (category.name == name.string) category.random = true;
      }
    }
  }
  rng_.seed(static_cast<unsigned>(std::random_device{}() ^ SDL_GetTicksNS()));
  if (categories_.empty()) {
    SDL_Log("ost.json sem faixas; jogo segue sem musica");
    return false;
  }
  // Começa na categoria "ost" (menu), se existir; senão na primeira.
  active_ = 0;
  for (size_t i = 0; i < categories_.size(); ++i) {
    if (categories_[i].name == "ost") active_ = i;
  }
  tracks_ = categories_[active_].tracks;
  random_ = categories_[active_].random;
  saved_.assign(categories_.size(), Saved{});
  if (SDL_getenv("PONTIFICE_AUDIO_ANALYZE") != nullptr) Analyze();

  decoder_ = new Decoder;
  current_ = tracks_.size();  // nenhuma tocou: sequencial começa na 0, aleatória sorteia
  if (!OpenNextTrack()) {
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
  // O volume é aplicado nas amostras (ApplyFade), em rampa; o ganho do stream fica em 1.
  if (!SDL_ResumeAudioStreamDevice(stream_)) {
    SDL_Log("SDL_ResumeAudioStreamDevice: %s", SDL_GetError());
  }
  SDL_AudioSpec device_spec{};
  SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream_), &device_spec, nullptr);
  SDL_Log("Musica: %zu categoria(s), \"%s\" com %zu faixa(s), volume %.0f%%, dispositivo %d "
          "canais @ %d Hz%s",
          categories_.size(), categories_[active_].name.c_str(), tracks_.size(),
          volume_ * 100.0f, device_spec.channels, device_spec.freq,
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
  const std::string path = base_ + tracks_[index].file;
  track_gain_ = tracks_[index].gain;
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
  fade_in_left_ = fade_in_total_ = 0;
  // Duração: varre os cabeçalhos do arquivo (rápido, arquivo já em memória) e volta ao início.
  total_frames_ = drmp3_get_pcm_frame_count(&decoder_->mp3);
  drmp3_seek_to_pcm_frame(&decoder_->mp3, 0);
  SDL_Log("Musica: tocando %s (ganho %.2f)", tracks_[index].file.c_str(), track_gain_);
  return true;
}

// Mede o RMS de cada faixa de todas as categorias (decodifica inteira) e sugere o ganho que
// a leva à referência kReferenceRms. Só depuração: PONTIFICE_AUDIO_ANALYZE=1 no Init.
void Music::Analyze() {
  constexpr double kReferenceRms = 0.10;  // -20 dBFS
  for (const Category& category : categories_) {
    for (const Track& track : category.tracks) {
      const std::string path = base_ + track.file;
      size_t size = 0;
      void* data = SDL_LoadFile(path.c_str(), &size);
      if (data == nullptr) continue;
      drmp3 mp3{};
      if (!drmp3_init_memory(&mp3, data, size, nullptr)) {
        SDL_free(data);
        continue;
      }
      std::vector<float> chunk(4096 * mp3.channels);
      double sum_sq = 0.0;
      uint64_t samples = 0;
      for (;;) {
        const drmp3_uint64 got = drmp3_read_pcm_frames_f32(&mp3, 4096, chunk.data());
        if (got == 0) break;
        for (drmp3_uint64 i = 0; i < got * mp3.channels; ++i) {
          sum_sq += static_cast<double>(chunk[i]) * chunk[i];
        }
        samples += got * mp3.channels;
      }
      drmp3_uninit(&mp3);
      SDL_free(data);
      const double rms = samples > 0 ? SDL_sqrt(sum_sq / samples) : 0.0;
      SDL_Log("Analise: %s [%s] RMS %.4f (%.1f dBFS) ganho sugerido %.2f", track.file.c_str(),
              category.name.c_str(), rms, 20.0 * SDL_log10(rms + 1e-9),
              rms > 0.0 ? kReferenceRms / rms : 1.0);
    }
  }
}

// Próxima faixa da categoria ativa. current_ >= tracks_.size() = nenhuma tocou ainda.
// Sequencial: a seguinte (começa na 0). Aleatória: qualquer uma menos a que acabou de tocar,
// em ordem sorteada; se nenhuma abrir, tenta repetir a atual. Faixas que falham são puladas.
bool Music::OpenNextTrack() {
  const size_t n = tracks_.size();
  if (n == 0) return false;
  if (random_ && n > 1) {
    std::vector<size_t> order;
    for (size_t i = 0; i < n; ++i) {
      if (i != current_) order.push_back(i);
    }
    std::shuffle(order.begin(), order.end(), rng_);
    for (size_t i : order) {
      if (OpenTrack(i)) return true;
    }
    return current_ < n && OpenTrack(current_);
  }
  size_t at = current_ < n ? current_ : n - 1;
  for (size_t tries = 0; tries < n; ++tries) {
    at = (at + 1) % n;
    if (OpenTrack(at)) return true;
  }
  return false;
}

void Music::SetVolume(float volume) { target_volume_.store(SDL_clamp(volume, 0.0f, 1.0f)); }

void Music::SetCategory(const std::string& name, bool resume) {
  for (size_t i = 0; i < categories_.size(); ++i) {
    if (categories_[i].name == name) {
      pending_category_.store(static_cast<int>(i) * 2 + (resume ? 1 : 0));
      return;
    }
  }
  SDL_Log("Musica: categoria desconhecida \"%s\"", name.c_str());
}

void Music::SetScene(const std::string& key) {
  const auto it = scene_volumes_.find(key);
  if (it == scene_volumes_.end()) return;
  SetVolume(it->second);
  SDL_Log("Musica: volume %s %.0f%%", key.c_str(), it->second * 100.0f);
}

// Ganho por quadro: sobe de 0 a 1 em kFadeSeconds no início e desce a 0 no fim da faixa,
// vezes o volume, que caminha em rampa até o alvo (troca de cena). Numa retomada (faixa
// aberta no meio), o fade in vem de fade_in_left_/fade_in_total_.
void Music::ApplyFade(float* samples, size_t frames, int channels) {
  const float rate = static_cast<float>(decoder_->mp3.sampleRate);
  const float fade_frames = kFadeSeconds * rate;
  const float target = muted_.load() ? 0.0f : target_volume_.load();
  const float step = 1.0f / (kRampSeconds * rate);
  for (size_t i = 0; i < frames; ++i) {
    const float pos = static_cast<float>(position_ + i);
    float gain = SDL_min(pos / fade_frames, 1.0f);
    if (total_frames_ > 0) {
      const float remaining = static_cast<float>(total_frames_ - SDL_min(position_ + i, total_frames_));
      gain = SDL_min(gain, remaining / fade_frames);
    }
    if (fade_in_left_ > 0) {
      gain *= 1.0f - static_cast<float>(fade_in_left_) / static_cast<float>(fade_in_total_);
      --fade_in_left_;
    }
    if (volume_ < target) volume_ = SDL_min(volume_ + step, target);
    if (volume_ > target) volume_ = SDL_max(volume_ - step, target);
    gain *= volume_ * track_gain_;
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

  // Pedido de troca de categoria: começa o fade out da faixa atual.
  const int request = self->pending_category_.exchange(-1);
  if (request >= 0) {
    const int pending = request / 2;
    if (pending == static_cast<int>(self->active_) && self->switching_to_ >= 0) {
      // Voltou atrás no meio do fade out: cancela a troca e a faixa volta em fade in a
      // partir do ganho em que estava.
      self->fade_in_total_ = self->switch_total_;
      self->fade_in_left_ = self->switch_total_ - self->switch_left_;
      self->switching_to_ = -1;
    } else if (pending != static_cast<int>(self->active_) && self->switching_to_ < 0) {
      self->switching_to_ = pending;
      self->switch_resume_ = (request % 2) == 1;
      self->switch_total_ = self->switch_left_ =
          static_cast<uint64_t>(kSwitchSeconds * dec->mp3.sampleRate);
    }
  }

  while (frames_wanted > 0) {
    // Fade out da troca terminou: guarda onde a categoria que sai parou (antes do fade out,
    // para a retomada continuar do ponto do pedido) e entra a nova, do começo ou de onde
    // parou (retomada), em fade in.
    if (self->switching_to_ >= 0 && self->switch_left_ == 0) {
      const uint64_t faded = self->switch_total_;
      self->saved_[self->active_] = {self->current_,
                                     self->position_ > faded ? self->position_ - faded : 0, true};
      self->active_ = static_cast<size_t>(self->switching_to_);
      self->switching_to_ = -1;
      self->tracks_ = self->categories_[self->active_].tracks;
      self->random_ = self->categories_[self->active_].random;
      const Saved saved = self->saved_[self->active_];
      bool resumed = false;
      if (self->switch_resume_ && saved.valid && saved.track < self->tracks_.size() &&
          self->OpenTrack(saved.track)) {
        drmp3_seek_to_pcm_frame(&dec->mp3, saved.position);
        self->position_ = saved.position;
        self->fade_in_total_ = self->fade_in_left_ =
            static_cast<uint64_t>(kSwitchSeconds * dec->mp3.sampleRate);
        resumed = true;
      }
      SDL_Log("Musica: categoria \"%s\"%s", self->categories_[self->active_].name.c_str(),
              resumed ? " (retomada)" : "");
      if (!resumed) {
        // Nenhuma desta categoria tocou: sequencial começa na 0, aleatória sorteia.
        self->current_ = self->tracks_.size();
        if (!self->OpenNextTrack()) return;
      }
      const SDL_AudioSpec spec{SDL_AUDIO_F32, static_cast<int>(dec->mp3.channels),
                               static_cast<int>(dec->mp3.sampleRate)};
      SDL_SetAudioStreamFormat(stream, &spec, nullptr);
      return;
    }
    drmp3_uint64 to_read = frames_wanted;
    if (self->switching_to_ >= 0) to_read = SDL_min(to_read, self->switch_left_);
    const drmp3_uint64 got = drmp3_read_pcm_frames_f32(&dec->mp3, to_read, self->buffer_.data());
    if (got > 0) {
      self->ApplyFade(self->buffer_.data(), static_cast<size_t>(got), channels);
      if (self->switching_to_ >= 0) {
        // Fade out da troca por cima do fade da faixa.
        for (drmp3_uint64 i = 0; i < got; ++i) {
          const float g = static_cast<float>(self->switch_left_ - i) /
                          static_cast<float>(self->switch_total_);
          for (int c = 0; c < channels; ++c) self->buffer_[i * channels + c] *= g;
        }
        self->switch_left_ -= got;
      }
      SDL_PutAudioStreamData(stream, self->buffer_.data(),
                             static_cast<int>(got * channels * sizeof(float)));
      if (self->debug_log_) {
        for (size_t i = 0; i < got * channels; ++i) {
          const float v = self->buffer_[i];
          self->debug_sum_sq_ += static_cast<double>(v) * v;
          self->debug_peak_ = SDL_max(self->debug_peak_, SDL_fabsf(v));
        }
        self->debug_frames_ += got;
        if (self->debug_frames_ >= dec->mp3.sampleRate) {
          const double rms = SDL_sqrt(self->debug_sum_sq_ / (self->debug_frames_ * channels));
          SDL_Log("Musica: volume aplicado %.0f%% (alvo %.0f%%), pico %.2f, RMS %.3f (%.1f dBFS)",
                  self->volume_ * 100.0f, self->target_volume_.load() * 100.0f,
                  self->debug_peak_, rms, 20.0 * SDL_log10(rms + 1e-9));
          self->debug_sum_sq_ = 0.0;
          self->debug_peak_ = 0.0f;
          self->debug_frames_ = 0;
        }
      }
      self->position_ += got;
      frames_wanted -= got;
      continue;
    }
    if (self->switching_to_ >= 0) {  // faixa acabou no meio do fade out: troca já
      self->switch_left_ = 0;
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
