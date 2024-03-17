#pragma once
// Copyright 2023 Sofia Cerasuoli (@SwitchAxe)
/*
  This file is part of Rewind.
  Rewind is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation, either version 3 of the license, or (at your
  option) any later version.
  Rewind is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  Rewind. If not, see <https://www.gnu.org/licenses/>.
*/
#include <functional>
#include <list>
#include <string>
#include <variant>
#include <vector>

enum class Type {
  String,
  Number,
  Boolean,
  List,
  Function,
  Operator,
  Defunc,
  Funcall,
  RecFunCall,
  Binary,
  Identifier,
  Command,
  CommandResult,
  Error,
  RawAst, // for the 'ast' builtin. don't eval this type!
};
struct Symbol;

using _Type =
    std::variant<std::monostate, long long int, long long unsigned int,
                 std::string, std::list<Symbol>, bool>;

struct Symbol {
  Symbol() = default;
  auto operator<=>(const Symbol &) const = default;
  Symbol(std::string _n, _Type _v, Type _t) {
    name = _n;
    value = _v;
    type = _t;
    is_lit = false;
  }
  Symbol(std::string _n, _Type _v, Type _t, bool _b) {
    name = _n;
    value = _v;
    type = _t;
    is_lit = _b;
  }
  std::string name; // empty string if not present
  _Type value;
  Type type;
  bool is_lit;
};

// function signature for the builtins
using path = std::vector<std::string>;

struct Functor {
  using sig = std::function<Symbol(std::list<Symbol>)>;
  using psig = std::function<Symbol(std::list<Symbol>, path P)>;
  Functor()
      : _fn([](std::list<Symbol> l) -> Symbol {
          return Symbol("", "Dummy", Type::String);
        }),
        _Pfn([](std::list<Symbol> l, path PATH) -> Symbol {
          return Symbol("", "Dummy", Type::String);
        }) {}
  Functor(sig f) { _fn = f; };
  Functor(psig f) { _Pfn = f; };
  // Functor(Functor& other) { _fn = other._fn; _Pfn = other._Pfn; }
  auto operator()(std::list<Symbol> l, path P) -> Symbol {
    return (_Pfn) ? _Pfn(l, P) : _fn(l);
  };
  auto operator()(std::list<Symbol> l) -> Symbol { return _fn(l); };

private:
  sig _fn;
  psig _Pfn;
};

template <class T>
concept not_bool = !std::same_as<std::decay_t<T>, bool>;

template <class T>
concept is_integer = std::integral<T> && not_bool<T>;
