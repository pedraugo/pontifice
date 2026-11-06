#include "scripts/audios/music.h"

#include <algorithm>

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#include "scripts/json/json.h"
#include "scripts/log/log.h"

namespace {

// Per-track fade in/out, silence between tracks, volume ramp, category cross-fade.
constexpr float kFadeSeconds = 2.0f;
constexpr float kGapSeconds = 3.0f;
constexpr float kRampSeconds = 1.0f;
constexpr float kSwitchSeconds = 1.5f;

}

struct Music::Decoder {
  stb_vorbis* vorbis = nullptr;
  int channels = 0;
  int sample_rate = 0;
  bool open = false;
  std::vector<uint8_t> file;
};

Music::~Music() { Shutdown(); }

bool Music::Init(const std::string& base_path) {
  base_ = base_path;
  json::Value file;
  if (!json::LoadFile(base_ + "assets/audio.json", file)) return false;
  const json::Value* music = file.Find("music");
  if (music == nullptr || !music->IsObject()) {
    LOG("audio.json has no \"music\" section; continuing without music");
    return false;
  }
  const json::Value& root = *music;
  // "volume": a number, or a per-scene object like {"menu": 0.15, "battle": 0.25}.
  if (const json::Value* volume = root.Find("volume")) {
    if (volume->IsNumber()) {
      volume_ = static_cast<float>(SDL_clamp(volume->number, 0.0, 1.0));
    } else if (volume->IsObject()) {
      for (const auto& [key, value] : volume->object) {
        if (value.IsNumber()) {
          scene_volumes_[key] = static_cast<float>(SDL_clamp(value.number, 0.0, 1.0));
        }
      }
      if (scene_volumes_.count("menu")) volume_ = scene_volumes_["menu"];
    }
  }
  target_volume_.store(volume_);
  debug_log_ = SDL_getenv("PONTIFICE_AUDIO_LOG") != nullptr;
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
    Category category{"menu", read_list(*tracks)};
    if (!category.tracks.empty()) categories_.push_back(std::move(category));
  }
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
    LOG("audio.json has no music tracks; continuing without music");
    return false;
  }
  active_ = 0;
  for (size_t i = 0; i < categories_.size(); ++i) {
    if (categories_[i].name == "menu") active_ = i;
  }
  tracks_ = categories_[active_].tracks;
  random_ = categories_[active_].random;
  saved_.assign(categories_.size(), Saved{});
  if (SDL_getenv("PONTIFICE_AUDIO_ANALYZE") != nullptr) Analyze();

  decoder_ = new Decoder;
  // tracks_.size() means nothing played yet; OpenNextTrack then starts at track 0.
  current_ = tracks_.size();
  if (!OpenNextTrack()) {
    LOG("No music track from audio.json could be opened; continuing without music");
    Shutdown();
    return false;
  }

  const SDL_AudioSpec spec{SDL_AUDIO_F32, static_cast<int>(decoder_->channels),
                           static_cast<int>(decoder_->sample_rate)};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &Music::Feed,
                                      this);
  if (stream_ == nullptr) {
    LOG("SDL_OpenAudioDeviceStream: %s; continuing without music", SDL_GetError());
    Shutdown();
    return false;
  }
  if (!SDL_ResumeAudioStreamDevice(stream_)) {
    LOG("SDL_ResumeAudioStreamDevice: %s", SDL_GetError());
  }
  SDL_AudioSpec device_spec{};
  SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(stream_), &device_spec, nullptr);
  LOG("Music: %zu category(ies), \"%s\" with %zu track(s), volume %.0f%%, device %d "
          "channels @ %d Hz%s",
          categories_.size(), categories_[active_].name.c_str(), tracks_.size(),
          volume_ * 100.0f, device_spec.channels, device_spec.freq,
          SDL_AudioDevicePaused(SDL_GetAudioStreamDevice(stream_)) ? " (pausado)" : "");
  return true;
}

void Music::Shutdown() {
  // Destroy the stream first: Feed no longer runs after that, so the decoder is ours.
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
    stb_vorbis_close(decoder_->vorbis);
    decoder_->vorbis = nullptr;
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
    LOG("Music: could not open %s: %s", path.c_str(), SDL_GetError());
    return false;
  }
  decoder_->file.assign(static_cast<uint8_t*>(data), static_cast<uint8_t*>(data) + size);
  SDL_free(data);
  int error = 0;
  decoder_->vorbis = stb_vorbis_open_memory(decoder_->file.data(),
                                            static_cast<int>(decoder_->file.size()), &error,
                                            nullptr);
  if (decoder_->vorbis == nullptr) {
    LOG("Music: %s is not a valid Ogg Vorbis (error %d)", path.c_str(), error);
    return false;
  }
  const stb_vorbis_info info = stb_vorbis_get_info(decoder_->vorbis);
  decoder_->channels = info.channels;
  decoder_->sample_rate = static_cast<int>(info.sample_rate);
  decoder_->open = true;
  current_ = index;
  position_ = 0;
  fade_in_left_ = fade_in_total_ = 0;
  total_frames_ = stb_vorbis_stream_length_in_samples(decoder_->vorbis);
  stb_vorbis_seek_start(decoder_->vorbis);
  LOG("Music: playing %s (gain %.2f)", tracks_[index].file.c_str(), track_gain_);
  return true;
}

// Debug only (PONTIFICE_AUDIO_ANALYZE=1): RMS per track and the gain suggested for kReferenceRms.
void Music::Analyze() {
  constexpr double kReferenceRms = 0.10;
  for (const Category& category : categories_) {
    for (const Track& track : category.tracks) {
      const std::string path = base_ + track.file;
      size_t size = 0;
      void* data = SDL_LoadFile(path.c_str(), &size);
      if (data == nullptr) continue;
      int error = 0;
      stb_vorbis* vorbis = stb_vorbis_open_memory(static_cast<const unsigned char*>(data),
                                                  static_cast<int>(size), &error, nullptr);
      if (vorbis == nullptr) {
        SDL_free(data);
        continue;
      }
      const int channels = stb_vorbis_get_info(vorbis).channels;
      std::vector<float> chunk(4096 * channels);
      double sum_sq = 0.0;
      uint64_t samples = 0;
      for (;;) {
        const int got = stb_vorbis_get_samples_float_interleaved(vorbis, channels, chunk.data(),
                                                                 4096 * channels);
        if (got <= 0) break;
        for (int i = 0; i < got * channels; ++i) {
          sum_sq += static_cast<double>(chunk[i]) * chunk[i];
        }
        samples += static_cast<uint64_t>(got) * channels;
      }
      stb_vorbis_close(vorbis);
      SDL_free(data);
      const double rms = samples > 0 ? SDL_sqrt(sum_sq / samples) : 0.0;
      LOG("Analysis: %s [%s] RMS %.4f (%.1f dBFS) suggested gain %.2f", track.file.c_str(),
              category.name.c_str(), rms, 20.0 * SDL_log10(rms + 1e-9),
              rms > 0.0 ? kReferenceRms / rms : 1.0);
    }
  }
}

// Random: any track but the last one played; sequential: the next. Unreadable tracks are skipped.
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
  LOG("Music: unknown category \"%s\"", name.c_str());
}

void Music::SetScene(const std::string& key) {
  const auto it = scene_volumes_.find(key);
  if (it == scene_volumes_.end()) return;
  SetVolume(it->second);
  LOG("Music: volume %s %.0f%%", key.c_str(), it->second * 100.0f);
}

// Per-frame gain: track edge fades, fade-in after a switch, volume ramp, then track gain.
void Music::ApplyFade(float* samples, size_t frames, int channels) {
  const float rate = static_cast<float>(decoder_->sample_rate);
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

// Audio thread: shares only pending_category_, target_volume_ and muted_ with the main thread.
void SDLCALL Music::Feed(void* userdata, SDL_AudioStream* stream, int additional_bytes,
                         int) {
  Music* self = static_cast<Music*>(userdata);
  Decoder* dec = self->decoder_;
  if (additional_bytes <= 0 || dec == nullptr || !dec->open) return;

  const int channels = static_cast<int>(dec->channels);
  uint64_t frames_wanted =
      static_cast<uint64_t>(additional_bytes) / (sizeof(float) * channels) + 1;
  self->buffer_.resize(static_cast<size_t>(frames_wanted) * channels);

  const int request = self->pending_category_.exchange(-1);
  if (request >= 0) {
    const int pending = request / 2;
    // Current category asked again mid fade-out: cancel the switch and fade back in.
    if (pending == static_cast<int>(self->active_) && self->switching_to_ >= 0) {
      self->fade_in_total_ = self->switch_total_;
      self->fade_in_left_ = self->switch_total_ - self->switch_left_;
      self->switching_to_ = -1;
    } else if (pending != static_cast<int>(self->active_) && self->switching_to_ < 0) {
      self->switching_to_ = pending;
      self->switch_resume_ = (request % 2) == 1;
      self->switch_total_ = self->switch_left_ =
          static_cast<uint64_t>(kSwitchSeconds * dec->sample_rate);
    }
  }

  while (frames_wanted > 0) {
    // Switch fade-out done: save where the old category stopped, then open the new one.
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
        stb_vorbis_seek(dec->vorbis, static_cast<unsigned>(saved.position));
        self->position_ = saved.position;
        self->fade_in_total_ = self->fade_in_left_ =
            static_cast<uint64_t>(kSwitchSeconds * dec->sample_rate);
        resumed = true;
      }
      LOG("Music: category \"%s\"%s", self->categories_[self->active_].name.c_str(),
              resumed ? " (resumed)" : "");
      if (!resumed) {
        self->current_ = self->tracks_.size();
        if (!self->OpenNextTrack()) return;
      }
      const SDL_AudioSpec spec{SDL_AUDIO_F32, static_cast<int>(dec->channels),
                               static_cast<int>(dec->sample_rate)};
      SDL_SetAudioStreamFormat(stream, &spec, nullptr);
      return;
    }
    uint64_t to_read = frames_wanted;
    if (self->switching_to_ >= 0) to_read = SDL_min(to_read, self->switch_left_);
    const int got_frames = stb_vorbis_get_samples_float_interleaved(
        dec->vorbis, channels, self->buffer_.data(), static_cast<int>(to_read * channels));
    const uint64_t got = got_frames > 0 ? static_cast<uint64_t>(got_frames) : 0;
    if (got > 0) {
      self->ApplyFade(self->buffer_.data(), static_cast<size_t>(got), channels);
      if (self->switching_to_ >= 0) {
        for (uint64_t i = 0; i < got; ++i) {
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
        if (self->debug_frames_ >= static_cast<uint64_t>(dec->sample_rate)) {
          const double rms = SDL_sqrt(self->debug_sum_sq_ / (self->debug_frames_ * channels));
          LOG("Music: applied volume %.0f%% (target %.0f%%), peak %.2f, RMS %.3f (%.1f dBFS)",
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
    if (self->switching_to_ >= 0) {
      self->switch_left_ = 0;
      continue;
    }
    // Track ended: queue silence and open the next; its format may differ, so reset the stream.
    const size_t gap_frames = static_cast<size_t>(kGapSeconds * dec->sample_rate);
    self->buffer_.assign(gap_frames * channels, 0.0f);
    SDL_PutAudioStreamData(stream, self->buffer_.data(),
                           static_cast<int>(gap_frames * channels * sizeof(float)));
    if (!self->OpenNextTrack()) return;
    const SDL_AudioSpec spec{SDL_AUDIO_F32, static_cast<int>(dec->channels),
                             static_cast<int>(dec->sample_rate)};
    SDL_SetAudioStreamFormat(stream, &spec, nullptr);
    return;
  }
}
