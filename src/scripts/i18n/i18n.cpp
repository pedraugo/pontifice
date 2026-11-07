#include "scripts/i18n/i18n.h"

#include <SDL3/SDL.h>

#include <unordered_map>

#include "scripts/json/json.h"
#include "scripts/log/log.h"

namespace i18n {

namespace {

// key -> (language code -> text).
std::unordered_map<std::string, std::unordered_map<std::string, std::string>> g_strings;
std::vector<Language> g_languages;
std::string g_language = kDefaultLanguage;

bool IsLeaf(const json::Value& node) {
  for (const auto& [code, text] : node.object) {
    if (!text.IsString()) return false;
  }
  return true;
}

// Nested groups become dotted keys: menu.play -> "menu.play"; "languages" is an array, skipped.
void Flatten(const json::Value& node, const std::string& prefix) {
  for (const auto& [name, child] : node.object) {
    if (!child.IsObject()) continue;
    const std::string key = prefix.empty() ? name : prefix + "." + name;
    if (!IsLeaf(child)) {
      Flatten(child, key);
      continue;
    }
    auto& by_language = g_strings[key];
    for (const auto& [code, text] : child.object) by_language[code] = text.string;
  }
}

const std::string* Lookup(const std::string& key, const std::string& code) {
  const auto entry = g_strings.find(key);
  if (entry == g_strings.end()) return nullptr;
  const auto text = entry->second.find(code);
  return text == entry->second.end() ? nullptr : &text->second;
}

}

bool Load(const std::string& path) {
  json::Value root;
  if (!json::LoadFile(path, root) || !root.IsObject()) return false;
  g_strings.clear();
  g_languages.clear();

  if (const json::Value* languages = root.Find("languages"); languages && languages->IsArray()) {
    for (const json::Value& item : languages->array) {
      if (!item.IsObject()) continue;
      Language language;
      if (const json::Value* v = item.Find("code"); v && v->IsString()) language.code = v->string;
      if (const json::Value* v = item.Find("name"); v && v->IsString()) language.name = v->string;
      if (const json::Value* v = item.Find("flag"); v && v->IsString()) language.flag = v->string;
      if (!language.code.empty()) g_languages.push_back(language);
    }
  }
  Flatten(root, "");
  LOG("Translations loaded: %zu keys, %zu languages", g_strings.size(), g_languages.size());
  return !g_strings.empty();
}

const std::vector<Language>& Languages() { return g_languages; }

void SetLanguage(const std::string& code) {
  g_language = code.empty() ? kDefaultLanguage : code;
}

const std::string& CurrentLanguage() { return g_language; }

std::string GetOr(const std::string& key, const std::string& fallback) {
  if (const std::string* text = Lookup(key, g_language)) return *text;
  if (const std::string* text = Lookup(key, kDefaultLanguage)) return *text;
  return fallback;
}

std::string Get(const std::string& key) { return GetOr(key, key); }

std::string Format(const std::string& key, std::initializer_list<std::string> args) {
  std::string text = Get(key);
  int index = 0;
  for (const std::string& arg : args) {
    const std::string marker = "{" + std::to_string(index++) + "}";
    for (size_t pos = text.find(marker); pos != std::string::npos;
         pos = text.find(marker, pos + arg.size())) {
      text.replace(pos, marker.size(), arg);
    }
  }
  return text;
}

}
