#include "scripts/config/Config.h"

#include <unordered_map>
#include <unordered_set>

#include "scripts/colors/Colors.h"

namespace config {

namespace {

std::unordered_map<std::string, json::Value> g_files;

void WarnOnce(const std::string& what) {
  static std::unordered_set<std::string> warned;
  if (warned.insert(what).second) SDL_Log("Ajuste ausente ou invalido: %s (usando o padrao)", what.c_str());
}

bool ToColor(const json::Value& value, SDL_FColor& out) {
  if (value.IsString()) {
    SDL_Color c;
    if (!value.string.empty() && value.string[0] == '#') {
      if (!colors::ParseHex(value.string, c)) return false;
    } else {
      c = colors::Get(value.string);
    }
    out = {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
    return true;
  }
  if (value.IsArray() && (value.array.size() == 3 || value.array.size() == 4)) {
    float v[4] = {0, 0, 0, 1};
    for (size_t i = 0; i < value.array.size(); ++i) {
      if (!value.array[i].IsNumber()) return false;
      v[i] = static_cast<float>(value.array[i].number);
    }
    out = {v[0], v[1], v[2], v[3]};
    return true;
  }
  return false;
}

bool ToVec3(const json::Value& value, math::Vec3& out) {
  if (!value.IsArray() || value.array.size() != 3) return false;
  float v[3];
  for (size_t i = 0; i < 3; ++i) {
    if (!value.array[i].IsNumber()) return false;
    v[i] = static_cast<float>(value.array[i].number);
  }
  out = {v[0], v[1], v[2]};
  return true;
}

}

bool Load(const std::string& name, const std::string& path) {
  json::Value root;
  if (!json::LoadFile(path, root) || !root.IsObject()) return false;
  g_files[name] = std::move(root);
  return true;
}

const json::Value* Node(const std::string& name, const std::string& path) {
  const auto file = g_files.find(name);
  if (file == g_files.end()) return nullptr;
  return path.empty() ? &file->second : file->second.Path(path);
}

float Float(const std::string& name, const std::string& path, float fallback) {
  const json::Value* v = Node(name, path);
  if (v == nullptr || !v->IsNumber()) {
    WarnOnce(name + ":" + path);
    return fallback;
  }
  return static_cast<float>(v->number);
}

int Int(const std::string& name, const std::string& path, int fallback) {
  const json::Value* v = Node(name, path);
  if (v == nullptr || !v->IsNumber()) {
    WarnOnce(name + ":" + path);
    return fallback;
  }
  return static_cast<int>(v->number);
}

math::Vec3 Vec3(const std::string& name, const std::string& path, const math::Vec3& fallback) {
  const json::Value* v = Node(name, path);
  math::Vec3 out;
  if (v == nullptr || !ToVec3(*v, out)) {
    WarnOnce(name + ":" + path);
    return fallback;
  }
  return out;
}

SDL_FColor Color(const std::string& name, const std::string& path, const SDL_FColor& fallback) {
  const json::Value* v = Node(name, path);
  SDL_FColor out;
  if (v == nullptr || !ToColor(*v, out)) {
    WarnOnce(name + ":" + path);
    return fallback;
  }
  return out;
}

float Float(const json::Value& object, const std::string& key, float fallback) {
  const json::Value* v = object.Find(key);
  return v != nullptr && v->IsNumber() ? static_cast<float>(v->number) : fallback;
}

std::string String(const json::Value& object, const std::string& key,
                   const std::string& fallback) {
  const json::Value* v = object.Find(key);
  return v != nullptr && v->IsString() ? v->string : fallback;
}

SDL_FColor Color(const json::Value& object, const std::string& key, const SDL_FColor& fallback) {
  const json::Value* v = object.Find(key);
  SDL_FColor out;
  return v != nullptr && ToColor(*v, out) ? out : fallback;
}

}
