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
#include "matchit.h"
#include "types.hpp"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <unistd.h>
#include <utility>
std::vector<std::map<std::string, Symbol>> variables;
// this is used to pass single-use environment variables to external programs.
// if this vector contains more than *number of elements in a pipe* or more than
// 1 for a single program call, it's most likely an error.
std::vector<std::map<std::string, std::string>> environment_variables;

// forward declaration so i can use it in the next function definition
std::optional<std::string>
get_absolute_path(std::string progn, const std::vector<std::string> &PATH);
std::map<std::string, Symbol> cmdline_args;
void get_env_vars(Symbol node, path PATH) {
  // called for the side effect of modifying the vector above.
  auto nodel = std::get<std::list<Symbol>>(node.value);
  auto _it = nodel.begin();
  auto lit = nodel.begin();
  lit = std::find_if(nodel.begin(), nodel.end(), [&](Symbol &s) -> bool {
    return std::holds_alternative<std::string>(s.value) &&
           get_absolute_path(std::get<std::string>(s.value), PATH) !=
               std::nullopt;
  });
  while (_it != lit) {
    if (_it->type == Type::List) {
      // either a key/value pair for an env var, or a user error.
      auto pairl = std::get<std::list<Symbol>>(_it->value);
      if (pairl.size() != 2) {
        throw std::logic_error{
            "Invalid key/value assignment for an environment variable!\n"
            "Correct syntax: '(<key> <value>)'.\n"};
      }
      auto fst = pairl.front();
      auto snd = pairl.back();
      if (!std::holds_alternative<std::string>(fst.value)) {
        throw std::logic_error{
            "Invalid value for a key in a key/value assignment for an env "
            "var!\n"
            "Expected a string.\n"};
      }
      std::string key = std::get<std::string>(fst.value);
      using namespace matchit;
      Id<std::string> s;
      Id<int> i;
      Id<bool> b;
      Id<std::list<Symbol>> l;
      std::string value = match(snd.value)(
          pattern | as<std::string>(s) = [&] { return *s; },
          pattern | as<int>(i) = [&] { return std::to_string(*i); },
          pattern | as<bool>(b) = [&] { return *b ? "true" : "false"; },
          pattern | as<std::list<Symbol>>(_) =
              [&] {
                throw std::logic_error{
                    "Unexpected list in key/value pair for an env var!\n"};
                return "";
              });
      if (environment_variables.empty()) {
        environment_variables.push_back(
            std::map<std::string, std::string>{{key, value}});
      } else {
        environment_variables[environment_variables.size() - 1]
            .insert_or_assign(key, value);
      }
    } else {
      throw std::logic_error{"Ill-formed external program call!\n"};
    }
    _it++;
  }
}

std::vector<std::pair<std::string, std::map<std::string, Symbol>>> call_stack;
std::vector<std::map<std::string, std::pair<Symbol, Symbol>>>
    user_defined_procedures;
Symbol rewind_pipe(Symbol node, const std::vector<std::string> &PATH);
Symbol rewind_call_ext_program(Symbol node,
                               const std::vector<std::string> &PATH,
                               bool must_pipe = false, int pipe_fd_out = 0,
                               int pipe_fd_in = 0);
Symbol rewind_redirect_append(Symbol node,
                              const std::vector<std::string> &PATH);
Symbol rewind_redirect_overwrite(Symbol node,
                                 const std::vector<std::string> &PATH);
Symbol eval(Symbol root, path PATH);
void rec_print_ast(Symbol root);
namespace fs = std::filesystem;
// just so the compiler doesn't complain about nonexistent PATH
// later in the 'procedures' map.

std::optional<std::string> get_absolute_path(std::string progn, path &PATH) {
  std::string full_path;
  auto it = PATH.begin();
  it = std::find_if(PATH.begin(), PATH.end(),
                    [&](const std::string &dir) -> bool {
                      std::string query = dir + "/" + progn;
                      return fs::directory_entry(query).exists();
                    });
  if (it != PATH.end()) {
    return std::string{(*it) + "/" + progn};
  }
  return std::nullopt;
}

std::optional<Symbol> variable_lookup(Symbol id) {
  if (!std::holds_alternative<std::string>(id.value))
    return std::nullopt;
  if (variables.empty() || !variables[variables.size() - 1].contains(
                               std::get<std::string>(id.value))) {
    return std::nullopt;
  }
  return std::optional<Symbol>{
      variables[variables.size() - 1][std::get<std::string>(id.value)]};
}

std::optional<std::pair<Symbol, Symbol>> procedure_lookup(Symbol id) {
  if (!std::holds_alternative<std::string>(id.value))
    return std::nullopt;
  if (user_defined_procedures.empty() ||
      !user_defined_procedures[user_defined_procedures.size() - 1].contains(
          std::get<std::string>(id.value))) {
    return std::nullopt;
  }
  return std::optional<std::pair<Symbol, Symbol>>{
      user_defined_procedures[user_defined_procedures.size() - 1]
                             [std::get<std::string>(id.value)]};
}

std::optional<Symbol> callstack_variable_lookup(Symbol id) {
  if (!std::holds_alternative<std::string>(id.value))
    return std::nullopt;
  if (call_stack.empty() || !call_stack[call_stack.size() - 1].second.contains(
                                std::get<std::string>(id.value))) {
    return std::nullopt;
  }
  return std::optional<Symbol>{call_stack[call_stack.size() - 1]
                                   .second[std::get<std::string>(id.value)]};
}

bool convert_value_to_bool(Symbol sym) {
  using namespace matchit;
#define p pattern
  auto is_strlit = [](const std::string &s) -> bool {
    return !s.empty() && s[0] == '"' && s[s.length() - 1] == '"';
  };
  using str = std::string;
  using lst = std::list<Symbol>;
  Id<str> s;
  Id<lst> l;
  Id<int> i;
  Id<bool> b;
  auto maybe_id = variable_lookup(sym);
  auto maybe_cs_id = callstack_variable_lookup(sym);
  bool clause;
  if (maybe_cs_id != std::nullopt) {
    clause = convert_value_to_bool(*maybe_cs_id);
  } else if (maybe_id != std::nullopt) {
    clause = convert_value_to_bool(*maybe_id);
  } else
    clause = match(sym.value)(
        p | as<int>(i) = [&] { return *i != 0; },
        p | as<str>(s) = [&] { return (is_strlit(*s) && ((*s).length() > 2)); },
        p | as<bool>(b) = [&] { return *b; },
        p | as<lst>(l) = [&] { return (!(*l).empty()); });
#undef p
  return clause;
}

std::map<std::string, Functor> procedures = {
    {"cd", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 1)
         throw std::logic_error{
             "The 'cd' builtin command expects precisely one argument!"};
       auto cur = fs::current_path();
       fs::current_path(std::string{(cur).c_str()} + "/" +
                        std::get<std::string>(args.front().value));
       return Symbol("", fs::current_path(), Type::Command);
     }}},
    {"set", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 2)
         throw std::logic_error{
             "The 'set' builtin command expects precisely two arguments!\n"};
       std::string var = std::get<std::string>(args.front().value);
       std::string val = std::get<std::string>(args.back().value);
       return Symbol("", setenv(var.c_str(), val.c_str(), 1), Type::Number);
     }}},
    {"get", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 1)
         throw std::logic_error{
             "The 'get' builtin command expects precisely one argument!"};
       char *s = std::getenv(std::get<std::string>(args.front().value).c_str());
       if (s)
         return Symbol("", std::string(s), Type::String);
       return Symbol("", "Nil", Type::String);
     }}},
    {"->", {[](std::list<Symbol> args, path PATH) -> Symbol {
       for (auto &e : args) {
         auto progl = std::get<std::list<Symbol>>(e.value);
         auto lit =
             std::find_if(progl.begin(), progl.end(), [&](Symbol &s) -> bool {
               return std::holds_alternative<std::string>(s.value) &&
                      get_absolute_path(std::get<std::string>(s.value), PATH) !=
                          std::nullopt;
             });
         std::string prog = std::get<std::string>(lit->value);
         auto rest = std::list<Symbol>(lit, progl.end());
         rest.pop_front();
         auto ext = *lit;
         get_env_vars(e, PATH);
         auto abs = get_absolute_path(prog, PATH);
         if (abs == std::nullopt) {
           throw std::logic_error{"Unknown executable " + prog + "!\n"};
         }
         std::string full_path;
         if (procedures.contains(prog))
           full_path = prog;
         else
           full_path = *abs;
         ext.value = full_path;
         rest.push_front(ext);
         e.value = rest;
       }
       std::reverse(environment_variables.begin(), environment_variables.end());
       Symbol node = Symbol("", args, Type::List);
       Symbol result = rewind_pipe(node, PATH);
       return Symbol("", result.value, result.type);
     }}},
    {">", {[](std::list<Symbol> args, path PATH) -> Symbol {
       if (args.size() != 2) {
         throw std::logic_error{
             "Expected exactly two arguments to the '>' operator!\n"};
       }
       if (args.front().type != Type::List) {
         throw std::logic_error{"Invalid first argument to the '>' operator!\n"
                                "Expected a command or an executable.\n"};
       }
       auto pipe_args = std::get<std::list<Symbol>>(args.front().value);
       pipe_args.pop_front();
       for (auto &e : pipe_args) {
         auto progl = std::get<std::list<Symbol>>(e.value);
         auto prog = std::get<std::string>(progl.front().value);
         progl.pop_front();
         auto abs = get_absolute_path(prog, PATH);
         if (abs == std::nullopt) {
           throw std::logic_error{"Unknown executable " + prog + "!\n"};
         }
         std::string full_path;
         if (procedures.contains(prog))
           full_path = prog;
         else
           full_path = *abs;
         progl.push_front(Symbol("", full_path, Type::Identifier));
         e.value = progl;
       }
       args.pop_front();
       if (args.front().type != Type::Identifier) {
         throw std::logic_error{"Invalid second argument to the '>' operator!\n"
                                "Expected a file name.\n"};
       }
       int out = open(std::get<std::string>(args.front().value).c_str(),
                      O_WRONLY | O_APPEND | O_CREAT, 0666);
       int stdoutcpy = dup(STDOUT_FILENO);
       dup2(out, STDOUT_FILENO);
       auto node = Symbol("", pipe_args, Type::List);
       Symbol status = rewind_pipe(node, PATH);
       close(out);
       dup2(stdoutcpy, STDOUT_FILENO);
       close(stdoutcpy);
       return status;
     }}},
    {"+", {[](std::list<Symbol> args) -> Symbol {
       int r = 0;
       for (auto e : args) {
         if (e.type == Type::Defunc)
           continue;
         if (e.type != Type::Number) {
           throw std::logic_error{"Unexpected operand to the '+' procedure!\n"};
         }
         r += std::get<int>(e.value);
       }
       Symbol ret("", r, Type::Number);
       return ret;
     }}},
    {"-", {[](std::list<Symbol> args) -> Symbol {
       int r;
       if (!std::holds_alternative<int>(args.front().value)) {
         throw std::logic_error{"Unexpected operand to the '-' procedure!\n"};
       }
       r = std::get<int>(args.front().value);
       args.pop_front();
       for (auto e : args) {
         if (e.type == Type::Defunc)
           continue;
         if (e.type != Type::Number) {
           throw std::logic_error{"Unexpected operand to the '-' procedure!\n"};
         }
         r -= std::get<int>(e.value);
       }
       Symbol ret("", r, Type::Number);
       return ret;
     }}},
    {"/", {[](std::list<Symbol> args) -> Symbol {
       int r;
       if (!std::holds_alternative<int>(args.front().value)) {
         throw std::logic_error{"Unexpected operand to the '/' procedure!\n"};
       }
       r = std::get<int>(args.front().value);
       args.pop_front();
       for (auto e : args) {
         if (e.type == Type::Defunc)
           continue;
         if (e.type != Type::Number) {
           throw std::logic_error{"Unexpected operand to the '/' procedure!\n"};
         }
         r /= std::get<int>(e.value);
       }
       Symbol ret("", r, Type::Number);
       return ret;
     }}},
    {"*", {[](std::list<Symbol> args) -> Symbol {
       int r;
       if (!std::holds_alternative<int>(args.front().value)) {
         throw std::logic_error{"Unexpected operand to the '*' procedure!\n"};
       }
       r = std::get<int>(args.front().value);
       args.pop_front();
       for (auto e : args) {
         if (e.type == Type::Defunc)
           continue;
         if (e.type != Type::Number) {
           throw std::logic_error{"Unexpected operand to the '*' procedure!\n"};
         }
         r *= std::get<int>(e.value);
       }
       Symbol ret("", r, Type::Number);
       return ret;
     }}},
    {"<", {[](std::list<Symbol> args) -> Symbol {
       bool is_true = true;
       if (args.empty())
         return Symbol("", true, Type::Boolean);
       auto first = args.front();
       args.pop_front();
       for (auto e : args) {
         if (!std::holds_alternative<int>(first.value) ||
             !std::holds_alternative<int>(e.value)) {
           throw std::logic_error{"The '<' operator only accepts integers!\n"};
         }
         is_true =
             is_true && (std::get<int>(first.value) < std::get<int>(e.value));
         first = e;
       }
       return Symbol("", is_true, Type::Boolean);
     }}},
    {"=", {[](std::list<Symbol> args, path PATH) -> Symbol {
       bool is_true = true;
       Symbol prev = args.front();
       args.pop_front();
       for (auto e : args) {
         if (e.type != prev.type) {
           throw std::logic_error{
               "Types of the arguments to '=' don't match!\n"};
         }
         Symbol lhs = eval(e, PATH);
         Symbol rhs = eval(prev, PATH);
         is_true = is_true && (lhs.value == rhs.value);
         prev = e;
       }
       return Symbol("", is_true, Type::Boolean);
     }}},
    {"s+", {[](std::list<Symbol> args) -> Symbol {
       const auto is_strlit = [](const std::string &s) -> bool {
         return (s.size() > 1) && (s[0] == '"') && (s[s.length() - 1] == '"');
       };
       std::string ret;
       for (auto e : args) {
         if (!std::holds_alternative<std::string>(e.value)) {
           throw std::logic_error{"Expected a string in 's+'!\n"};
         }
         if (is_strlit(std::get<std::string>(e.value))) {
           std::string no_strlit = std::get<std::string>(e.value);
           no_strlit = no_strlit.substr(1, no_strlit.length() - 2);
           ret += no_strlit;
         }
       }
       ret.insert(0, 1, '\"');
       ret.push_back('\"');
       return Symbol("", ret, Type::String);
     }}},
    {"toi", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 1) {
         throw std::logic_error{"'toi' expects precisely one string to try and "
                                "convert to an integer!\n"};
       }
       if (args.front().type != Type::String) {
         throw std::logic_error{"'toi' expects a string!\n"};
       }
       std::string s = std::get<std::string>(args.front().value);
       int n;
       auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.length(), n);
       if (ec != std::errc()) {
         throw std::logic_error{"Exception in 'toi': Failed to convert the "
                                "string to an integer!\n"};
       }
       return Symbol("", n, Type::Number);
     }}},
    {"let", {[](std::list<Symbol> args) -> Symbol {
       if (args.front().type != Type::Identifier)
         throw std::logic_error{
             "First argument to 'let' must be an identifier!\n"};
       if (args.size() > 2) {
         // function definition
         std::string name = std::get<std::string>(args.front().value);
         args.pop_front();
         Symbol arguments = args.front();
         args.pop_front();
         Symbol stmts = Symbol("", args, Type::List);
         if (user_defined_procedures.empty()) {
           user_defined_procedures.push_back(
               std::map<std::string, std::pair<Symbol, Symbol>>{
                   {name, std::make_pair(arguments, stmts)}});
         } else {
           user_defined_procedures[user_defined_procedures.size() - 1]
               .insert_or_assign(name, std::make_pair(arguments, stmts));
         }
         return Symbol("", true, Type::Defunc);
       }
       Symbol id = args.front();
       args.pop_front();
       if (variables.empty()) {
         variables.push_back(std::map<std::string, Symbol>());
       }
       variables[variables.size() - 1].insert_or_assign(
           std::get<std::string>(id.value), args.front());
       return Symbol("", args.front().value, args.front().type);
     }}},
    {"if", {[](std::list<Symbol> args, path PATH) -> Symbol {
       // (if <clause> (expr1 ... exprn) (else1 ... elsen))
       // if <clause> converts to Cpp's "true" then return
       // (expr1 ... exprn) to the caller, and the other
       // list otherwise
       if (args.size() != 3) {
         throw std::logic_error{
             "An if statement must have precisely three arguments!\n"};
       }
       Symbol clause_expr = eval(args.front(), PATH);
       bool clause = convert_value_to_bool(clause_expr);
       args.pop_front();
       if (clause) {
         return eval(args.front(), PATH);
       }
       args.pop_front();
       return eval(args.front(), PATH);
     }}},
    {"$", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 1) {
         throw std::logic_error{
             "the reference '$' operator expects a variable!\n"};
       }
       auto var = variable_lookup(args.front());
       auto cs = callstack_variable_lookup(args.front());
       if (var != std::nullopt) {
         return Symbol((*var).name, (*var).value, (*var).type);
       } else if (cs != std::nullopt) {
         return Symbol((*cs).name, (*cs).value, (*cs).type);
       }
       return (Symbol("", "", Type::String));
     }}},
    {"print", {[](std::list<Symbol> args, path PATH) -> Symbol {
       for (auto e : args) {
         if (e.type == Type::Defunc)
           continue;
         if (e.type == Type::List)
           e = eval(e, PATH);
         rec_print_ast(e);
       }
       return Symbol("", false, Type::Command);
     }}},
    {"strip", {[](std::list<Symbol> args) -> Symbol {
       const auto is_strlit = [](const std::string &s) -> bool {
         return (s.size() > 1) && (s[0] == '"') && (s[s.length() - 1] == '"');
       };
       if (args.size() != 1) {
         throw std::logic_error{"'strip' expects a single string to which "
                                "remove the trailing newline!\n"};
       }
       if (args.front().type != Type::String) {
         throw std::logic_error{"'strip' expects a string!\n"};
       }
       auto str = std::get<std::string>(args.front().value);
       if (str.empty()) {
         return Symbol("", "", Type::String);
       }
       if (is_strlit(str)) {
         if (str[str.size() - 2] == '\n') {
           str = str.substr(0, str.size() - 2);
           str.push_back('"');
         }
         return Symbol("", str, Type::String);
       }

       if (str.size() == 1) {
         if (str[0] == '\n') {
           return Symbol("", "", Type::String);
         } else
           return Symbol("", str, Type::String);
       }
       if (str[str.size() - 1] != '\n') {
         return Symbol(args.front().name, str, args.front().type);
       }
       str = str.substr(0, str.size() - 1);
       return Symbol(args.front().name, str, args.front().type);
     }}},
    {"nostr", {[](std::list<Symbol> args) -> Symbol {
       if ((args.size() != 1) ||
           !std::holds_alternative<std::string>(args.front().value)) {
         throw std::logic_error{
             "'nostr' expects a single string literal to try and convert to a "
             "bareword!\n"};
       }
       const auto is_strlit = [](const std::string &s) -> bool {
         return (s.size() > 1) && (s[0] == '"') && (s[s.length() - 1] == '"');
       };
       auto s = std::get<std::string>(args.front().value);
       if (is_strlit(std::get<std::string>(args.front().value))) {
         s = s.substr(1, s.length() - 2);
       }
       args.front().value = s;
       return args.front();
     }}},
    {"cmd", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 1) {
         throw std::logic_error{"'cmd' expects exactly one command argument "
                                "name (a number) to get the value of!\n"};
       }
       if (args.front().type != Type::Number) {
         throw std::logic_error{
             "The command line argument name must be a string!\n"};
       }
       if (cmdline_args.contains(std::to_string(std::get<int>(args.front().value)))) {
         return cmdline_args.at(std::to_string(std::get<int>(args.front().value)));
       }
       return Symbol("", "", Type::String);
     }}}};

std::array<std::string, 5> special_forms = {"->", "let", "if", ">", "$"};
