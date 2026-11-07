#include "scripts/audios/sfx.h"

#include <algorithm>
#include "scripts/json/json.h"
#include "scripts/log/log.h"

Sfx::~Sfx() { StopAll(); }

bool Sfx::Init(const std::string& base_path) {
  json::Value file;
  if (!json::LoadFile(base_path + "assets/audio.json", file)) return false;
  const json::Value* sfx = file.Find("sfx");
  if (sfx == nullptr || !sfx->IsArray()) {
    LOG("audio.json has no \"sfx\" section; continuing without sound effects");
    return false;
  }
  size_t loaded = 0;
  for (const json::Value& clip : sfx->array) {
    if (!clip.IsObject()) continue;
    const json::Value* name = clip.Find("name");
    const json::Value* path = clip.Find("file");
    if (name == nullptr || !name->IsString() || path == nullptr || !path->IsString()) continue;
    const auto number = [&clip](const char* key) {
      const json::Value* value = clip.Find(key);
      return value != nullptr && value->IsNumber() ? static_cast<float>(value->number) : 1.0f;
    };
    if (Load(name->string, base_path + path->string, number("gain") * number("volume"))) ++loaded;
  }
  LOG("Sfx: %zu/%zu clip(s) loaded, volume %.0f%%", loaded, sfx->array.size(), volume_ * 100.0f);
  return loaded > 0;
}

bool Sfx::Load(const std::string& name, const std::string& path, float gain) {
  audios::Clip clip = audios::Clip::Load(path);
  if (!clip.Valid()) return false;
  clips_[name] = Entry{std::move(clip), SDL_clamp(gain, 0.0f, 1.0f)};
  return true;
}

bool Sfx::Play(const std::string& name, float gain) {
  if (muted_) return false;
  const auto it = clips_.find(name);
  if (it == clips_.end()) {
    LOG("Unknown sound effect \"%s\"", name.c_str());
    return false;
  }
  const float level = it->second.gain * gain;
  SDL_AudioStream* stream = audios::Start(it->second.clip, level * volume_);
  if (stream == nullptr) return false;
  playing_.push_back(Playing{stream, level});
  return true;
}

void Sfx::Update() {
  const auto done = std::remove_if(playing_.begin(), playing_.end(), [](const Playing& p) {
    if (!audios::Finished(p.stream)) return false;
    SDL_DestroyAudioStream(p.stream);
    return true;
  });
  playing_.erase(done, playing_.end());
}

void Sfx::StopAll() {
  for (const Playing& p : playing_) SDL_DestroyAudioStream(p.stream);
  playing_.clear();
}

void Sfx::SetVolume(float volume) {
  volume_ = SDL_clamp(volume, 0.0f, 1.0f);
  for (const Playing& p : playing_) SDL_SetAudioStreamGain(p.stream, p.gain * volume_);
}
