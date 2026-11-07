#pragma once

#include <string>
#include <utility>
#include <vector>

namespace json {

// Minimal RFC 8259 reader: objects, arrays, escaped strings, numbers, true/false/null.
class Value {
 public:
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

  Type type = Type::kNull;
  bool boolean = false;
  double number = 0.0;
  std::string string;
  std::vector<Value> array;
  std::vector<std::pair<std::string, Value>> object;  // File order; Find returns the first match.

  bool IsObject() const { return type == Type::kObject; }
  bool IsArray() const { return type == Type::kArray; }
  bool IsString() const { return type == Type::kString; }
  bool IsNumber() const { return type == Type::kNumber; }

  const Value* Find(const std::string& key) const;
  // Dotted path like "battle.fog.color"; nullptr if any level is missing.
  const Value* Path(const std::string& dotted) const;
};

// On failure `error`, if given, receives a message with the line number.
bool Parse(const std::string& text, Value& out, std::string* error);
// Logs read or parse errors with LOG.
bool LoadFile(const std::string& path, Value& out);

}
