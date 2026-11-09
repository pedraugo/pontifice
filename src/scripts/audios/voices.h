#pragma once

#include <SDL3/SDL.h>

#include <deque>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <utility>

#include "scripts/audios/clip.h"

// Narration, one voice at a time: Say queues a line, Interrupt drops the queue and current line.
class Voices {
 public:
  Voices() = default;
  ~Voices();
  Voices(const Voices&) = delete;
  Voices& operator=(const Voices&) = delete;

  // Reads the "voices" object of assets/audio.json: "volume" and file patterns by name.
  bool Init(const std::string& base_path);
  // Pattern `name` with each "{key}" replaced; empty when the pattern or the file is missing.
  std::string File(const std::string& name,
                   std::initializer_list<std::pair<std::string, std::string>> fields) const;
  // Decodes ahead of Say, e.g. in a scene's Preload thread.
  void Preload(const std::string& path) { ClipFor(path); }

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

  std::string base_;
  std::unordered_map<std::string, std::string> files_;  // Pattern by name, from audio.json.
  std::unordered_map<std::string, audios::Clip> clips_;  // Loaded on first use, keyed by path.
  std::deque<std::string> queue_;
  SDL_AudioStream* current_ = nullptr;
  float volume_ = 1.0f;
  bool muted_ = false;
};
