#pragma once

#include <SDL3/SDL.h>

#include <deque>
#include <string>
#include <unordered_map>

#include "scripts/audios/Clip.h"

// Narration, one voice at a time: Say queues a line, Interrupt drops the queue and current line.
class Voices {
 public:
  Voices() = default;
  ~Voices();
  Voices(const Voices&) = delete;
  Voices& operator=(const Voices&) = delete;

  void Say(const std::string& path);
  void Interrupt();
  // Call once per frame to move on to the next line.
  void Update();
  bool Speaking() const { return current_ != nullptr || !queue_.empty(); }

  void SetVolume(float volume);
  float Volume() const { return volume_; }
  void SetMuted(bool muted);
  bool Muted() const { return muted_; }

 private:
  const audios::Clip* ClipFor(const std::string& path);
  void PlayNext();

  std::unordered_map<std::string, audios::Clip> clips_;  // Loaded on first use, keyed by path.
  std::deque<std::string> queue_;
  SDL_AudioStream* current_ = nullptr;
  float volume_ = 1.0f;
  bool muted_ = false;
};
