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
#include "src/shell/shell.hpp"
#include "types.hpp"
#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <variant>
void rec_print_ast(Symbol root);
Symbol get_ast(std::vector<std::string> tokens);
std::vector<std::string> get_tokens(std::string stream);
std::string rewind_read_file(std::string filename);
std::vector<std::string> rewind_split_file(std::string content);
std::vector<std::map<std::string, Symbol>> variables;
std::vector<int> active_pids;
// this is used to pass single-use environment variables to external programs.
// if this vector contains more than *number of elements in a pipe* or more than
// 1 for a single program call, it's most likely an error.
std::vector<std::map<std::string, std::string>> environment_variables;

// These two functions are used to get an integer, or 0, from a variant,
// depending on if the variant contains an integer (see the concept in
// types.hpp) or another type. The return value of the second function is not a
// problem, since we're only gonna call them when we are certain we're gonna get
// an integer anyway.
using ints = std::variant<long long int, long long unsigned int>;

ints get_int(_Type n) {
  return std::visit(
      []<class T>(T t) -> ints {
        if constexpr (std::is_same_v<T, long long unsigned int> ||
                      std::is_same_v<T, long long signed int>)
          return t;
        return 0;
      },
      n);
}

static const Symbol match_any = Symbol("", "_", Type::Identifier);
static const Symbol match_eq = Symbol("", "=", Type::Operator);
static const Symbol match_neq = Symbol("", "!=", Type::Operator);
static const Symbol match_in_list = Symbol("", "in", Type::Operator);

static Symbol match_less_than =
    Symbol("",
           std::list<Symbol>{Symbol("", "<", Type::Operator),
                             Symbol("", "x", Type::Identifier)},
           Type::List);
static const Symbol match_less_than_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", "a", Type::Identifier),
                             Symbol("", "<", Type::Operator),
                             Symbol("", "b", Type::Identifier)},
           Type::List);
bool compare_list_structure(Symbol l, Symbol r) {
  if (l.type != Type::List)
    throw std::logic_error{"First operand to 'compare_list_structure' "
                           "(internal function) is not a list!\n"};
  if (r.type != Type::List)
    throw std::logic_error{"Second operand to 'compare_list_structure' "
                           "(internal function) is not a list!\n"};
  auto ll = std::get<std::list<Symbol>>(l.value);
  auto rl = std::get<std::list<Symbol>>(r.value);
  if (ll.size() != rl.size())
    return false;
  if (ll.empty())
    return true;
  // Zip the two lists together and then iterate on the result
  std::list<std::pair<Symbol, Symbol>> zipped;
  std::transform(
      ll.begin(), ll.end(), rl.begin(), std::back_inserter(zipped),
      [](const Symbol &lle, const Symbol &rle) -> std::pair<Symbol, Symbol> {
        return std::make_pair(lle, rle);
      });
  bool is_structure_equal = true;
  for (auto p : zipped) {
    if (p.first.type == Type::List) {
      if (p.second.type == Type::List) {
        is_structure_equal =
            is_structure_equal && compare_list_structure(p.first, p.second);
      } else
        return false;
    }
    if (p.second.type == Type::List) {
      return false;
    }
  }
  return is_structure_equal;
}

bool weak_compare(Symbol fst, Symbol other) {
  if (fst == other)
    return true;
  if (fst.type == Type::Identifier)
    return true;
  if (other.type == Type::Identifier)
    return true;
  if (fst.type == other.type) {
    if (fst.type == Type::List) {
      bool is_same_pattern = true;
      auto ll = std::get<std::list<Symbol>>(fst.value);
      auto otherl = std::get<std::list<Symbol>>(other.value);
      if (ll.size() != otherl.size()) return false;
      std::list<std::pair<Symbol, Symbol>> zipped;
      std::transform(
          ll.begin(), ll.end(), otherl.begin(), std::back_inserter(zipped),
          [](const Symbol &l, const Symbol &r) -> std::pair<Symbol, Symbol> {
            return std::make_pair(l, r);
          });
      for (auto [l, r] : zipped) {
        is_same_pattern = is_same_pattern &&
                          ((l.type == r.type) || (l.type == Type::Identifier) ||
                           (r.type == Type::Identifier));
      }
      return is_same_pattern;
    } else
      return true;
  }
  return false;
}

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
    bool is_local_executable =
        std::holds_alternative<std::string>(s.value) &&
        (std::get<std::string>(s.value).substr(0, 2) == "./");
    bool is_in_path =
        std::holds_alternative<std::string>(s.value) &&
        get_absolute_path(std::get<std::string>(s.value), PATH) != std::nullopt;
    return is_local_executable || is_in_path;
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
      Id<long long int> i;
      Id<long long unsigned int> u;
      Id<bool> b;
      Id<std::list<Symbol>> l;
      std::string value = match(snd.value)(
          pattern | as<std::string>(s) = [&] { return *s; },
          pattern | as<long long int>(i) = [&] { return std::to_string(*i); },
          pattern | as<long long unsigned int>(u) =
              [&] { return std::to_string(*u); },
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
  Id<long long int> i;
  Id<long long unsigned int> u;
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
        p | as<long long int>(i) = [&] { return *i != 0; },
        p | as<long long unsigned int>(u) = [&] { return *i != 0; },
        p | as<str>(s) = [&] { return (is_strlit(*s) && ((*s).length() > 2)); },
        p | as<bool>(b) = [&] { return *b; },
        p | as<lst>(l) = [&] { return (!(*l).empty()); });
#undef p
  return clause;
}

std::map<std::string, Functor> procedures = {
    {"cd", {[](std::list<Symbol> args) -> Symbol {
       if ((args.size() != 1) || ((args.front().type != Type::Identifier) &&
                                  (args.front().type != Type::String)))
         throw std::logic_error{
             "The 'cd' builtin command expects precisely one path!"};
       auto cur = fs::current_path();
       if (cur.is_absolute()) {
         fs::current_path(
             std::string{std::get<std::string>(args.front().value).c_str()});
       } else if (fs::directory_entry(std::string{cur.c_str()} + "/" +
                                      std::get<std::string>(args.front().value))
                      .exists()) {
         fs::current_path(std::string{cur.c_str()} + "/" +
                          std::get<std::string>(args.front().value));
       } else {
         throw std::logic_error{"Unknown path!\n"};
       }
       setenv("PWD", std::string{fs::current_path()}.c_str(), 1);
       return Symbol("", fs::current_path(), Type::Command);
     }}},
    {"set", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 2)
         throw std::logic_error{"The 'set' builtin command expects "
                                "precisely two arguments!\n"};
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
       if (args.front().type != Type::String) {
         throw std::logic_error{"Invalid first argument to the '>' operator!\n"
                                "Expected a string!.\n"};
       }
       auto contents = std::get<std::string>(args.front().value);

       args.pop_front();
       if (args.front().type != Type::Identifier) {
         throw std::logic_error{"Invalid second argument to the '>' operator!\n"
                                "Expected a file name.\n"};
       }
       int out = open(std::get<std::string>(args.front().value).c_str(),
                      O_WRONLY | O_APPEND | O_CREAT, 0666);
       Symbol status = Symbol("", 0, Type::Number);
       int stdoutcpy = dup(STDOUT_FILENO);
       status.value = dup2(out, STDOUT_FILENO);
       if (std::get<long long signed int>(status.value) != STDOUT_FILENO)
         return status;
       std::cout << contents;
       close(out);
       status.value = dup2(stdoutcpy, STDOUT_FILENO);
       if (std::get<long long signed int>(status.value) != STDOUT_FILENO)
         return status;
       close(stdoutcpy);
       status.type = Type::Command;
       return status;
     }}},
    {">>", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 2) {
         throw std::logic_error{
             "Expected exactly two arguments to the '>>' operator!\n"};
       }
       if (args.front().type != Type::String) {
         throw std::logic_error{"Invalid first argument to the '>>' operator!\n"
                                "Expected a string!.\n"};
       }
       auto contents = std::get<std::string>(args.front().value);

       args.pop_front();
       if (args.front().type != Type::Identifier) {
         throw std::logic_error{"Invalid second argument to the '>' operator!\n"
                                "Expected a file name.\n"};
       }
       int out = open(std::get<std::string>(args.front().value).c_str(),
                      O_WRONLY | O_CREAT, 0666);
       Symbol status = Symbol("", 0, Type::Number);
       int stdoutcpy = dup(STDOUT_FILENO);
       status.value = dup2(out, STDOUT_FILENO);
       if (std::get<long long signed int>(status.value) != STDOUT_FILENO)
         return status;
       std::cout << contents;
       close(out);
       status.value = dup2(stdoutcpy, STDOUT_FILENO);
       if (std::get<long long signed int>(status.value) != STDOUT_FILENO)
         return status;
       close(stdoutcpy);
       status.type = Type::Command;
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
         if (std::holds_alternative<long long int>(e.value))
           r += std::get<long long int>(e.value);
         else
           r += std::get<long long unsigned int>(e.value);
       }
       Symbol ret("", r, Type::Number);
       return ret;
     }}},
    {"-", {[](std::list<Symbol> args) -> Symbol {
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
    {"/", {[](std::list<Symbol> args) -> Symbol {
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
    {"*", {[](std::list<Symbol> args) -> Symbol {
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
    {"<", {[](std::list<Symbol> args) -> Symbol {
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
         is_true = is_true && (get_int(first.value) < get_int(e.value));
         first = e;
       }
       return Symbol("", is_true, Type::Boolean);
     }}},
    {"=", {[](std::list<Symbol> args, path PATH) -> Symbol {
       bool is_true = true;
       Symbol prev = args.front();
       args.pop_front();
       Symbol lhs;
       for (auto e : args) {
         if (e.type != prev.type) {
           throw std::logic_error{
               "Types of the arguments to '=' don't match!\n"};
         }
         is_true = is_true && (e.value == prev.value);
         prev = e;
       }
       return Symbol("", is_true, Type::Boolean);
     }}},
    {"!=", {[](std::list<Symbol> args, path PATH) -> Symbol {
       bool is_true = true;
       Symbol prev = args.front();
       args.pop_front();
       Symbol lhs;
       for (auto e : args) {
         if (e.type != prev.type) {
           throw std::logic_error{
               "Types of the arguments to '=' don't match!\n"};
         }
         if (e.type == Type::List)
           lhs = eval(e, PATH);
         else
           lhs = e;
         is_true = is_true && (lhs.value != prev.value);
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
           continue;
         }
         ret += std::get<std::string>(e.value);
       }
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
       long long unsigned int n = 0;
       auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.length(), n);
       if (ec != std::errc()) {
         throw std::logic_error{"Exception in 'toi': Failed to convert the "
                                "string to an integer!\n"};
       }
       return Symbol("", n, Type::Number);
     }}},
    {"stol", {[](std::list<Symbol> args) -> Symbol {
       const auto is_strlit = [](const std::string &s) -> bool {
         return (s.size() > 1) && (s[0] == '"') && (s[s.size() - 1] == '"');
       };

       if ((args.size() > 1) || (args.front().type != Type::String)) {
         throw std::logic_error{
             "'stol' expects a single string to turn into a list!\n"};
       }
       std::string s = std::get<std::string>(args.front().value);
       if (is_strlit(s)) {
         s = s.substr(1, s.size() - 2);
       }
       std::list<Symbol> l;
       for (auto ch : s) {
         l.push_back(Symbol("", std::string{ch}, Type::String));
       }
       return Symbol("", l, Type::List);
     }}},
    {"let", {[](std::list<Symbol> args, path PATH) -> Symbol {
       if (args.front().type != Type::Identifier) {
         if (args.front().type != Type::List) {
           throw std::logic_error{
               "First argument to 'let' must be an identifier!\n"};
         }
         static int lambda_id = 0;
         std::string name = "__re_lambda_" + std::to_string(lambda_id);
         lambda_id++;
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
         // return the name of the lambda. this is used to assign the name
         // of the "anonymous" (user-side) function to something that can
         // be called by the interpreter when the user calls a parameter
         // that represents a lambda. i really hope this makes sense to you
         // as it does to me
         return Symbol("", name, Type::Identifier);
       }
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

       Symbol result = eval(args.front(), PATH);
       if (variables.empty()) {
         variables.push_back(std::map<std::string, Symbol>());
       }
       variables[variables.size() - 1].insert_or_assign(
           std::get<std::string>(id.value), result);
       return result;
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
    {"cond", {[](std::list<Symbol> args, path PATH) -> Symbol {
       // the arguments are a sequence of lists of the form:
       // (<clause> <consequent>). Only no <clause>s can match, or
       // one does, and if this happens, the corresponding <consequent>
       // gets executed. The 'else' clause is a catch-all, and its
       // <consequent> is always executed.
       Symbol result;
       for (auto e : args) {
         if (e.type != Type::List) {
           throw std::logic_error{"Wrong expression in 'cond'! Expected "
                                  "[<clause> <consequent>].\n"};
         }
         if (std::get<std::list<Symbol>>(e.value).size() < 2) {
           throw std::logic_error{"Wrong expression in 'cond'! Expected "
                                  "[<clause> <consequent>].\n"};
         }
         std::list<Symbol> branch = std::get<std::list<Symbol>>(e.value);
         Symbol clause = branch.front();
         branch.pop_front();
         Symbol clause_bool = eval(clause, PATH);
         if (convert_value_to_bool(clause_bool)) {
           for (auto e : branch) {
             result = eval(e, PATH);
           }
           return result;
         }
       }
       return Symbol("", std::list<Symbol>(), Type::List);
     }}},
    {"match", {[](std::list<Symbol> args, path PATH) -> Symbol {
       if (args.size() < 2) {
         throw std::logic_error{
             "The 'match' procedure expects at least two arguments!\n"};
       }
       Symbol element = eval(args.front(), PATH);
       args.pop_front();
       static std::array<Symbol, 5> valid_patterns;
       if (element.type == Type::Number) {
         valid_patterns = {
             match_any,
             Symbol("",
                    std::list<Symbol>{match_less_than,
                                      Symbol("", "x", Type::Identifier)},
                    Type::List),
             Symbol(
                 "",
                 std::list<Symbol>{match_in_list,
                                   Symbol("", std::list<Symbol>{}, Type::List)},
                 Type::List),
             Symbol(
                 "",
                 std::list<Symbol>{match_eq, Symbol("", "x", Type::Identifier)},
                 Type::List),
             Symbol("",
                    std::list<Symbol>{match_neq,
                                      Symbol("", "x", Type::Identifier)},
                    Type::List)};
       }
       for (auto branch : args) {
         if (branch.type != Type::List) {
           throw std::logic_error{"Invalid argument to 'match' after the "
                                  "first value. Expected a list!\n"};
         }
         auto branchl = std::get<std::list<Symbol>>(branch.value);
         if (branchl.empty()) {
           throw std::logic_error{"Invalid empty branch in 'match'!\n"};
         }
         Symbol pattern = branchl.front();
         branchl.pop_front();
         if (match_any == pattern) {
           Symbol result;
           for (auto expr : branchl) {
             result = eval(expr, PATH);
           }
           return result;
         } else if (weak_compare(match_less_than, pattern)) {
           Symbol value = std::get<std::list<Symbol>>(pattern.value).back();
           if (element.type != Type::Number) {
             throw std::logic_error{"Type mismatch in a 'match' block! "
                                    "Expected an integer in a condition!\n"};
           }
           if (value.type != Type::Number) {
             throw std::logic_error{"Type mismatch in a 'match' block! "
                                    "Expected an integer in a pattern!\n"};
           }
           if (get_int(element.value) < get_int(value.value)) {
             Symbol result;
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
         } else if (weak_compare(match_less_than_capture, pattern)) {
           std::list<Symbol> expr = std::get<std::list<Symbol>>(pattern.value);
           Symbol value = expr.back();
           expr.pop_back();
           Symbol id = expr.back();
           variables.push_back(std::map<std::string, Symbol>{});
           variables[variables.size() - 1].insert_or_assign(
               std::get<std::string>(id.value), element);
           if (get_int(element.value) < get_int(value.value)) {
             Symbol result;
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
         }
       }
       return Symbol("", false, Type::Boolean);
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
    {"cmd", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 1) {
         throw std::logic_error{"'cmd' expects exactly one command argument "
                                "name (a number) to get the value of!\n"};
       }
       if (args.front().type != Type::Number) {
         throw std::logic_error{
             "The command line argument name must be an integer!\n"};
       }
       if (std::holds_alternative<unsigned long long int>(args.front().value)) {
         if (cmdline_args.contains(std::to_string(
                 std::get<unsigned long long int>(args.front().value)))) {
           return cmdline_args.at(std::to_string(
               std::get<unsigned long long int>(args.front().value)));
         }
       } else {
         if (cmdline_args.contains(
                 std::to_string(std::get<long long int>(args.front().value)))) {
           return cmdline_args.at(
               std::to_string(std::get<long long int>(args.front().value)));
         }
       }
       return Symbol("", "", Type::String);
     }}},
    {"hd", {[](std::list<Symbol> args) -> Symbol {
       if (args.front().type != Type::Number) {
         throw std::logic_error{
             "The first argument to 'hd' must be a number!\n"};
       }
       long long int n;
       if (std::holds_alternative<long long unsigned int>(args.front().value))
         n = std::get<long long unsigned int>(args.front().value);
       else
         n = std::get<long long int>(args.front().value);
       args.pop_front();
       if (args.front().type != Type::List) {
         throw std::logic_error{
             "The second argument to 'hd' must be a list!\n"};
       }
       auto lst = std::get<std::list<Symbol>>(args.front().value);
       if (n > lst.size()) {
         return Symbol("", lst, Type::List);
       }
       auto ret = std::list<Symbol>();
       for (int i = 0; i < n; ++i) {
         ret.push_back(lst.front());
         lst.pop_front();
       }
       return Symbol("", ret, Type::List);
     }}},
    {"tl", {[](std::list<Symbol> args) -> Symbol {
       if (args.front().type != Type::Number) {
         throw std::logic_error{
             "The first argument to 'tl' must be a number!\n"};
       }
       long long int n;
       if (std::holds_alternative<long long unsigned int>(args.front().value))
         n = std::get<long long unsigned int>(args.front().value);
       else
         n = std::get<long long int>(args.front().value);
       args.pop_front();
       if (args.front().type != Type::List) {
         throw std::logic_error{
             "The second argument to 'tl' must be a list!\n"};
       }
       auto lst = std::get<std::list<Symbol>>(args.front().value);
       std::reverse(lst.begin(), lst.end());
       if (n > lst.size()) {
         return Symbol("", lst, Type::List);
       }
       auto ret = std::list<Symbol>{};
       for (int i = 0; i < n; ++i) {
         ret.push_back(lst.front());
         lst.pop_front();
       }
       Symbol rets = Symbol("", ret, Type::List);
       return rets;
     }}},
    {"first", {[](std::list<Symbol> args) -> Symbol {
       if (args.empty()) {
         return Symbol("", std::list<Symbol>{}, Type::List);
       }
       if ((args.front().type != Type::List) || (args.size() > 1)) {
         throw std::logic_error{
             "'first' expects a list of which to return the first element!\n"};
       }
       auto lst = std::get<std::list<Symbol>>(args.front().value);
       if (lst.empty()) {
         return Symbol("", lst, Type::List);
       }
       return lst.front();
     }}},
    {"length", {[](std::list<Symbol> args) -> Symbol {
       if (args.empty()) {
         return Symbol("", 0, Type::Number);
       }
       if ((args.front().type != Type::List) || (args.size() > 1)) {
         throw std::logic_error{
             "'first' expects a list of which to return the first element!\n"};
       }
       auto lst = std::get<std::list<Symbol>>(args.front().value);
       if (lst.empty()) {
         return Symbol("", 0, Type::Number);
       }
       return Symbol("", static_cast<long long unsigned int>(lst.size()),
                     Type::Number);
     }}},
    {"++", {[](std::list<Symbol> args) -> Symbol {
       std::list<Symbol> l;
       for (auto e : args) {
         if (e.type == Type::List) {
           for (auto x : std::get<std::list<Symbol>>(e.value)) {
             l.push_back(x);
           }
         } else {
           l.push_back(e);
         }
       }
       return Symbol("", l, Type::List);
     }}},
    {"load", {[](std::list<Symbol> args, path PATH) -> Symbol {
       Symbol last_evaluated;
       for (auto e : args) {
         if ((e.type != Type::Identifier) && (e.type != Type::String)) {
           throw std::logic_error{"Arguments to 'load' must be either string "
                                  "literals or barewords!"};
         }
         std::string filename = std::get<std::string>(e.value);
         std::vector<std::string> expr_list =
             rewind_split_file(rewind_read_file(filename));
         for (auto expr : expr_list) {
           try {
             Symbol ast = get_ast(get_tokens(expr));
             last_evaluated = eval(ast, PATH);
           } catch (std::logic_error e) {
             std::cout << "Rewind: Exception in included file " << filename
                       << ": " << e.what() << "\n";
           }
         }
       }
       return last_evaluated;
     }}}};

std::array<std::string, 6> special_forms = {"->", "let",  "if",
                                            "$",  "cond", "match"};
