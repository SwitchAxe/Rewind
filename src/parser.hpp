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
#include "lexer.hpp"
#include "procedures.hpp"
#include "types.hpp"
#include <charconv>
#include <iostream>
#include <stack>
#include <type_traits>

bool is_strlit(std::string s) {
  return (s.size() > 1) && (s[0] == '"') && (s.back() == '"');
}

std::optional<long long signed int> try_convert_num(std::string n) {
  long long int v;
  auto [ptr, ec] = std::from_chars(n.data(), n.data() + n.size(), v);
  if (ptr == n.data() + n.size()) {
    return v;
  }
  return std::nullopt;
}

Symbol get_ast(std::vector<std::string> tokens, path PATH) {
  Symbol result;
  std::stack<Symbol> stk;
  bool in_lambda_fn = false;
  std::list<Symbol> lambda_fn_statements;
  std::list<Symbol> lambda_fn_parameters;
  static int lambda_id = 0;
  bool in_def = false;
  bool in_def_past_name = false;
  int bracket_balance = 0;
  for (auto tk : tokens) {
    if (procedures.contains(tk)) {
      if (tk == "let") {
        in_def = true;
      }
      if (stk.empty() ||
          ((stk.size() > 0) &&
           (std::get<std::list<Symbol>>(stk.top().value).size() > 0))) {
        auto l = std::list<Symbol>{Symbol("", tk, Type::Operator)};
        auto sym = Symbol("", l, Type::List);
        if (stk.size() > 0) {
          auto parent = stk.top();
          stk.pop();
          auto l = std::get<std::list<Symbol>>(parent.value);
          parent.value = l;
          stk.push(parent);
          stk.push(sym);
        } else
          stk.push(sym);
      } else {
        auto sym = Symbol("", tk, Type::Operator);
        auto last = stk.top();
        stk.pop();
        auto l = std::get<std::list<Symbol>>(last.value);
        l.push_back(sym);
        last.value = l;
        stk.push(last);
      }
    } else if (auto v = try_convert_num(tk); v != std::nullopt) {
      auto sym = Symbol("", *v, Type::Number);
      if (stk.empty()) {
        sym.name = "root";
        return sym;
      }
      auto cur = stk.top();
      stk.pop();
      auto l = std::get<std::list<Symbol>>(cur.value);
      l.push_back(sym);
      cur.value = l;
      stk.push(cur);
    } else if (is_strlit(tk)) {
      auto sym = Symbol("", tk.substr(1, tk.size() - 2), Type::String);
      if (stk.empty()) {
        sym.name = "root";
        return sym;
      }
      auto cur = stk.top();
      stk.pop();
      auto l = std::get<std::list<Symbol>>(cur.value);
      l.push_back(sym);
      cur.value = l;
      stk.push(cur);
    } else if ((tk == ")") || (tk == "]") || (tk == ",")) {
      if (in_lambda_fn) {
        lambda_fn_statements.push_back(stk.top());
        stk.pop();
      } else {
        if (stk.empty()) {
          throw std::logic_error{"Rewind: Exception in the parser, "
                                 "Unexpected closing paren while the stack "
                                 "is empty."};
        }
        auto last = stk.top();
        stk.pop();
        auto parent = stk.top();
        stk.pop();
        auto l = std::get<std::list<Symbol>>(parent.value);
        l.push_back(last);
        parent.value = l;
        stk.push(parent);
      }
    } else if ((tk == "(") || (tk == "[")) {
      stk.push(Symbol("", std::list<Symbol>{}, Type::List));
    } else if (tk == ";") {
      if (in_lambda_fn) {
        in_lambda_fn = false;
        if (lambda_fn_statements.empty()) {
          lambda_fn_statements.push_back(stk.top());
        }
        user_defined_procedures[user_defined_procedures.size() - 1]
            .insert_or_assign(
                "__re_lambda" + std::to_string(lambda_id),
                std::make_pair(Symbol("", lambda_fn_parameters, Type::List),
                               Symbol("", lambda_fn_statements, Type::List)));
        stk.pop();
        auto last = stk.top();
        stk.pop();
        auto l = std::get<std::list<Symbol>>(last.value);
        l.push_back(Symbol("", "__re_lambda" + std::to_string(lambda_id),
                           Type::Identifier));
        last.value = l;
        stk.push(last);
      } else {
        if (stk.size() != 2) {
          if (stk.size() != 1) {
            throw std::logic_error{
                "Invalid token in the parser, ';' is a "
                "termination token for function definitions. "
                "(node stack too big)\n"};
          }
          auto root = stk.top();
          auto l = std::get<std::list<Symbol>>(root.value);
          auto keyword = l.front();
          auto as_string = std::get<std::string>(keyword.value);
          if (as_string != "let") {
            throw std::logic_error{
                "Invalid token in the parser, ';' is a "
                "termination token for function definitions. "
                "(non-matching 'let' found for ';')\n"};
          }
          return root;
        }
        auto last = stk.top();
        stk.pop();
        auto root = stk.top();
        auto l = std::get<std::list<Symbol>>(root.value);
        auto keyword = l.front();
        auto as_string = std::get<std::string>(keyword.value);
        if (as_string != "let") {
          throw std::logic_error{
              "Invalid token in the parser, ';' is a "
              "termination token for function definitions.\n"};
        }
        l.push_back(last);
        root.value = l;
        return root;
      }
    } else if (tk == "=>") {
      // at this point we already have a list in the expression,
      // we can consider it as a list of arguments for our lambda function:
      auto last = stk.top();
      stk.pop();
      auto l = std::get<std::list<Symbol>>(last.value);
      auto args = l.back();
      l.pop_back();
      last.value = l;
      stk.push(last);
      in_lambda_fn = true;
      lambda_id++;
      lambda_fn_parameters = std::get<std::list<Symbol>>(args.value);
    } else {
      auto sym = Symbol("", tk, Type::Identifier);
      if (stk.empty() && ((tk[0] != '$') && (tk[0] != '%')))
        stk.push(Symbol("root", std::list<Symbol>{sym}, Type::List));
      else if (tk[0] == '%') {
        sym.value = tk.substr(1);
        stk.push(sym);
      } else if (tk[0] == '$') {
        auto varname = Symbol("", tk.substr(1), Type::Identifier);
        Symbol var_lookup;
        if (auto cs = callstack_variable_lookup(varname); cs != std::nullopt) {
          stk.push(*cs);
        } else if (auto vs = variable_lookup(varname); vs != std::nullopt) {
          stk.push(*vs);
        } else
          throw std::logic_error{"Unbound variable" + tk.substr(1) + ".\n"};
      } else {
        if (in_def_past_name && (stk.size() == 1)) {
          in_def_past_name = false;
          stk.push(Symbol("", std::list<Symbol>{}, Type::List));
        }
        if (in_def) {
          in_def_past_name = true;
          in_def = false;
        }
        if (std::holds_alternative<std::list<Symbol>>(stk.top().value) &&
            std::get<std::list<Symbol>>(stk.top().value).empty()) {
          stk.pop();
          stk.push(Symbol("",
                          std::list<Symbol>{Symbol("", tk, Type::Identifier)},
                          Type::List));
        } else {
          auto last = stk.top();
          stk.pop();
          auto l = std::get<std::list<Symbol>>(last.value);
          l.push_back(Symbol("", tk, Type::Identifier));
          last.value = l;
          stk.push(last);
        }
      }
    }
  }
  return stk.top();
}

// DEBUG PURPOSES ONLY and for printing the final result until i
// make an iterative version of this thing

std::string rec_print_ast(Symbol root, bool debug) {
  std::cout << std::boolalpha;
  std::string res;
  if (std::holds_alternative<std::list<Symbol>>(root.value)) {
    if (debug)
      res += std::string{(root.type == Type::RawAst) ? "(Ast)" : "(List) "} +
             " [ ";
    else
      res += "[ ";
    for (auto s : std::get<std::list<Symbol>>(root.value)) {
      res += rec_print_ast(s, debug) + " ";
    }
    res += "]";
  } else {
    std::visit(
        [&]<class T>(T &&v) -> void {
          if constexpr (std::is_same_v<std::decay_t<T>, std::monostate>) {
          } else if constexpr (std::is_same_v<std::decay_t<T>,
                                              std::list<Symbol>>) {
          } else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
            if (debug)
              res += ((root.type == Type::String) ? "(Str) " : "(Id) ") +
                     process_escapes(v);
            else
              res += process_escapes(v);
          } else if (std::is_same_v<std::decay_t<T>, bool>) {
            if (debug)
              res += std::string{(v ? "(Bool) true" : "(Bool) false")};
            else
              res += v ? "true" : "false";
          } else if (debug)
            res += ((root.type == Type::Number) ? "(Num) " : "(?) ") +
                   std::to_string(v);
          else
            res += std::to_string(v);
        },
        root.value);
  }
  return res;
}
