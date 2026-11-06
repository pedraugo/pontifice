#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "scripts/audios/Clip.h"

// Short one-shot sounds; each Play gets its own stream, so effects overlap.
class Sfx {
 public:
  Sfx() = default;
  ~Sfx();
  Sfx(const Sfx&) = delete;
  Sfx& operator=(const Sfx&) = delete;

  bool Load(const std::string& name, const std::string& path);
  bool Play(const std::string& name, float gain = 1.0f);
  // Call once per frame to release finished streams.
  void Update();
  void StopAll();

  void SetVolume(float volume);
  float Volume() const { return volume_; }
  void SetMuted(bool muted) { muted_ = muted; }
  bool Muted() const { return muted_; }

 private:
  std::unordered_map<std::string, audios::Clip> clips_;
  std::vector<SDL_AudioStream*> playing_;
  float volume_ = 1.0f;
  bool muted_ = false;
};
