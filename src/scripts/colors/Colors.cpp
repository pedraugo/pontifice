#include "scripts/colors/Colors.h"

#include <unordered_map>
#include <unordered_set>

#include "scripts/json/Json.h"

namespace colors {

namespace {

constexpr SDL_Color kMissing{255, 0, 255, 255};  // magenta: papel sem cor definida

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

// Guarda cada string "#..." de um objeto como "prefixo.chave". Objetos aninhados recursam.
void Collect(const json::Value& node, const std::string& prefix, const std::string& file) {
  for (const auto& [key, value] : node.object) {
    const std::string name = prefix.empty() ? key : prefix + "." + key;
    if (value.IsObject()) {
      Collect(value, name, file);
    } else if (value.IsString()) {
      SDL_Color color;
      if (ParseHex(value.string, color)) {
        Table()[name] = color;
      } else {
        SDL_Log("%s: cor invalida em %s: \"%s\"", file.c_str(), name.c_str(),
                value.string.c_str());
      }
    }
  }
}

}  // namespace

bool ParseHex(const std::string& hex, SDL_Color& out) {
  if ((hex.size() != 7 && hex.size() != 9) || hex[0] != '#') return false;
  int digits[8] = {0, 0, 0, 0, 0, 0, 15, 15};  // alpha padrão ff
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
  if (const json::Value* palette = root.Find("palette")) Collect(*palette, "palette", path);
  if (const json::Value* roles = root.Find("roles")) Collect(*roles, "", path);
  return !Table().empty();
}

SDL_Color Get(const std::string& role) {
  const auto it = Table().find(role);
  if (it != Table().end()) return it->second;
  if (Warned().insert(role).second) SDL_Log("Cor sem definicao em colors.json: %s", role.c_str());
  return kMissing;
}

SDL_FColor GetF(const std::string& role) {
  const SDL_Color c = Get(role);
  return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
}

}  // namespace colors
