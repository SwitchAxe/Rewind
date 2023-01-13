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
  Symbol() = default;
  auto operator<=>(const Symbol&) const = default;
  Symbol(std::string _n, _Type _v, Type _t) {
    name = _n;
    value = _v;
    type = _t;
  }
  Symbol(std::string _n, _Type _v, Type _t, int _d) {
    name = _n;
    value = _v;
    type = _t;
    depth = _d;
  }
  int depth = 0;
  std::string name; // empty string if not present
  _Type value;
  Type type;
};

// function signature for the builtins
using path = const std::vector<std::string>&;

struct Functor {
  using sig = std::function<Symbol(std::list<Symbol>)>;
  using psig = std::function<Symbol(std::list<Symbol>, path P)>;
  Functor() :
    _fn([](std::list<Symbol> l) -> Symbol {
      return Symbol("", "Dummy", Type::String);
    }),
    _Pfn([](std::list<Symbol> l, path PATH) -> Symbol {
      return Symbol("", "Dummy", Type::String);
    })
  {}
  Functor(sig f) { _fn = f; };
  Functor(psig f) { _Pfn = f; };
  //Functor(Functor& other) { _fn = other._fn; _Pfn = other._Pfn; }
  auto operator()(std::list<Symbol> l, path P) -> Symbol {
    return (_Pfn) ? _Pfn(l, P) : _fn(l);
  };
  auto operator()(std::list<Symbol> l) -> Symbol{ return _fn(l); };
private:
  sig _fn;
  psig _Pfn;
};
