#include "scripts/sounds/Effect.h"

#include <algorithm>

Effect::~Effect() { StopAll(); }

bool Effect::Load(const std::string& name, const std::string& path) {
  sounds::Clip clip = sounds::Clip::Load(path);
  if (!clip.Valid()) return false;
  clips_[name] = std::move(clip);
  return true;
}

bool Effect::Play(const std::string& name, float gain) {
  if (muted_) return false;
  const auto it = clips_.find(name);
  if (it == clips_.end()) {
    SDL_Log("Efeito desconhecido \"%s\"", name.c_str());
    return false;
  }
  SDL_AudioStream* stream = sounds::Start(it->second, gain * volume_);
  if (stream == nullptr) return false;
  playing_.push_back(stream);
  return true;
}

void Effect::Update() {
  const auto done = std::remove_if(playing_.begin(), playing_.end(), [](SDL_AudioStream* s) {
    if (!sounds::Finished(s)) return false;
    SDL_DestroyAudioStream(s);
    return true;
  });
  playing_.erase(done, playing_.end());
}

void Effect::StopAll() {
  for (SDL_AudioStream* stream : playing_) SDL_DestroyAudioStream(stream);
  playing_.clear();
}

void Effect::SetVolume(float volume) {
  volume_ = SDL_clamp(volume, 0.0f, 1.0f);
  for (SDL_AudioStream* stream : playing_) SDL_SetAudioStreamGain(stream, volume_);
}
