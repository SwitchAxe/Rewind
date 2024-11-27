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
#include "utils.hpp"
#include <functional>
#include <list>
#include <map>
#include <string>
#include <variant>
#include <vector>

enum class Type {
    String,
    Number,
    Boolean,
    List,
    ListLiteral,
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

using _Type = std::variant<std::monostate, long long int, long long unsigned int,
    std::string, std::list<Symbol>, bool>;

struct Symbol {
    Symbol() = default;
    bool operator==(const Symbol&) const = default;
    Symbol(std::string _n, _Type _v, Type _t)
    {
        name = _n;
        value = _v;
        type = _t;
        is_global = false;
    }
    Symbol(std::string _n, _Type _v, Type _t, bool _b)
    {
        name = _n;
        value = _v;
        type = _t;
        is_global = _b;
    }
    std::string name; // empty string if not present
    _Type value;
    Type type;
    bool is_block;
    bool is_global;
    int line;
    // this is associated with any symbol, but it's useful for
    // functions.
    std::map<std::string, Symbol> variables;
};

// function signature for the builtins
using path = std::vector<std::string>;
using variables = std::map<std::string, Symbol>;
struct Functor {
    using sig = std::function<Symbol(std::list<Symbol>)>;
    using psig = std::function<Symbol(std::list<Symbol>, path)>;
    using pvsig = std::function<Symbol(std::list<Symbol>, path, variables&)>;
    using vsig = std::function<Symbol(std::list<Symbol>, variables&)>;
    Functor()
        : _fn([](std::list<Symbol>) { return Symbol(); })
        , _Pfn([](std::list<Symbol>, path) { return Symbol(); })
        , _PVfn([](std::list<Symbol>, path, variables&) { return Symbol(); })
    {
    }
    Functor(sig&& s)
        : _fn(s)
    {
    }
    Functor(psig&& s)
        : _Pfn(s)
    {
    }
    Functor(pvsig&& s)
        : _PVfn(s)
    {
    }
    Functor(vsig&& s)
        : _Vfn(s)
    {
    }
    auto operator()(std::list<Symbol> l, path P, variables& v)
    {
        if (_PVfn)
            return _PVfn(l, P, v);
        if (_Pfn)
            return _Pfn(l, P);
        return _fn(l);
    }

    auto operator()(std::list<Symbol> l, variables& V) -> Symbol
    {
        return (_Vfn) ? _Vfn(l, V) : _fn(l);
    };

    auto operator()(std::list<Symbol> l, path P) -> Symbol
    {
        return (_Pfn) ? _Pfn(l, P) : _fn(l);
    };
    auto operator()(std::list<Symbol> l) -> Symbol { return _fn(l); };
    sig _fn;
    psig _Pfn;
    pvsig _PVfn;
    vsig _Vfn;
};

template <class T>
concept not_bool = !std::same_as<std::decay_t<T>, bool>;

template <class T>
concept is_integer = std::integral<T> && not_bool<T>;
