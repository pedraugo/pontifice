#pragma once

#include <SDL3/SDL.h>

#include <string>

#include "scripts/json/json.h"
#include "scripts/math/math.h"

namespace config {

// Settings from assets/*.json, registered under a name ("light", "fog", "shadow") in main.
bool Load(const std::string& name, const std::string& path);

// Dotted path ("battle.arena.ambient") into file `name`; read-only after Load, thread-safe.
const json::Value* Node(const std::string& name, const std::string& path);

// Missing or wrong-typed value returns `fallback` and warns in the log once.
float Float(const std::string& name, const std::string& path, float fallback);
int Int(const std::string& name, const std::string& path, int fallback);
bool Bool(const std::string& name, const std::string& path, bool fallback);
std::string String(const std::string& name, const std::string& path, const std::string& fallback);
math::Vec3 Vec3(const std::string& name, const std::string& path, const math::Vec3& fallback);
// "#rrggbb", "#rrggbbaa", a colors.json name ("stoneDark") or [r, g, b(, a)] in 0..1.
SDL_FColor Color(const std::string& name, const std::string& path, const SDL_FColor& fallback);

// Same readers on an object already in hand (array items); silent on a missing key.
float Float(const json::Value& object, const std::string& key, float fallback);
std::string String(const json::Value& object, const std::string& key, const std::string& fallback);
SDL_FColor Color(const json::Value& object, const std::string& key, const SDL_FColor& fallback);

}
