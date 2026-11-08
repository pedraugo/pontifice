#include "scripts/save/save.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <utility>

#include "scripts/json/json.h"
#include "scripts/log/log.h"
#include "scripts/rules/rules.h"

namespace save {

namespace {

constexpr int kVersion = 1;

std::string Quoted(const std::string& text) {
  std::string out = "\"";
  for (const char c : text) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char escaped[8];
          std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned char>(c));
          out += escaped;
        } else {
          out += c;
        }
    }
  }
  return out + "\"";
}

// Current local time as "YYYY-MM-DD HH:MM:SS".
std::string Now() {
  SDL_Time now = 0;
  SDL_DateTime dt{};
  if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &dt, true)) return "";
  char text[32];
  std::snprintf(text, sizeof(text), "%04d-%02d-%02d %02d:%02d:%02d", dt.year, dt.month, dt.day,
                dt.hour, dt.minute, dt.second);
  return text;
}

std::string Str(const json::Value* value) {
  return value != nullptr && value->IsString() ? value->string : "";
}

}

std::map<std::string, int> Data::Available() const {
  std::map<std::string, int> counts;
  for (const Unlock& unlock : unlocked) ++counts[unlock.code];
  return counts;
}

int Data::Available(const std::string& code) const {
  return static_cast<int>(std::count_if(unlocked.begin(), unlocked.end(),
                                        [&code](const Unlock& u) { return u.code == code; }));
}

int Data::InDeck(const std::string& code) const {
  return static_cast<int>(std::count(deck.begin(), deck.end(), code));
}

void Data::UnlockCard(const std::string& code) { unlocked.push_back({code, Now()}); }

void Data::SetInDeck(const std::string& code, int copies) {
  const int current = InDeck(code);
  const int room = rules::Get().deck_size - (static_cast<int>(deck.size()) - current);
  const int target = std::clamp(copies, 0, std::min(Available(code), room));
  for (int i = current; i < target; ++i) deck.push_back(code);
  for (int i = current; i > target; --i) {
    deck.erase(std::find(deck.rbegin(), deck.rend(), code).base() - 1);
  }
}

std::string Path() {
  if (const char* env = SDL_getenv("PONTIFICE_SAVE")) return env;
  const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
  return std::string(home != nullptr ? home : "./") + "pontifice.json";
}

std::string ToJson(const Data& data) {
  std::string out = "{\n";
  out += "  \"version\": " + std::to_string(kVersion) + ",\n";
  out += "  \"name\": " + Quoted(data.name) + ",\n";
  out += "  \"language\": " + Quoted(data.language) + ",\n";
  out += "  \"difficulty\": " + std::to_string(data.difficulty) + ",\n";
  out += "  \"tutorial\": " + std::string(data.tutorial ? "true" : "false") + ",\n";
  out += "  \"unlocked\": [";
  for (size_t i = 0; i < data.unlocked.size(); ++i) {
    const Unlock& unlock = data.unlocked[i];
    out += std::string(i == 0 ? "\n" : ",\n") + "    {\"code\": " + Quoted(unlock.code) +
           ", \"date\": " + Quoted(unlock.date) + "}";
  }
  out += data.unlocked.empty() ? "],\n" : "\n  ],\n";
  out += "  \"deck\": [";
  for (size_t i = 0; i < data.deck.size(); ++i) {
    out += std::string(i == 0 ? "\n" : ",\n") + "    " + Quoted(data.deck[i]);
  }
  out += data.deck.empty() ? "]\n" : "\n  ]\n";
  return out + "}\n";
}

bool FromJson(const std::string& text, Data& out) {
  json::Value root;
  std::string error;
  if (!json::Parse(text, root, &error)) {
    LOG("Save not parsed: %s", error.c_str());
    return false;
  }
  const json::Value* name = root.Find("name");
  const json::Value* unlocked = root.Find("unlocked");
  if (name == nullptr || !name->IsString() || name->string.empty() || unlocked == nullptr ||
      !unlocked->IsArray()) {
    LOG("Save without a name or unlocked cards");
    return false;
  }
  Data data;
  data.name = name->string;
  data.language = Str(root.Find("language"));
  const json::Value* difficulty = root.Find("difficulty");
  data.difficulty = std::clamp(difficulty != nullptr && difficulty->IsNumber()
                                   ? static_cast<int>(difficulty->number)
                                   : rules::Get().default_difficulty,
                               0, 3);
  const json::Value* tutorial = root.Find("tutorial");
  data.tutorial = tutorial == nullptr || !tutorial->IsBool() || tutorial->boolean;
  for (const json::Value& item : unlocked->array) {
    const std::string code = Str(item.Find("code"));
    if (!code.empty()) data.unlocked.push_back({code, Str(item.Find("date"))});
  }
  if (const json::Value* deck = root.Find("deck"); deck != nullptr && deck->IsArray()) {
    for (const json::Value& code : deck->array) {
      if (code.IsString()) data.SetInDeck(code.string, data.InDeck(code.string) + 1);
    }
  }
  out = std::move(data);
  return true;
}

std::optional<Data> Load() {
  const std::string path = Path();
  if (!SDL_GetPathInfo(path.c_str(), nullptr)) return std::nullopt;
  size_t size = 0;
  void* bytes = SDL_LoadFile(path.c_str(), &size);
  if (bytes == nullptr) {
    LOG("Save not read (%s): %s", path.c_str(), SDL_GetError());
    return std::nullopt;
  }
  const std::string text(static_cast<const char*>(bytes), size);
  SDL_free(bytes);
  Data data;
  if (!FromJson(text, data)) {
    LOG("Save ignored (%s); a new one replaces it", path.c_str());
    return std::nullopt;
  }
  LOG("Save loaded: %s, %zu unlocked, deck of %zu", path.c_str(), data.unlocked.size(),
      data.deck.size());
  return data;
}

bool Write(const Data& data) {
  const std::string path = Path();
  const std::string temporary = path + ".tmp";
  const std::string text = ToJson(data);
  if (!SDL_SaveFile(temporary.c_str(), text.data(), text.size()) ||
      !SDL_RenamePath(temporary.c_str(), path.c_str())) {
    LOG("Save not written (%s): %s", path.c_str(), SDL_GetError());
    SDL_RemovePath(temporary.c_str());
    return false;
  }
  LOG("Save written: %s", path.c_str());
  return true;
}

}
