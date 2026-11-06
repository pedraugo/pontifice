#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "scripts/audios/clip.h"

// Short one-shot sounds; each Play gets its own stream, so effects overlap.
class Sfx {
 public:
  Sfx() = default;
  ~Sfx();
  Sfx(const Sfx&) = delete;
  Sfx& operator=(const Sfx&) = delete;

  // Loads every clip of the "sfx" section of assets/audio.json ("volume" and "clips").
  bool Init(const std::string& base_path);
  // `gain` is the clip's own level, applied on every Play.
  bool Load(const std::string& name, const std::string& path, float gain = 1.0f);
  bool Play(const std::string& name, float gain = 1.0f);
  // Call once per frame to release finished streams.
  void Update();
  void StopAll();

  void SetVolume(float volume);
  float Volume() const { return volume_; }
  void SetMuted(bool muted) { muted_ = muted; }
  bool Muted() const { return muted_; }

 private:
  struct Entry {
    audios::Clip clip;
    float gain = 1.0f;
  };
  struct Playing {
    SDL_AudioStream* stream = nullptr;
    float gain = 1.0f;
  };
  std::unordered_map<std::string, Entry> clips_;
  std::vector<Playing> playing_;
  float volume_ = 1.0f;
  bool muted_ = false;
};
