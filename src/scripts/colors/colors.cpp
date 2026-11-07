#include "scripts/colors/colors.h"

#include <unordered_map>
#include <unordered_set>

#include "scripts/json/json.h"
#include "scripts/log/log.h"

namespace colors {

namespace {

constexpr SDL_Color kMissing{255, 0, 255, 255};

std::unordered_map<std::string, SDL_Color>& Table() {
  static std::unordered_map<std::string, SDL_Color> table;
  return table;
}

std::unordered_set<std::string>& Warned() {
  static std::unordered_set<std::string> warned;
  return warned;
}

int HexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}

bool ParseHex(const std::string& hex, SDL_Color& out) {
  if ((hex.size() != 7 && hex.size() != 9) || hex[0] != '#') return false;
  int digits[8] = {0, 0, 0, 0, 0, 0, 15, 15};  // Alpha defaults to ff.
  for (size_t i = 1; i < hex.size(); ++i) {
    digits[i - 1] = HexDigit(hex[i]);
    if (digits[i - 1] < 0) return false;
  }
  out.r = static_cast<Uint8>(digits[0] * 16 + digits[1]);
  out.g = static_cast<Uint8>(digits[2] * 16 + digits[3]);
  out.b = static_cast<Uint8>(digits[4] * 16 + digits[5]);
  out.a = static_cast<Uint8>(digits[6] * 16 + digits[7]);
  return true;
}

bool Load(const std::string& path) {
  json::Value root;
  if (!json::LoadFile(path, root)) return false;
  Table().clear();
  Warned().clear();
  for (const auto& [name, value] : root.object) {
    SDL_Color color;
    if (value.IsString() && ParseHex(value.string, color)) {
      Table()[name] = color;
    } else {
      LOG("%s: invalid color in %s", path.c_str(), name.c_str());
    }
  }
  return !Table().empty();
}

SDL_Color Get(const std::string& name) {
  const auto it = Table().find(name);
  if (it != Table().end()) return it->second;
  if (Warned().insert(name).second) LOG("Color not defined in colors.json: %s", name.c_str());
  return kMissing;
}

SDL_FColor GetF(const std::string& name) {
  const SDL_Color c = Get(name);
  return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

}
