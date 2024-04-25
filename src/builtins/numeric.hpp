#pragma once
#include "../include.hpp"
#include <utility>

using ints = std::variant<long long int, long long unsigned int>;

ints get_int(_Type n) {
  return std::visit(
      []<class T>(T t) -> ints {
        if constexpr (std::is_same_v<T, long long unsigned int> ||
                      std::is_same_v<T, long long signed int>) {
          return t;
        }
        return 0;
      },
      n);
}

std::map<std::string, Functor> numeric = {
  std::pair{"+", Functor{[](std::list<Symbol> args) -> Symbol {
    int r = 0;
    for (auto e : args) {
      if (e.type == Type::Defunc)
        continue;
      if (e.type != Type::Number) {
        throw std::logic_error{"Unexpected operand to the '+' procedure!\n"};
      }
      if (std::holds_alternative<long long int>(e.value))
        r += std::get<long long int>(e.value);
      else
        r += std::get<long long unsigned int>(e.value);
    }
    Symbol ret("", r, Type::Number);
    return ret;
  }}},
  std::pair{"-", Functor{[](std::list<Symbol> args) -> Symbol {
    long long int r;
    if (args.front().type != Type::Number) {
      throw std::logic_error{"Unexpected operand to the '-' procedure!\n"};
    }
    if (std::holds_alternative<long long int>(args.front().value))
      r = std::get<long long int>(args.front().value);
    else
      r = std::get<long long unsigned int>(args.front().value);
    args.pop_front();
    for (auto e : args) {
      if (e.type == Type::Defunc)
        continue;
      if (e.type != Type::Number) {
        throw std::logic_error{"Unexpected operand to the '-' procedure!\n"};
      }
      if (std::holds_alternative<long long int>(e.value))
        r -= std::get<long long int>(e.value);
      else
        r -= std::get<long long unsigned int>(e.value);
    }
    Symbol ret("", r, Type::Number);
    return ret;
  }}},
  std::pair{"/", Functor{[](std::list<Symbol> args) -> Symbol {
    int r;
    if (args.front().type != Type::Number) {
      throw std::logic_error{"Unexpected operand to the '/' procedure!\n"};
    }
    if (std::holds_alternative<long long int>(args.front().value))

      r = std::get<long long int>(args.front().value);
    else
      r = std::get<long long unsigned int>(args.front().value);
    args.pop_front();
    for (auto e : args) {
      if (e.type == Type::Defunc)
        continue;
      if (e.type != Type::Number) {
        throw std::logic_error{"Unexpected operand to the '/' procedure!\n"};
      }
      if (std::holds_alternative<long long int>(e.value))
        r /= std::get<long long int>(e.value);
      else
        r /= std::get<long long unsigned int>(e.value);
    }
    Symbol ret("", r, Type::Number);
    return ret;
  }}},
  std::pair{"%", Functor{[](std::list<Symbol> args) -> Symbol {
    if ((args.size() != 2) || (args.front().type != args.back().type) ||
        (args.front().type != Type::Number) ||
        (args.back().type != Type::Number)) {
      throw std::logic_error{
        "Exception: The 'modulus' operator only accepts two integers!\n"};
    }
    return Symbol("",
                  std::visit(
                             [](auto t, auto u) -> long long unsigned int {
                               return std::modulus<>{}(t, u);
                             },
                             get_int(args.front().value),
                             get_int(args.back().value)),
                  Type::Number);
  }}},
  std::pair{"*", Functor{[](std::list<Symbol> args) -> Symbol {
    int r;
    if (args.front().type != Type::Number) {
      throw std::logic_error{"Unexpected operand to the '*' procedure!\n"};
    }
    if (std::holds_alternative<long long int>(args.front().value))
      r = std::get<long long int>(args.front().value);
    else
      r = std::get<long long unsigned int>(args.front().value);
    args.pop_front();
    for (auto e : args) {
      if (e.type == Type::Defunc)
        continue;
      if (e.type != Type::Number) {
        throw std::logic_error{"Unexpected operand to the '*' procedure!\n"};
      }
      if (std::holds_alternative<long long int>(e.value))
        r *= std::get<long long int>(e.value);
      else
        r *= std::get<long long unsigned int>(e.value);
    }
    Symbol ret("", r, Type::Number);
    return ret;
  }}},
  std::pair{"<", Functor{[](std::list<Symbol> args) -> Symbol {
    bool is_true = true;
    if (args.empty())
      return Symbol("", true, Type::Boolean);
    auto first = args.front();
    args.pop_front();
    for (auto e : args) {
      if ((first.type != Type::Number) || (e.type != Type::Number)) {
        throw std::logic_error{"The '<' operator only accepts integers!\n"};
      }
      // uses (in)equality operator overloads for integer variants
      if ((e.type == first.type) && (e.type == Type::Number)) {
        is_true = is_true && std::visit(
					[]<class T, class U>(T t, U u) -> bool {
					  return std::cmp_greater(t, u);
					},
					get_int(e.value), get_int(first.value));
        continue;
      }
      first = e;
    }
    return Symbol("", is_true, Type::Boolean);
  }}},
};
