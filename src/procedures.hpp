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
#include "types.hpp"
#include "parser.hpp"
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/wait.h>
#include <termios.h>
#include <type_traits>
#include <unistd.h>
#include <utility>
#include <variant>
struct PidSym {
  Symbol s;
  pid_t pid;
};


std::string rec_print_ast(Symbol root, bool debug);
RecInfo parse(std::vector<Token> tokens, int i);
std::string rewind_read_file(std::string filename);
std::vector<std::pair<int, std::string>> rewind_split_file(std::string content);
variables constants;
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

template <class K, class V>
std::map<K, V> combine(std::map<K, V> x, std::map<K, V> y) {
  x.merge(y);
  return x;
}

namespace fs = std::filesystem;
std::optional<std::string>
get_absolute_path(std::string progn, const path &PATH) {
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
      std::string value = std::visit(overloaded{
	[](std::string s) { return s; },
	[](unsigned long long int x) { return std::to_string(x); },
	[](signed long long int x) { return std::to_string(x); },
	[](bool b) -> std::string { return b ? "true" : "false"; },
	[](std::list<Symbol> l) -> std::string { return ""; },
	[](std::monostate) -> std::string { return ""; }
      }, snd.value);
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
Symbol rewind_pipe(Symbol node, const std::vector<std::string> &PATH,
                   bool must_read);
PidSym rewind_call_ext_program(Symbol node,
                               const std::vector<std::string> &PATH,
                               bool must_pipe = false, int pipe_fd_out = 0,
                               int pipe_fd_in = 0);
Symbol rewind_redirect_append(Symbol node,
                              const std::vector<std::string> &PATH);
Symbol rewind_redirect_overwrite(Symbol node,
                                 const std::vector<std::string> &PATH);

Symbol eval(Symbol root, const path &PATH, variables& vars = constants,
            int line = 0);


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

std::array<std::string, 10> special_forms = {
    "->", "let", "if", "$", "cond", "match", "<<<", "defined", "and", "or"};
