#pragma once
#include <string>
#include <variant>
#include <list>
#include <functional>
#include <vector>


enum Type {
  String,
  Number,
  Boolean,
  List,
  Operator,
  Defunc,
  Identifier,
  Command,
};
struct Symbol;

using _Type = std::variant<std::monostate,
			   int, std::string,
			   std::list<Symbol>,
			   bool>;

struct Symbol {
  auto operator<=>(const Symbol&) const = default;
  Symbol() = default;
  Symbol(std::string _n, _Type _v, Type _t) {
    name = _n;
    value = _v;
    type = _t;
  }

  std::string name; // empty string if not present
  _Type value;
  Type type;
};

// function signature for the builtins
using fnsig = std::function<Symbol(std::list<Symbol>)>;
