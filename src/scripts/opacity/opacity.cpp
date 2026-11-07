#include "scripts/opacity/opacity.h"

#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "scripts/json/json.h"
#include "scripts/log/log.h"

namespace opacity {

namespace {

std::unordered_map<std::string, float>& Table() {
  static std::unordered_map<std::string, float> table;
  return table;
}

std::unordered_set<std::string>& Warned() {
  static std::unordered_set<std::string> warned;
  return warned;
}

}

bool Load(const std::string& path) {
  json::Value root;
  if (!json::LoadFile(path, root)) return false;
  Table().clear();
  Warned().clear();
  for (const auto& [name, value] : root.object) {
    if (value.IsNumber() && value.number >= 0.0 && value.number <= 1.0) {
      Table()[name] = static_cast<float>(value.number);
    } else {
      LOG("%s: invalid opacity in %s", path.c_str(), name.c_str());
    }
  }
  return !Table().empty();
}

float Get(const std::string& name) {
  const auto it = Table().find(name);
  if (it != Table().end()) return it->second;
  if (Warned().insert(name).second) LOG("Opacity not defined in opacity.json: %s", name.c_str());
  return 1.0f;
}

SDL_Color Apply(SDL_Color color, const std::string& name) {
  color.a = static_cast<Uint8>(std::lround(color.a * Get(name)));
  return color;
}

SDL_FColor Apply(SDL_FColor color, const std::string& name) {
  color.a *= Get(name);
  return color;
}

}
