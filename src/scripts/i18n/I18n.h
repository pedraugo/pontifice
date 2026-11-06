#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace i18n {

constexpr const char* kDefaultLanguage = "pt";

struct Language {
  std::string code;
  std::string name;
  std::string flag;
};

// Reads translate.json: "languages" list and "strings" {key: {code: text}}.
bool Load(const std::string& path);
const std::vector<Language>& Languages();

// Read-only after Load and SetLanguage; lookups are safe from other threads.
void SetLanguage(const std::string& code);
const std::string& CurrentLanguage();

// Falls back to Portuguese, then to the key itself, so text never vanishes.
std::string Get(const std::string& key);
// Like Get, but a key missing in every language returns `fallback`.
std::string GetOr(const std::string& key, const std::string& fallback);
// Get with "{0}", "{1}", ... replaced by args in order.
std::string Format(const std::string& key, std::initializer_list<std::string> args);

}
