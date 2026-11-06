#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "scripts/sounds/Clip.h"

// Short one-shot sounds: sword hits, shield blocks, card placement and the like. Each Play
// gets its own stream, so several effects overlap. Call Update once per frame to release
// the streams that finished.
class Effect {
 public:
  Effect() = default;
  ~Effect();
  Effect(const Effect&) = delete;
  Effect& operator=(const Effect&) = delete;

  // Loads and caches sounds/<file> under `name`; false if the file does not decode.
  bool Load(const std::string& name, const std::string& path);
  bool Play(const std::string& name, float gain = 1.0f);
  void Update();
  void StopAll();

  void SetVolume(float volume);
  float Volume() const { return volume_; }
  void SetMuted(bool muted) { muted_ = muted; }
  bool Muted() const { return muted_; }

 private:
  std::unordered_map<std::string, sounds::Clip> clips_;
  std::vector<SDL_AudioStream*> playing_;
  float volume_ = 1.0f;
  bool muted_ = false;
};
