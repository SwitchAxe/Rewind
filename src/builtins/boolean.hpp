#pragma once
#include "numeric.hpp"
#include <utility>

bool convert_value_to_bool(Symbol sym) {
  bool clause;
  clause = std::visit(overloaded {
    [](std::string s) { return !s.empty(); },
    [](signed long long int x) { return x != 0; },
    [](unsigned long long int x) { return x != 0; },
    [](bool b) { return b; },
    [](std::list<Symbol> l) { return !l.empty(); },
    [](std::monostate) { return false; }
  }, sym.value);
  return clause;
}

std::map<std::string, Functor> boolean = {
  std::pair{"=", Functor{[](std::list<Symbol> args) -> Symbol {
    bool is_true = true;
    Symbol prev = args.front();
    args.pop_front();
    for (auto e : args) {
      if ((e.type == prev.type) && (e.type == Type::Number)) {
	is_true = is_true && std::visit(
					[]<class T, class U>(T t, U u) -> bool {
					  return std::cmp_equal(t, u);
					},
					get_int(e.value), get_int(prev.value));
	continue;
      }
      is_true = is_true && (e.value == prev.value);
      prev = e;
    }
    return Symbol("", is_true, Type::Boolean);
  }}},
  std::pair{"!=", Functor{[](std::list<Symbol> args) -> Symbol {
    bool is_true = true;
    Symbol prev = args.front();
    args.pop_front();
    Symbol lhs;
    for (auto e : args) {
      if ((e.type == prev.type) && (e.type == Type::Number)) {
        is_true = is_true && std::visit(
					[]<class T, class U>(T t, U u) -> bool {
					  return !std::cmp_equal(t, u);
					},
					get_int(e.value), get_int(prev.value));
        continue;
      }
      if (e.type != prev.type) {
        return Symbol("", true, Type::Boolean);
      }
      lhs = e;
      is_true = is_true && (lhs.value != prev.value);
      prev = e;
    }
    return Symbol("", is_true, Type::Boolean);
  }}},
  std::pair{"and", Functor{[](std::list<Symbol> args) -> Symbol {
    bool is_true = true;
    Symbol clause;
    for (auto e : args) {
      if (e.type != Type::Boolean) {
        throw std::logic_error{"Type mismatch in the 'and' operator: Only "
                               "booleans are allowed!\n"};
      }
      is_true = is_true && std::get<bool>(e.value);
      if (!is_true) {
        return Symbol("", false, Type::Boolean);
      }
    }
    return Symbol("", is_true, Type::Boolean);
  }}},
  std::pair{"or", Functor{[](std::list<Symbol> args) -> Symbol {
    bool is_true = false;
    Symbol clause;
    for (auto e : args) {
      if (e.type != Type::Boolean) {
        throw std::logic_error{"Type mismatch in the 'or' operator: Only "
                               "booleans are allowed!\n"};
      }
      is_true = is_true || std::get<bool>(e.value);
      if (is_true) {
        return Symbol("", true, Type::Boolean);
      }
    }
    return Symbol("", is_true, Type::Boolean);
  }}},
  std::pair{"not", Functor{[](std::list<Symbol> args) -> Symbol {
    if (args.size() > 1) {
      throw std::logic_error{
        "Exception: the 'not' operator only accepts 0 or 1 arguments!\n"};
    }
    if (args.empty()) {
      return Symbol("", false, Type::Boolean);
    }
    if (args.front().type != Type::Boolean) {
      throw std::logic_error{
        "Exception: the 'not' operator must accept 0 or 1 booleans!\n"};
    }
    return Symbol("", !std::get<bool>(args.front().value), Type::Boolean);
  }}},
};
