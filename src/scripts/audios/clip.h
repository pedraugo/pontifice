#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

namespace audios {

struct Clip {
  int channels = 0;
  int sample_rate = 0;
  std::vector<float> samples;  // Interleaved f32 frames.

  bool Valid() const { return channels > 0 && sample_rate > 0 && !samples.empty(); }
  float Seconds() const;
  static Clip Load(const std::string& path);
};

// One-shot stream on the default device; destroy it once Finished.
SDL_AudioStream* Start(const Clip& clip, float gain);
bool Finished(SDL_AudioStream* stream);

}
