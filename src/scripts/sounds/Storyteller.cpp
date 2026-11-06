#include "scripts/sounds/Storyteller.h"

Storyteller::~Storyteller() { Interrupt(); }

void Storyteller::Say(const std::string& path) { queue_.push_back(path); }

void Storyteller::Interrupt() {
  queue_.clear();
  if (current_ != nullptr) {
    SDL_DestroyAudioStream(current_);
    current_ = nullptr;
  }
}

void Storyteller::Update() {
  if (current_ != nullptr && sounds::Finished(current_)) {
    SDL_DestroyAudioStream(current_);
    current_ = nullptr;
  }
  if (current_ == nullptr && !queue_.empty()) PlayNext();
}

const sounds::Clip* Storyteller::ClipFor(const std::string& path) {
  if (const auto it = clips_.find(path); it != clips_.end()) return &it->second;
  sounds::Clip clip = sounds::Clip::Load(path);
  if (!clip.Valid()) return nullptr;
  return &(clips_[path] = std::move(clip));
}

void Storyteller::PlayNext() {
  while (!queue_.empty()) {
    const std::string path = std::move(queue_.front());
    queue_.pop_front();
    const sounds::Clip* clip = ClipFor(path);
    if (clip == nullptr) continue;  // missing line: skip to the next one
    current_ = sounds::Start(*clip, muted_ ? 0.0f : volume_);
    if (current_ != nullptr) return;
  }
}

void Storyteller::SetVolume(float volume) {
  volume_ = SDL_clamp(volume, 0.0f, 1.0f);
  if (current_ != nullptr && !muted_) SDL_SetAudioStreamGain(current_, volume_);
}

void Storyteller::SetMuted(bool muted) {
  muted_ = muted;
  if (current_ != nullptr) SDL_SetAudioStreamGain(current_, muted_ ? 0.0f : volume_);
}
