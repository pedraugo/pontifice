#include "scripts/json/Json.h"

#include <SDL3/SDL.h>

#include <cstdlib>

namespace json {

namespace {

class Parser {
 public:
  Parser(const std::string& text, std::string* error) : text_(text), error_(error) {}

  bool ParseDocument(Value& out) {
    SkipSpace();
    if (!ParseValue(out)) return false;
    SkipSpace();
    if (pos_ != text_.size()) return Fail("conteudo apos o valor principal");
    return true;
  }

 private:
  bool Fail(const char* what) {
    if (error_ != nullptr) {
      int line = 1;
      for (size_t i = 0; i < pos_ && i < text_.size(); ++i) {
        if (text_[i] == '\n') ++line;
      }
      *error_ = std::string(what) + " (linha " + std::to_string(line) + ")";
    }
    return false;
  }

  bool End() const { return pos_ >= text_.size(); }
  char Peek() const { return End() ? '\0' : text_[pos_]; }

  void SkipSpace() {
    while (!End() && (Peek() == ' ' || Peek() == '\t' || Peek() == '\n' || Peek() == '\r')) {
      ++pos_;
    }
  }

  bool Expect(char c, const char* what) {
    if (Peek() != c) return Fail(what);
    ++pos_;
    return true;
  }

  bool Literal(const char* word, Value& out, Value::Type type, bool boolean) {
    const size_t len = std::char_traits<char>::length(word);
    if (text_.compare(pos_, len, word) != 0) return Fail("literal invalido");
    pos_ += len;
    out.type = type;
    out.boolean = boolean;
    return true;
  }

  bool ParseValue(Value& out) {
    switch (Peek()) {
      case '{':
        return ParseObject(out);
      case '[':
        return ParseArray(out);
      case '"':
        out.type = Value::Type::kString;
        return ParseString(out.string);
      case 't':
        return Literal("true", out, Value::Type::kBool, true);
      case 'f':
        return Literal("false", out, Value::Type::kBool, false);
      case 'n':
        return Literal("null", out, Value::Type::kNull, false);
      default:
        return ParseNumber(out);
    }
  }

  bool ParseObject(Value& out) {
    out.type = Value::Type::kObject;
    ++pos_;
    SkipSpace();
    if (Peek() == '}') {
      ++pos_;
      return true;
    }
    while (true) {
      SkipSpace();
      std::string key;
      if (Peek() != '"') return Fail("chave de objeto deve ser string");
      if (!ParseString(key)) return false;
      SkipSpace();
      if (!Expect(':', "esperado ':'")) return false;
      SkipSpace();
      Value value;
      if (!ParseValue(value)) return false;
      out.object.emplace_back(std::move(key), std::move(value));
      SkipSpace();
      if (Peek() == ',') {
        ++pos_;
        continue;
      }
      return Expect('}', "esperado ',' ou '}'");
    }
  }

  bool ParseArray(Value& out) {
    out.type = Value::Type::kArray;
    ++pos_;
    SkipSpace();
    if (Peek() == ']') {
      ++pos_;
      return true;
    }
    while (true) {
      SkipSpace();
      Value value;
      if (!ParseValue(value)) return false;
      out.array.push_back(std::move(value));
      SkipSpace();
      if (Peek() == ',') {
        ++pos_;
        continue;
      }
      return Expect(']', "esperado ',' ou ']'");
    }
  }

  static void AppendUtf8(std::string& out, unsigned code) {
    if (code < 0x80) {
      out.push_back(static_cast<char>(code));
    } else if (code < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (code >> 6)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else if (code < 0x10000) {
      out.push_back(static_cast<char>(0xE0 | (code >> 12)));
      out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xF0 | (code >> 18)));
      out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
    }
  }

  bool ParseHex4(unsigned& code) {
    if (pos_ + 4 > text_.size()) return Fail("escape \\u incompleto");
    code = 0;
    for (int i = 0; i < 4; ++i) {
      const char c = text_[pos_++];
      code <<= 4;
      if (c >= '0' && c <= '9') {
        code |= static_cast<unsigned>(c - '0');
      } else if (c >= 'a' && c <= 'f') {
        code |= static_cast<unsigned>(c - 'a' + 10);
      } else if (c >= 'A' && c <= 'F') {
        code |= static_cast<unsigned>(c - 'A' + 10);
      } else {
        return Fail("escape \\u invalido");
      }
    }
    return true;
  }

  bool ParseString(std::string& out) {
    ++pos_;
    out.clear();
    while (true) {
      if (End()) return Fail("string sem fechamento");
      const char c = text_[pos_++];
      if (c == '"') return true;
      if (c != '\\') {
        out.push_back(c);
        continue;
      }
      if (End()) return Fail("escape incompleto");
      const char e = text_[pos_++];
      switch (e) {
        case '"': out.push_back('"'); break;
        case '\\': out.push_back('\\'); break;
        case '/': out.push_back('/'); break;
        case 'b': out.push_back('\b'); break;
        case 'f': out.push_back('\f'); break;
        case 'n': out.push_back('\n'); break;
        case 'r': out.push_back('\r'); break;
        case 't': out.push_back('\t'); break;
        case 'u': {
          unsigned code = 0;
          if (!ParseHex4(code)) return false;
          // UTF-16 surrogate pair: code points outside the BMP.
          if (code >= 0xD800 && code <= 0xDBFF && text_.compare(pos_, 2, "\\u") == 0) {
            pos_ += 2;
            unsigned low = 0;
            if (!ParseHex4(low)) return false;
            code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
          }
          AppendUtf8(out, code);
          break;
        }
        default:
          return Fail("escape desconhecido");
      }
    }
  }

  bool ParseNumber(Value& out) {
    const char* start = text_.c_str() + pos_;
    char* end = nullptr;
    out.number = std::strtod(start, &end);
    if (end == start) return Fail("valor invalido");
    out.type = Value::Type::kNumber;
    pos_ += static_cast<size_t>(end - start);
    return true;
  }

  const std::string& text_;
  std::string* error_;
  size_t pos_ = 0;
};

}

const Value* Value::Find(const std::string& key) const {
  if (type != Type::kObject) return nullptr;
  for (const auto& [k, v] : object) {
    if (k == key) return &v;
  }
  return nullptr;
}

const Value* Value::Path(const std::string& dotted) const {
  const Value* current = this;
  size_t start = 0;
  while (current != nullptr && start <= dotted.size()) {
    const size_t dot = dotted.find('.', start);
    const std::string key = dotted.substr(start, dot == std::string::npos ? dot : dot - start);
    current = current->Find(key);
    if (dot == std::string::npos) break;
    start = dot + 1;
  }
  return current;
}

bool Parse(const std::string& text, Value& out, std::string* error) {
  out = Value{};
  Parser parser(text, error);
  return parser.ParseDocument(out);
}

bool LoadFile(const std::string& path, Value& out) {
  size_t size = 0;
  void* data = SDL_LoadFile(path.c_str(), &size);
  if (data == nullptr) {
    SDL_Log("JSON nao lido: %s (%s)", path.c_str(), SDL_GetError());
    return false;
  }
  const std::string text(static_cast<const char*>(data), size);
  SDL_free(data);
  std::string error;
  if (!Parse(text, out, &error)) {
    SDL_Log("JSON invalido em %s: %s", path.c_str(), error.c_str());
    return false;
  }
  return true;
}

}
