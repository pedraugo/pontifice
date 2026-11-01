#pragma once

#include <string>
#include <utility>
#include <vector>

// Leitor JSON mínimo (RFC 8259, sem extensões): objetos, arrays, strings com escapes
// comuns, números, true/false/null. Suficiente para assets/*.json.
namespace json {

class Value {
 public:
  enum class Type { kNull, kBool, kNumber, kString, kArray, kObject };

  Type type = Type::kNull;
  bool boolean = false;
  double number = 0.0;
  std::string string;
  std::vector<Value> array;
  std::vector<std::pair<std::string, Value>> object;  // ordem do arquivo

  bool IsObject() const { return type == Type::kObject; }
  bool IsArray() const { return type == Type::kArray; }
  bool IsString() const { return type == Type::kString; }
  bool IsNumber() const { return type == Type::kNumber; }

  // Membro de objeto; nullptr se não é objeto ou a chave não existe.
  const Value* Find(const std::string& key) const;
  // Caminho com pontos: "roles.panel.fill". nullptr se qualquer nível falta.
  const Value* Path(const std::string& dotted) const;
};

// Retorna false e descreve em `error` (com linha) se o texto não é JSON válido.
bool Parse(const std::string& text, Value& out, std::string* error);
// Lê e interpreta um arquivo. Em erro, registra no log do SDL e retorna false.
bool LoadFile(const std::string& path, Value& out);

}  // namespace json
