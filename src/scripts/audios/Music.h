#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <map>
#include <random>
#include <string>
#include <vector>

// Decodes and fades tracks in the audio callback; the main thread only calls the public API.
class Music {
 public:
  Music() = default;
  ~Music();
  Music(const Music&) = delete;
  Music& operator=(const Music&) = delete;

  bool Init(const std::string& base_path);
  void Shutdown();

  void SetVolume(float volume);
  void SetScene(const std::string& key);
  bool Muted() const { return muted_.load(); }
  void ToggleMuted() { muted_.store(!muted_.load()); }
  // Fades out the current track; with resume the new category continues where it last stopped.
  void SetCategory(const std::string& name, bool resume = false);

 private:
  struct Decoder;

  static void SDLCALL Feed(void* userdata, SDL_AudioStream* stream, int additional_bytes,
                           int total_bytes);
  bool OpenTrack(size_t index);
  bool OpenNextTrack();
  void CloseTrack();
  void ApplyFade(float* samples, size_t frames, int channels);

  struct Track {
    std::string file;
    float gain = 1.0f;
  };
  struct Category {
    std::string name;
    std::vector<Track> tracks;
    bool random = false;
  };
  void Analyze();

  std::string base_;
  std::vector<Category> categories_;
  size_t active_ = 0;
  // Main-thread request: index * 2 + (1 if resume), or -1 for none.
  std::atomic<int> pending_category_{-1};
  int switching_to_ = -1;
  bool switch_resume_ = false;
  struct Saved {
    size_t track = 0;
    uint64_t position = 0;
    bool valid = false;
  };
  // Where each category stopped, for SetCategory with resume.
  std::vector<Saved> saved_;
  uint64_t fade_in_left_ = 0;
  uint64_t fade_in_total_ = 0;
  uint64_t switch_left_ = 0;
  uint64_t switch_total_ = 0;
  std::vector<Track> tracks_;
  bool random_ = false;
  std::mt19937 rng_;
  float track_gain_ = 1.0f;
  std::map<std::string, float> scene_volumes_;
  // 0..1; volume_ ramps toward it over kRampSeconds.
  std::atomic<float> target_volume_{0.3f};
  std::atomic<bool> muted_{false};
  float volume_ = 0.3f;
  size_t current_ = 0;
  Decoder* decoder_ = nullptr;
  SDL_AudioStream* stream_ = nullptr;
  std::vector<float> buffer_;
  uint64_t position_ = 0;
  // PONTIFICE_AUDIO_LOG=1: logs peak and RMS of each second sent.
  bool debug_log_ = false;
  double debug_sum_sq_ = 0.0;
  float debug_peak_ = 0.0f;
  uint64_t debug_frames_ = 0;
  uint64_t total_frames_ = 0;
};
