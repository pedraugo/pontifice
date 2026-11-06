#include "scripts/audios/Clip.h"

// Owns the stb_vorbis implementation; Music.cpp includes it header-only.
#define STB_VORBIS_NO_STDIO
#include <stb_vorbis.c>

namespace audios {

float Clip::Seconds() const {
  if (!Valid()) return 0.0f;
  return static_cast<float>(samples.size() / channels) / static_cast<float>(sample_rate);
}

Clip Clip::Load(const std::string& path) {
  Clip clip;
  size_t size = 0;
  void* data = SDL_LoadFile(path.c_str(), &size);
  if (data == nullptr) {
    SDL_Log("Som: nao abriu %s: %s", path.c_str(), SDL_GetError());
    return clip;
  }
  int error = 0;
  stb_vorbis* vorbis = stb_vorbis_open_memory(static_cast<const unsigned char*>(data),
                                              static_cast<int>(size), &error, nullptr);
  if (vorbis == nullptr) {
    SDL_Log("Som: %s nao e um Ogg Vorbis valido (erro %d)", path.c_str(), error);
    SDL_free(data);
    return clip;
  }
  const stb_vorbis_info info = stb_vorbis_get_info(vorbis);
  clip.channels = info.channels;
  clip.sample_rate = static_cast<int>(info.sample_rate);
  clip.samples.reserve(static_cast<size_t>(stb_vorbis_stream_length_in_samples(vorbis)) *
                       clip.channels);
  std::vector<float> chunk(4096 * static_cast<size_t>(clip.channels));
  for (;;) {
    const int got = stb_vorbis_get_samples_float_interleaved(vorbis, clip.channels, chunk.data(),
                                                             static_cast<int>(chunk.size()));
    if (got <= 0) break;
    clip.samples.insert(clip.samples.end(), chunk.begin(),
                        chunk.begin() + static_cast<long>(got) * clip.channels);
  }
  stb_vorbis_close(vorbis);
  SDL_free(data);
  return clip;
}

SDL_AudioStream* Start(const Clip& clip, float gain) {
  if (!clip.Valid()) return nullptr;
  const SDL_AudioSpec spec{SDL_AUDIO_F32, clip.channels, clip.sample_rate};
  SDL_AudioStream* stream =
      SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
  if (stream == nullptr) {
    SDL_Log("Som: SDL_OpenAudioDeviceStream: %s", SDL_GetError());
    return nullptr;
  }
  SDL_SetAudioStreamGain(stream, gain);
  SDL_PutAudioStreamData(stream, clip.samples.data(),
                         static_cast<int>(clip.samples.size() * sizeof(float)));
  SDL_FlushAudioStream(stream);
  SDL_ResumeAudioStreamDevice(stream);
  return stream;
}

bool Finished(SDL_AudioStream* stream) {
  return stream == nullptr || SDL_GetAudioStreamQueued(stream) <= 0;
}

}
