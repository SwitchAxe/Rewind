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
  bool in_lambda_function = false;
  bool in_lambda_arguments = false;
  Symbol lambda_arguments = Symbol("", std::list<Symbol>{}, Type::List);
  Symbol lambda_condition = Symbol("", std::list<Symbol>{}, Type::List);
  Symbol lambda_statements = Symbol("", std::list<Symbol>{}, Type::List);
  bool in_lambda_condition = false;
  bool in_lambda_statements = false;
  bool must_call = true;
  bool in_call = false;
  std::stack<Symbol> stk;
  Symbol final_expr = Symbol("root", std::list<Symbol>{}, Type::List);
  static int lambda_n = 0;
  for (auto tk : tokens) {
    if (tk == ",") {
      if (in_lambda_function) {
        auto l = std::get<std::list<Symbol>>(lambda_statements.value);
        auto r = std::get<std::list<Symbol>>(final_expr.value);
        if (r.size() == 1)
          l.push_back(r.back());
        else
          l.push_back(final_expr);
        lambda_statements.value = l;
        final_expr.value = std::list<Symbol>{};
      } else {
        if (stk.size() > 1) {
          auto top = stk.top();
          stk.pop();
          auto l = std::get<std::list<Symbol>>(top.value);
          l.push_back(final_expr);
          final_expr.value = l;
        }
        must_call = true;
        in_call = false;
      }
    } else if ((tk == "(") || (tk == "[")) {
      stk.push(final_expr);
      final_expr.value = std::list<Symbol>{};
    } else if ((tk == ")") || (tk == "]")) {
      auto top = stk.top();
      stk.pop();
      auto l = std::get<std::list<Symbol>>(top.value);
      l.push_back(final_expr);
      final_expr.value = l;
      must_call = true;
      in_call = false;
    } else if (tk == ";") {
      if (in_lambda_function) {
        std::string id = "__re_lambda" + std::to_string(lambda_n);
        if (!lambda_condition.value.valueless_by_exception()) {
          id = "__re_clambda" + std::to_string(lambda_n);
        }
        auto l = std::get<std::list<Symbol>>(final_expr.value);
        if (l.size() > 0) {
          auto l = std::get<std::list<Symbol>>(lambda_statements.value);
          auto r = std::get<std::list<Symbol>>(final_expr.value);
          if (r.size() == 1)
            l.push_back(r.back());
          else
            l.push_back(final_expr);

          lambda_statements.value = l;
        }
        user_defined_procedures[user_defined_procedures.size() - 1]
            .insert_or_assign(
                id,
                std::make_pair((lambda_condition.value.valueless_by_exception())
                                   ? lambda_arguments
                                   : Symbol("",
                                            std::list<Symbol>{lambda_arguments,
                                                              lambda_condition},
                                            Type::List),
                               lambda_statements));
        lambda_n++;
        final_expr = stk.top();
        l = std::get<std::list<Symbol>>(final_expr.value);
        l.push_back(Symbol("", id, Type::Identifier));
        final_expr.value = l;
        stk.pop();
        in_lambda_function = false;
      } else {
        if (stk.size() == 1) {
          auto top = stk.top();
          auto l = std::get<std::list<Symbol>>(top.value);
          l.push_back(final_expr);
          top.value = l;
          return top;
        } else {
          auto top = stk.top();
          auto l = std::get<std::list<Symbol>>(top.value);
          l.push_back(final_expr);
          top.value = l;
          stk.pop();
          while (stk.size() > 0) {
            auto p = stk.top();
            stk.pop();
            auto l = std::get<std::list<Symbol>>(p.value);
            l.push_back(top);
            p.value = l;
            top = p;
          }
          return top;
        }
      }
    } else if (tk == "=>") {
      auto l = std::get<std::list<Symbol>>(final_expr.value);
      if (l.empty() || (l.back().type != Type::List)) {
        throw std::logic_error{"Missing parameters to a lambda function!\n"};
      }
      in_lambda_function = true;
      lambda_arguments = l.back();
      l.pop_back();
      final_expr.value = l;
      stk.push(final_expr);
      final_expr.value = std::list<Symbol>{};
    } else if (tk == "?=>") {
      auto l = std::get<std::list<Symbol>>(final_expr.value);
      if ((l.size() < 2) || (l.back().type != Type::List)) {
        throw std::logic_error{"Missing parameters or a condition to a "
                               "conditional lambda function!\n"};
      }
      in_lambda_function = true;
      lambda_condition = l.back();
      l.pop_back();
      lambda_arguments = l.back();
      l.pop_back();
      final_expr.value = l;
      stk.push(final_expr);
      final_expr.value = std::list<Symbol>{};
    } else if (is_strlit(tk)) {
      Symbol sym = Symbol("", tk, Type::String);
      auto l = std::get<std::list<Symbol>>(final_expr.value);
      l.push_back(sym);
      final_expr.value = l;
    } else if (auto v = try_convert_num(tk); v != std::nullopt) {
      Symbol sym = Symbol("", *v, Type::Number);
      auto l = std::get<std::list<Symbol>>(final_expr.value);
      l.push_back(sym);
      final_expr.value = l;
    } else {
      if (tk[0] == '$') {
        auto varname = Symbol("", tk.substr(1), Type::Identifier);
        Symbol var_lookup;
        Symbol sym;
        if (auto cs = callstack_variable_lookup(varname); cs != std::nullopt) {
          sym = *cs;
        } else if (auto vs = variable_lookup(varname); vs != std::nullopt) {
          sym = *vs;
        } else
          throw std::logic_error{"Unbound variable" + tk.substr(1) + ".\n"};
        auto l = std::get<std::list<Symbol>>(final_expr.value);
        if (l.empty() && stk.empty()) {
          return sym;
        }
	l.push_back(sym);
        final_expr.value = l;
      } else {
        auto l = std::get<std::list<Symbol>>(final_expr.value);
        if ((l.empty()) && (stk.empty()) &&
            ((user_defined_procedures.empty()) ||
             !user_defined_procedures.back().contains(tk)) &&
            !procedures.contains(tk)) {
	  return Symbol("root", tk, Type::String);
	}
          if (must_call) {
            if (!l.empty())
              stk.push(final_expr);
            final_expr =
                Symbol(stk.empty() ? "root" : "",
                       std::list<Symbol>{Symbol("", tk,
                                                procedures.contains(tk)
                                                    ? Type::Operator
                                                    : Type::Identifier)},
                       Type::List);
            must_call = false;
            in_call = true;
          } else {
            l.push_back(Symbol("", tk,
                               procedures.contains(tk) ? Type::Operator
                                                       : Type::Identifier));
            final_expr.value = l;
          }
      }
    }
  }
  return final_expr;
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
