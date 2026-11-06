#include "scripts/audios/Voices.h"

Voices::~Voices() { Interrupt(); }

void Voices::Say(const std::string& path) { queue_.push_back(path); }

void Voices::Interrupt() {
  queue_.clear();
  if (current_ != nullptr) {
    SDL_DestroyAudioStream(current_);
    current_ = nullptr;
  }
}

void Voices::Update() {
  if (current_ != nullptr && audios::Finished(current_)) {
    SDL_DestroyAudioStream(current_);
    current_ = nullptr;
  }
  if (current_ == nullptr && !queue_.empty()) PlayNext();
}

const audios::Clip* Voices::ClipFor(const std::string& path) {
  if (const auto it = clips_.find(path); it != clips_.end()) return &it->second;
  audios::Clip clip = audios::Clip::Load(path);
  if (!clip.Valid()) return nullptr;
  return &(clips_[path] = std::move(clip));
}

void Voices::PlayNext() {
  while (!queue_.empty()) {
    const std::string path = std::move(queue_.front());
    queue_.pop_front();
    const audios::Clip* clip = ClipFor(path);
    if (clip == nullptr) continue;
    current_ = audios::Start(*clip, muted_ ? 0.0f : volume_);
    if (current_ != nullptr) return;
  }
}

void Voices::SetVolume(float volume) {
  volume_ = SDL_clamp(volume, 0.0f, 1.0f);
  if (current_ != nullptr && !muted_) SDL_SetAudioStreamGain(current_, volume_);
}

void Voices::SetMuted(bool muted) {
  muted_ = muted;
  if (current_ != nullptr) SDL_SetAudioStreamGain(current_, muted_ ? 0.0f : volume_);
}
