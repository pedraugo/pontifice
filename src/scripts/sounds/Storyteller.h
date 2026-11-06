#pragma once

#include <SDL3/SDL.h>

#include <deque>
#include <string>
#include <unordered_map>

#include "scripts/sounds/Clip.h"

// Narration and character lines: one voice at a time. Say queues a line after the ones
// already waiting; Interrupt drops the queue and cuts the current line. Call Update once
// per frame to move to the next line.
class Storyteller {
 public:
  Storyteller() = default;
  ~Storyteller();
  Storyteller(const Storyteller&) = delete;
  Storyteller& operator=(const Storyteller&) = delete;

  // Lines are loaded on first use and cached by path.
  void Say(const std::string& path);
  void Interrupt();
  void Update();
  bool Speaking() const { return current_ != nullptr || !queue_.empty(); }

  void SetVolume(float volume);
  float Volume() const { return volume_; }
  void SetMuted(bool muted);
  bool Muted() const { return muted_; }

 private:
  const sounds::Clip* ClipFor(const std::string& path);
  void PlayNext();

  std::unordered_map<std::string, sounds::Clip> clips_;
  std::deque<std::string> queue_;
  SDL_AudioStream* current_ = nullptr;
  float volume_ = 1.0f;
  bool muted_ = false;
};
