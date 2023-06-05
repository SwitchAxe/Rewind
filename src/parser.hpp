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

enum class State {
  None,
  Identifier,
  InArgumentList,
  FirstFunctionCall,
  InFunCallArguments,
  List,
  Comma,
  Semicolon,
  LambdaFunction,
  LambdaFunctionFirstFunctionCall,
  LambdaFunctionInArgumentList,
  LambdaFunctionLiteral,
  LambdaFunctionIdentifier,
  LambdaFunctionArgumentList,
  End,
  Error,
};

// information we need to keep track of in get_ast_aux
struct RecInfo {
  Symbol result;
  int end_index;
  State st;
};

RecInfo get_ast_aux(std::vector<std::string> tokens, int si, int ei,
                    bool is_root, path PATH, int level, State st) {
  RecInfo res;
  std::list<Symbol> as_list;
  res.result = Symbol(is_root ? "root" : "", as_list, Type::List);
  State cur_state = st;
  static int lambda_id = 0;
  for (int i = si; i < ei; ++i) {
    auto cur = tokens[i];
    if ((cur == "(") || (cur == "[")) {
      RecInfo info = get_ast_aux(tokens, i + 1, ei, false, PATH, level + 1,
                                 ((cur_state == State::FirstFunctionCall) ||
                                  (cur_state == State::Identifier))
                                     ? State::InArgumentList
                                     : State::List);
      i = info.end_index;
      as_list.push_back(info.result);
      cur_state = info.st;
      res.st = cur_state;
    } else if ((cur == ")") || (cur == "]")) {
      res.result.value = as_list;
      return {.result = res.result,
              .end_index = i,
              .st = (cur_state == State::InArgumentList)
                        ? State::FirstFunctionCall
                        : State::InFunCallArguments};
    }

    else if (cur == ";") {
      res.result.value = as_list;
      if (level > 0) {
        return {.result = res.result, .end_index = i, .st = State::Semicolon};
      }
      return {.result = res.result, .end_index = i, .st = State::End};
    } else if (cur == ",") {
      res.result.value = as_list;
      if (cur_state == State::LambdaFunctionLiteral) {
        cur_state = State::LambdaFunctionFirstFunctionCall;
      }
      if (cur_state == State::LambdaFunctionIdentifier) {
        cur_state = State::LambdaFunctionFirstFunctionCall;
      }
      if (cur_state == State::LambdaFunctionInArgumentList) {
	cur_state = State::LambdaFunctionFirstFunctionCall;
      }
      return {.result = res.result,
              .end_index = i,
              .st =
                  (level > 0)
                      ? (((cur_state == State::LambdaFunctionInArgumentList) ||
                          (cur_state == State::LambdaFunctionFirstFunctionCall))
                             ? State::LambdaFunctionFirstFunctionCall
                             : State::Comma)
                      : State::Error};
    } else if (is_strlit(cur)) {
      as_list.push_back(Symbol(is_root ? "root" : "",
                               cur.substr(1, cur.size() - 2), Type::String));
      if (cur_state == State::LambdaFunctionFirstFunctionCall) {
        cur_state = State::LambdaFunctionLiteral;
      } else if (cur_state == State::LambdaFunctionIdentifier) {
	cur_state = State::LambdaFunctionFirstFunctionCall;
      }
    } else if (auto v = try_convert_num(cur); v != std::nullopt) {
      as_list.push_back(Symbol(is_root ? "root" : "", *v, Type::Number));
      if (cur_state == State::LambdaFunctionFirstFunctionCall) {
        cur_state = State::LambdaFunctionLiteral;
      } else if (cur_state == State::LambdaFunctionIdentifier) {
        cur_state = State::LambdaFunctionFirstFunctionCall;
      }
    } else if (cur == "=>") {
      if (as_list.empty() || as_list.back().type != Type::List) {
        throw std::logic_error{"Parser error: Found a lambda token ('=>') with "
                               "no parameter list before it!\n"};
      }
      std::string id = "__re_lambda" + std::to_string(lambda_id);
      RecInfo info = get_ast_aux(tokens, i + 1, ei, false, PATH, level + 1,
                                 State::LambdaFunctionFirstFunctionCall);
      cur_state = State::None;
      auto body = info.result;
      // keep iterating until we finish the statements composing the lambda
      // function
      while ((info.st == State::LambdaFunctionInArgumentList) ||
             (info.st == State::LambdaFunctionFirstFunctionCall)) {
        i = info.end_index;
        info = get_ast_aux(tokens, i + 1, ei, false, PATH, level + 1,
                           State::LambdaFunctionFirstFunctionCall);
        auto l = std::get<std::list<Symbol>>(body.value);
        l.push_back(info.result);
        body.value = l;
      }
      i = info.end_index;
      auto parameters = as_list.back();
      as_list.pop_back();
      if (user_defined_procedures.empty()) {
        user_defined_procedures.push_back({});
      }
      user_defined_procedures[user_defined_procedures.size() - 1]
          .insert_or_assign(id, std::make_pair(parameters, body));
      as_list.push_back(Symbol("", id, Type::Identifier));
      lambda_id++;
    } else if (cur == "?=>") {
      if (as_list.size() < 2) {
        throw std::logic_error{
            "Parser error: Found a conditional lambda token ('?=>') with "
            "missing condition and/or parameters before it!"};
      }
      auto cond = as_list.back();
      if (cond.type != Type::List) {
        throw std::logic_error{
            "Parser error: Condition for the lambda is not a list.\n"};
      }
      as_list.pop_back();
      auto params = as_list.back();
      if (params.type != Type::List) {
        throw std::logic_error{"Parser error: Parameters for the conditional "
                               "lambda are not a list.\n"};
      }
      as_list.pop_back();
      RecInfo info = get_ast_aux(tokens, i + 1, ei, false, PATH, level + 1,
                                 State::LambdaFunctionFirstFunctionCall);
      auto body = info.result;
      auto l = std::get<std::list<Symbol>>(body.value);
      if (l.size() != 2) {
        throw std::logic_error{"Parser error: A conditional lambda expects "
                               "only two comma-separated statements!\n"};
      }
      std::string id = "__re_clambda" + std::to_string(lambda_id);
      if (user_defined_procedures.empty()) {
        user_defined_procedures.push_back({});
      }
      user_defined_procedures[user_defined_procedures.size() - 1]
          .insert_or_assign(
              id, std::make_pair(
                      Symbol("", std::list<Symbol>{params, cond}, Type::List),
                      body));
      as_list.push_back(Symbol("", id, Type::Identifier));
      lambda_id++;
      i = info.end_index;
    } else {
      // identifiers are complicated, but we can somehow get around it
      // by having a fuckton of states to keep track of where we are
      if ((cur_state == State::FirstFunctionCall) ||
          (cur_state == State::None) ||
          (cur_state == State::LambdaFunctionFirstFunctionCall)) {
        RecInfo info = get_ast_aux(
            tokens, i + 1, ei, false, PATH, level + 1,
            cur == "let" ? (cur_state == State::LambdaFunctionFirstFunctionCall
                                ? State::LambdaFunctionIdentifier
                                : State::Identifier)
                         : (cur_state == State::LambdaFunctionFirstFunctionCall
                                ? State::LambdaFunctionInArgumentList
                                : State::InFunCallArguments));
        Symbol op = Symbol(is_root ? "root" : "", cur,
                           procedures.contains(cur) ? Type::Operator
                                                    : Type::Identifier);
        auto l = std::get<std::list<Symbol>>(info.result.value);
        l.push_front(op);
        i = info.end_index;
        cur_state = info.st;
	if (cur_state == State::Comma) {
	  cur_state = State::FirstFunctionCall;
	}
        if (cur_state == State::LambdaFunctionFirstFunctionCall) {
          auto sym = Symbol("", std::list<Symbol>{Symbol("", l, Type::List)},
                            Type::List);
          if (as_list.empty())
            as_list = std::get<std::list<Symbol>>(sym.value);
          else
            as_list.push_back(std::get<std::list<Symbol>>(sym.value).back());
        } else {
          if (as_list.empty()) {
            as_list = l;
          } else
            as_list.push_back(Symbol(is_root ? "root" : "", l, Type::List));
        }
        res.st = cur_state;
        if ((level > 0) || (cur_state == State::Semicolon)) {
          // early return to completely exit a function call when we're done
          // collecting all its arguments
          res.result.value = as_list;
          return {.result = res.result,
                  .end_index = i,
                  .st = cur_state};
        }
      } else {
        as_list.push_back(Symbol("", cur,
                                 procedures.contains(cur) ? Type::Operator
                                                          : Type::Identifier));
      }
      if (cur_state == State::Identifier) {
        cur_state = State::FirstFunctionCall;
      }
    }
    res.result.value = as_list;
  }
  return {.result = res.result, .end_index = ei, .st = State::None};
}

Symbol get_ast(std::vector<std::string> tokens, path PATH) {
  auto fexpr =
      get_ast_aux(tokens, 0, tokens.size(), true, PATH, 0, State::None);
  if (fexpr.result.type == Type::List) {
    if (auto l = std::get<std::list<Symbol>>(fexpr.result.value);
        l.size() == 1) {
      if (std::holds_alternative<std::string>(l.front().value)) {
        if (auto s = std::get<std::string>(l.front().value);
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
