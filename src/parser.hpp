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
  QuestionOperator,
  QuestionOperatorFirstFunctionCall,
  QuestionOperatorInArgumentList,
  PipeOperator,
  PipeOperatorStart,
  ComposeOperator,
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
                    bool is_root, path PATH, int level, State st,
                    int list_balance = 0) {
  RecInfo res;
  std::list<Symbol> as_list;
  res.result = Symbol(is_root ? "root" : "", as_list, Type::List);
  State cur_state = st;
  static int lambda_id = 0;
  std::list<Symbol> maybe_cond{Symbol("", "cond", Type::Operator)};
  std::list<Symbol> maybe_match{};
  for (int i = si; i < ei; ++i) {
    auto cur = tokens[i];
    if ((cur == "(") || (cur == "[")) {
      RecInfo info = get_ast_aux(tokens, i + 1, ei, false, PATH, level + 1,
                                 ((cur_state == State::FirstFunctionCall) ||
                                  (cur_state == State::Identifier))
                                     ? State::InArgumentList
                                     : State::List,
                                 list_balance + 1);
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
    } else if (cur == ";") {
      res.result.value = as_list;
      if (as_list.size() == 1) {
        if ((as_list.back().type == Type::String) ||
            (as_list.back().type == Type::Number)) {
          res.result = as_list.back();
        } else if ((std::holds_alternative<std::string>(
                       as_list.back().value)) &&
                   std::get<std::string>(as_list.back().value)[0] == '@') {
          res.result =
              Symbol("", std::get<std::string>(as_list.back().value).substr(1),
                     Type::Identifier);
        }
      }
      if (level > 0) {
        return {.result = res.result, .end_index = i, .st = State::Semicolon};
      }
      return {.result = res.result, .end_index = i, .st = State::End};
    } else if (cur == ",") {
      res.result.value = as_list;
      if (as_list.size() == 1) {
        if ((as_list.back().type == Type::String) ||
            (as_list.back().type == Type::Number)) {
          res.result = as_list.back();
        } else if ((std::holds_alternative<std::string>(
                       as_list.back().value)) &&
                   std::get<std::string>(as_list.back().value)[0] == '@') {
          res.result =
              Symbol("", std::get<std::string>(as_list.back().value).substr(1),
                     Type::Identifier);
        }
      }
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
              .st = (level > 0)
                        ? ((cur_state == State::LambdaFunctionFirstFunctionCall)
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
    } else if ((cur == "false") || (cur == "true")) {
      as_list.push_back(Symbol(is_root ? "root" : "",
                               cur == "false" ? false : true, Type::Boolean));
      if (cur_state == State::LambdaFunctionFirstFunctionCall) {
        cur_state = State::LambdaFunctionLiteral;
      } else if (cur_state == State::LambdaFunctionIdentifier) {
        cur_state = State::LambdaFunctionFirstFunctionCall;
      }
    } else if (cur == "<<") {
      RecInfo info = get_ast_aux(tokens, i + 1, ei, false, PATH, level + 1,
                                 State::Identifier);
      i = info.end_index;
      as_list.push_back(info.result);
      res.result.value = as_list;
      return {.result = res.result, .end_index = i, .st = State::None};
    } else if (cur == "|") {
      int idx;
      for (idx = i + 1; idx < ei; ++idx) {
        if (tokens[idx] == "=>")
          break;
      }
      auto cond = get_ast_aux(tokens, i + 1, idx, false, PATH, level + 1,
                              State::FirstFunctionCall);
      auto l = std::list<Symbol>{};
      auto as_l = std::list<Symbol>{};
      if (cond.result.type == Type::List)
        as_l = std::get<std::list<Symbol>>(cond.result.value);
      else
        as_l.push_back(cond.result);
      if (as_l.size() == 1) {
        l.push_back(as_l.back());
      } else
        l.push_back(cond.result);
      int to_other_pipe;
      for (to_other_pipe = idx + 1; to_other_pipe < ei; ++to_other_pipe) {
        if (tokens[to_other_pipe] == "|") {
          break;
        }
      }
      auto branch = get_ast_aux(tokens, idx + 1, to_other_pipe, false, PATH,
                                level + 1, State::FirstFunctionCall);

      if (branch.result.type == Type::List)
        as_l = std::get<std::list<Symbol>>(branch.result.value);
      else
        as_l = {branch.result};
      if (as_l.size() == 1) {
        l.push_back(as_l.back());
      } else
        l.push_back(branch.result);
      while (branch.end_index < (to_other_pipe - 1)) {
        branch = get_ast_aux(tokens, branch.end_index + 1, to_other_pipe, false,
                             PATH, level + 1, State::FirstFunctionCall);
        if (branch.result.type == Type::List)
          as_l = std::get<std::list<Symbol>>(branch.result.value);
        else
          as_l = {branch.result};
        if (as_l.size() == 1) {
          l.push_back(as_l.back());
        } else
          l.push_back(branch.result);
      }
      if (to_other_pipe == ei) {
        if (cur_state == State::QuestionOperator) {
          if (l.size() == 1)
            maybe_match.push_back(l.back());
          else
            maybe_match.push_back(Symbol("", l, Type::List));
        } else
          maybe_cond.push_back(Symbol("", l, Type::List));
        if (cur_state == State::QuestionOperator) {
          if (maybe_match.size() == 1)
            as_list.push_back(maybe_match.back());
          else
            as_list.push_back(Symbol("", maybe_match, Type::List));
        } else
          as_list.push_back(Symbol("", maybe_cond, Type::List));
        res.result.value = as_list;
        return {.result = res.result, .end_index = ei, .st = State::None};
      }
      i = branch.end_index;
      if (cur_state != State::QuestionOperator)
        cur_state = State::PipeOperator;
      if (cur_state == State::QuestionOperator) {
        if (l.size() == 1)
          maybe_match.push_back(l.back());
        else
          maybe_match.push_back(Symbol("", l, Type::List));
      } else
        maybe_cond.push_back(Symbol("", l, Type::List));
    } else if (cur == "?") {
      int to_first_pipe;
      auto match_full_body =
          std::list<Symbol>{Symbol("", "match", Type::Operator)};
      for (to_first_pipe = i; to_first_pipe < ei; ++to_first_pipe) {
        if (tokens[to_first_pipe] == "|") {
          break;
        }
      }
      auto to_be_matched =
          get_ast_aux(tokens, i + 1, to_first_pipe, false, PATH, level + 1,
                      State::FirstFunctionCall);
      std::list<Symbol> as_l;
      std::list<Symbol> l =
          std::get<std::list<Symbol>>(to_be_matched.result.value);
      if (l.size() == 1) {
        if ((l.back().type == Type::Number) ||
            (l.back().type == Type::String)) {
          as_l.push_back(l.back());
        } else if ((std::holds_alternative<std::string>(l.back().value)) &&
                   (std::get<std::string>(l.back().value)[0] == '@')) {
          as_l.push_back(Symbol("",
                                std::get<std::string>(l.back().value).substr(1),
                                Type::Identifier));
        } else
          as_l.push_back(l.back());
      } else
        as_l = {to_be_matched.result};
      if (as_l.size() == 1) {
        match_full_body.push_back(as_l.back());
      } else
        match_full_body.push_back(to_be_matched.result);
      RecInfo info = get_ast_aux(tokens, to_first_pipe, ei, false, PATH,
                                 level + 1, State::QuestionOperator);
      as_l = std::get<std::list<Symbol>>(info.result.value);
      if (as_l.size() == 1) {
        for (auto e : std::get<std::list<Symbol>>(as_l.back().value)) {
          // this is a dirty hack i am NOT proud of, but it will do
          // for now until i fix this for good
          match_full_body.push_back(e);
        }
      } else
        match_full_body.push_back(info.result);
      as_list.push_back(Symbol("", match_full_body, Type::List));
      i = info.end_index;
    } else if (cur == "=>") {
      if (as_list.empty() || as_list.back().type != Type::List) {
        throw std::logic_error{"Parser error: Found a lambda token ('=>') with "
                               "no parameter list before it!\n"};
      }
      std::string id = "__re_lambda" + std::to_string(lambda_id);
      RecInfo info = get_ast_aux(tokens, i + 1, ei, false, PATH, level + 1,
                                 State::LambdaFunctionFirstFunctionCall);
      auto l = std::list<Symbol>();
      Symbol m;
      if (std::holds_alternative<std::list<Symbol>>(info.result.value)) {
        if (auto l = std::get<std::list<Symbol>>(info.result.value);
            l.size() == 1) {
          if ((l.back().type == Type::String) ||
              (l.back().type == Type::Number) ||
              (l.back().type == Type::List)) {
            m = l.back();
          } else if ((std::holds_alternative<std::string>(l.back().value)) &&
                     std::get<std::string>(l.back().value)[0] == '@') {
            m = Symbol("", std::get<std::string>(l.back().value).substr(1),
                       Type::Identifier);
          } else
            m = Symbol("", l, Type::List);
        } else
          m = info.result;
      } else
        m = info.result;
      l.push_back(m);
      Symbol body = Symbol("", l, Type::List);
      // keep iterating until we finish the statements composing the lambda
      // function
      while ((info.st == State::LambdaFunctionInArgumentList) ||
             (info.st == State::LambdaFunctionFirstFunctionCall)) {
        i = info.end_index;
        info = get_ast_aux(tokens, i + 1, ei, false, PATH, level + 1,
                           State::LambdaFunctionFirstFunctionCall);
        if (std::holds_alternative<std::list<Symbol>>(info.result.value)) {
          if (auto l = std::get<std::list<Symbol>>(info.result.value);
              l.size() == 1) {
            if ((l.back().type == Type::String) ||
                (l.back().type == Type::Number) ||
                (l.back().type == Type::List)) {
              m = l.back();
            } else if ((std::holds_alternative<std::string>(l.back().value)) &&
                       std::get<std::string>(l.back().value)[0] == '@') {
              m = Symbol("", std::get<std::string>(l.back().value).substr(1),
                         Type::Identifier);
            } else
              m = Symbol("", l, Type::List);
          } else
            m = info.result;
        } else
          m = info.result;
        l.push_back(m);
      }
      body.value = l;
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
      i = info.end_index;
      auto body = info.result;
      std::list<Symbol> l;
      Symbol m;
      if (std::holds_alternative<std::list<Symbol>>(info.result.value)) {
        if (auto l = std::get<std::list<Symbol>>(info.result.value);
            l.size() == 1) {
          if ((l.back().type == Type::String) ||
              (l.back().type == Type::Number) ||
              (l.back().type == Type::List)) {
            m = l.back();
          } else if ((std::holds_alternative<std::string>(l.back().value)) &&
                     std::get<std::string>(l.back().value)[0] == '@') {
            m = Symbol("", std::get<std::string>(l.back().value).substr(1),
                       Type::Identifier);
          } else
            m = Symbol("", l, Type::List);
        } else
          m = Symbol("", l, Type::List);
      } else
        m = info.result;
      l.push_back(m);
      info = get_ast_aux(tokens, i + 1, ei, false, PATH, level + 1,
                         State::LambdaFunctionFirstFunctionCall);
      i = info.end_index;
      if (std::holds_alternative<std::list<Symbol>>(info.result.value)) {
        if (auto l = std::get<std::list<Symbol>>(info.result.value);
            l.size() == 1) {
          if ((l.back().type == Type::String) ||
              (l.back().type == Type::Number) ||
              (l.back().type == Type::List)) {
            m = l.back();
          } else if ((std::holds_alternative<std::string>(l.back().value)) &&
                     std::get<std::string>(l.back().value)[0] == '@') {
            m = Symbol("", std::get<std::string>(l.back().value).substr(1),
                       Type::Identifier);
          } else
            m = Symbol("", l, Type::List);
        } else
          m = Symbol("", l, Type::List);
      } else
        m = info.result;
      l.push_back(m);
      body.value = l;
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
    } else {
      // identifiers are complicated, but we can somehow get around it
      // by having a fuckton of states to keep track of where we are
      if ((cur[0] != '@') && (list_balance == 0) &&
          ((cur_state == State::FirstFunctionCall) ||
           (cur_state == State::None) ||
           (cur_state == State::LambdaFunctionFirstFunctionCall))) {
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
        auto l = std::list<Symbol>();
        if (info.result.type == Type::List) {
          l = std::get<std::list<Symbol>>(info.result.value);
        } else
          l.push_back(info.result);
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
          return {.result = res.result, .end_index = i, .st = cur_state};
        }
      } else {
        if (cur[0] == '@')
          cur = cur.substr(1);
        as_list.push_back(Symbol("", cur,
                                 procedures.contains(cur) ? Type::Operator
                                                          : Type::Identifier));
      }
      if (cur_state == State::Identifier) {
        cur_state = State::FirstFunctionCall;
      }
      if (cur_state == State::PipeOperator) {
        cur_state = State::FirstFunctionCall;
      }
    }
    res.result.value = as_list;
  }
  res.result.value = as_list;
  if (as_list.size() == 1) {
    if ((as_list.back().type == Type::String) ||
        (as_list.back().type == Type::Number)) {
      res.result = as_list.back();
    } else if ((std::holds_alternative<std::string>(as_list.back().value)) &&
               std::get<std::string>(as_list.back().value)[0] == '@') {
      res.result =
          Symbol("", std::get<std::string>(as_list.back().value).substr(1),
                 Type::Identifier);
    }
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
              res += ((root.type == Type::String)
                          ? "(Str) "
                          : (root.type == Type::Operator ? "(Op) " : "(Id) ")) +
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
