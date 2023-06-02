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

// information we need to keep track of in get_ast_aux
struct RecInfo {
  Symbol result;
  int end_index;
  // how deep we are within a 'let' form, up until the first function call
  // -1: no 'let' found
  // 0: keyword
  // 1: identifier
  // 2: args (if any)
  // 3: fun_call or value (this measures 2 if there's no args i.e. no function)
  int let_level;
};

RecInfo get_ast_aux(std::vector<std::string> tokens, int si, int ei,
                    bool must_call, bool in_call, int let_level, path PATH) {
  RecInfo res;
  res.let_level = let_level;
  auto as_list = std::list<Symbol>();
  res.result = Symbol("", as_list, Type::List);
  Symbol lambda_parameters;
  Symbol lambda_condition;
  int lambda_id = 0;
  for (int i = si; i < ei; ++i) {
    auto cur = tokens[i];
    if ((cur == "(") || (cur == "[") || (cur == ",")) {
      RecInfo info = get_ast_aux(tokens, i + 1, ei, true, false,
                                 (res.let_level == 1) ? 2 : -1, PATH);
      i = info.end_index;
      as_list.push_back(info.result);
      res.let_level = info.let_level;
    } else if ((cur == ")") || (cur == "]")) {
      return {.result = res.result,
              .end_index = i,
              .let_level = (let_level == 2) ? 3 : -1};
    } else if (is_strlit(cur)) {
      as_list.push_back(
          Symbol("", cur.substr(1, cur.size() - 2), Type::String));
    } else if (auto v = try_convert_num(cur); v != std::nullopt) {
      as_list.push_back(Symbol("", *v, Type::Number));
    } else if (cur == ";") {
      return {.result = res.result, .end_index = i, .let_level = -1};
    } else if (cur == "=>") {
      // pop the last list we got and make it the arguments of our lambda
      // throw an exception if there's no such list
      // then we can recurse on the rest of the tokens, using the fact that
      // lambdas are terminated by ';', so we can safely stop recursion when
      // we find this token (see the base case above this branch)
      if (as_list.empty() || (as_list.back().type != Type::List)) {
        throw std::logic_error{"Parser error: Found a lambda token ('=>') with "
                               "no list before it."};
      }
      lambda_parameters = as_list.back();
      as_list.pop_back();
      RecInfo info = get_ast_aux(tokens, i + 1, ei, true, false, -1, PATH);
      // now info.result should contain the entirety of our lambda's body
      std::string id = "__re_lambda" + std::to_string(lambda_id);
      user_defined_procedures[user_defined_procedures.size() - 1]
          .insert_or_assign(id, std::make_pair(lambda_parameters, info.result));
      lambda_id++;
      as_list.push_back(Symbol("", id, Type::Identifier));
    } else if (cur == "?=>") {
      // exact same considerations as above, but we also require a second list
      // to act as the condition
      if (as_list.size() < 2) {
        throw std::logic_error{"Parser error: too few (< 2) nodes in the AST "
                               "before a conditional lambda."};
      }
      if (as_list.back().type != Type::List) {
        throw std::logic_error{
            "Parser error: The lambda condition is not a list."};
      }
      auto cond = as_list.back();
      as_list.pop_back();
      if (as_list.back().type != Type::List) {
        throw std::logic_error{
            "Parser error: The lambda parameters are not a list."};
      }
      auto params = as_list.back();
      as_list.pop_back();
      std::string id = "__re_clambda" + std::to_string(lambda_id);
      RecInfo info = get_ast_aux(tokens, i + 1, ei, true, false, -1, PATH);
      user_defined_procedures[user_defined_procedures.size() - 1]
          .insert_or_assign(
              id, std::make_pair(
                      Symbol("", std::list<Symbol>{params, cond}, Type::List),
                      info.result));
    } else {
      if (cur[0] == '$') {
        Symbol sym;
        Symbol varname = Symbol("", cur.substr(1), Type::Identifier);
        if (auto cs = callstack_variable_lookup(varname); cs != std::nullopt) {
          sym = *cs;
        } else if (auto vs = variable_lookup(varname); vs != std::nullopt) {
          sym = *vs;
        } else
          throw std::logic_error{"Unbound variable " + cur.substr(1) + ".\n"};
        as_list.push_back(sym);
      } else {
        if (res.let_level > 1) {
          // found a function call with or without the 'let' having any args
          RecInfo info = get_ast_aux(tokens, i + 1, ei, true, false, -1, PATH);
          i = info.end_index;
          auto sym = info.result;
          auto op = Symbol("", cur,
                           procedures.contains(cur) ? Type::Operator
                                                    : Type::Identifier);
          auto l = std::get<std::list<Symbol>>(sym.value);
          l.push_front(op);
          sym.value = l;
          as_list.push_back(sym);
          res.let_level = -1;
        } else {
          if (res.let_level > -1) {
            ++res.let_level;
          }
          if (cur == "let") {
            if (res.let_level > 0) {
              throw std::logic_error{"Parser error: Found a 'let' special form "
                                     "inside another 'let'...?"};
            }
            res.let_level++;
          }
          as_list.push_back(Symbol(
              (i == 0) ? "root" : "", cur,
              procedures.contains(cur) ? Type::Operator : Type::Identifier));
        }
      }
    }
    res.result = Symbol((i == 0) ? "root" : "", as_list, Type::List);
  }
  return res;
}

Symbol get_ast(std::vector<std::string> tokens, path PATH) {
  auto fexpr = get_ast_aux(tokens, 0, tokens.size(), false, false, -1, PATH);
  if (fexpr.result.type == Type::List) {
    if (auto l = std::get<std::list<Symbol>>(fexpr.result.value);
        l.size() == 1) {
      if (std::holds_alternative<std::string>(l.back().value)) {
        if (auto s = std::get<std::string>(l.back().value);
            !procedures.contains(s) &&
            (user_defined_procedures.empty() ||
             !user_defined_procedures.back().contains(s)) &&
            (get_absolute_path(s, PATH) == std::nullopt)) {
          return l.back();
        }
      } else
        return l.back();
    }
  }
  return fexpr.result;
}

// DEBUG PURPOSES ONLY and for printing the final result until i
// make an iterative version of this thing

std::string rec_print_ast(Symbol root, bool debug) {
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
