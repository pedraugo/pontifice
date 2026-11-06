#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

namespace sounds {

struct Clip {
  int channels = 0;
  int sample_rate = 0;
  std::vector<float> samples;  // interleaved f32 frames

  bool Valid() const { return channels > 0 && sample_rate > 0 && !samples.empty(); }
  float Seconds() const;
  static Clip Load(const std::string& path);
};

// One-shot playback on the default device; the stream mixes with the others.
SDL_AudioStream* Start(const Clip& clip, float gain);
bool Finished(SDL_AudioStream* stream);

}  // namespace sounds
