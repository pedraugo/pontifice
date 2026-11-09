#include "scripts/audios/voices.h"

#include "scripts/json/json.h"
#include "scripts/log/log.h"

Voices::~Voices() { Interrupt(); }

bool Voices::Init(const std::string& base_path) {
  base_ = base_path;
  json::Value file;
  if (!json::LoadFile(base_path + "assets/audio.json", file)) return false;
  const json::Value* voices = file.Find("voices");
  if (voices == nullptr || !voices->IsObject()) {
    LOG("audio.json has no \"voices\" section; continuing without voices");
    return false;
  }
  for (const auto& [name, value] : voices->object) {
    if (name == "volume" && value.IsNumber()) {
      volume_ = SDL_clamp(static_cast<float>(value.number), 0.0f, 1.0f);
    } else if (value.IsString()) {
      files_[name] = value.string;
    }
  }
  LOG("Voices: %zu pattern(s), volume %.0f%%", files_.size(), volume_ * 100.0f);
  return true;
}

std::string Voices::File(const std::string& name,
                         std::initializer_list<std::pair<std::string, std::string>> fields) const {
  const auto it = files_.find(name);
  if (it == files_.end()) return "";
  std::string path = base_ + it->second;
  for (const auto& [key, value] : fields) {
    const std::string marker = "{" + key + "}";
    for (size_t pos = path.find(marker); pos != std::string::npos;
         pos = path.find(marker, pos + value.size())) {
      path.replace(pos, marker.size(), value);
    }
  }
  return SDL_GetPathInfo(path.c_str(), nullptr) ? path : "";
}

void Voices::Say(const std::string& path) { queue_.push_back(path); }

void Voices::Interrupt() {
  queue_.clear();
  if (current_ != nullptr) {
    SDL_DestroyAudioStream(current_);
    current_ = nullptr;
  }
}

void Voices::Update() {
  if (current_ != nullptr && audios::Finished(current_)) {
    SDL_DestroyAudioStream(current_);
    current_ = nullptr;
  }
  if (current_ == nullptr && !queue_.empty()) PlayNext();
}

const audios::Clip* Voices::ClipFor(const std::string& path) {
  if (const auto it = clips_.find(path); it != clips_.end()) return &it->second;
  audios::Clip clip = audios::Clip::Load(path);
  if (!clip.Valid()) return nullptr;
  return &(clips_[path] = std::move(clip));
}

void Voices::PlayNext() {
  while (!queue_.empty()) {
    const std::string path = std::move(queue_.front());
    queue_.pop_front();
    const audios::Clip* clip = ClipFor(path);
    if (clip == nullptr) continue;
    current_ = audios::Start(*clip, muted_ ? 0.0f : volume_);
    if (current_ != nullptr) {
      LOG("Voice: playing %s (%.1f s)", path.c_str(), clip->Seconds());
      return;
    }
  }
}

void Voices::SetVolume(float volume) {
  volume_ = SDL_clamp(volume, 0.0f, 1.0f);
  if (current_ != nullptr && !muted_) SDL_SetAudioStreamGain(current_, volume_);
}

void Voices::SetMuted(bool muted) {
  muted_ = muted;
  if (current_ != nullptr) SDL_SetAudioStreamGain(current_, muted_ ? 0.0f : volume_);
}
