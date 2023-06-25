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
std::string rec_print_ast(Symbol root, bool debug = false);
Symbol get_ast(std::vector<std::string> tokens, path PATH);
std::vector<std::string> get_tokens(std::string stream);
std::string rewind_read_file(std::string filename);
std::vector<std::pair<int, std::string>> rewind_split_file(std::string content);
std::vector<std::map<std::string, Symbol>> variables;
std::vector<int> active_pids;
// this is used to pass single-use environment variables to external programs.
// if this vector contains more than *number of elements in a pipe* or more than
// 1 for a single program call, it's most likely an error.
std::vector<std::map<std::string, std::string>> environment_variables;
termios original;  // this will contain the "cooked" terminal mode
termios immediate; // raw terminal mode
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
                      std::is_same_v<T, long long signed int>) {
          return t;
        }
        return 0;
      },
      n);
}

static const Symbol match_any = Symbol("", "_", Type::Identifier);
static const Symbol match_eq =
    Symbol("",
           std::list<Symbol>{Symbol("", "=", Type::Operator),
                             Symbol("", "x", Type::Identifier)},
           Type::List);

static const Symbol match_neq =
    Symbol("",
           std::list<Symbol>{Symbol("", "!=", Type::Operator),
                             Symbol("", "x", Type::Identifier)},
           Type::List);

static const Symbol match_in_list =
    Symbol("",
           std::list<Symbol>{Symbol("", "in", Type::Operator),
                             Symbol("", "l", Type::List)},
           Type::List);

static Symbol match_less_than =
    Symbol("",
           std::list<Symbol>{Symbol("", "<", Type::Operator),
                             Symbol("", "x", Type::Number)},
           Type::List);
static const Symbol match_less_than_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", "<", Type::Operator),
                             Symbol("", "a", Type::Number),
                             Symbol("", "b", Type::Number)},
           Type::List);

static const Symbol match_greater_than =
    Symbol("",
           std::list<Symbol>{Symbol("", ">", Type::Operator),
                             Symbol("", "b", Type::Number)},
           Type::List);

static const Symbol match_greater_than_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", ">", Type::Operator),
                             Symbol("", "a", Type::Number),
                             Symbol("", "b", Type::Number)},
           Type::List);

static const Symbol match_eq_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", "=", Type::Operator),
                             Symbol("", "a", Type::Identifier),
                             Symbol("", "x", Type::Identifier)},
           Type::List);

static const Symbol match_neq_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", "!=", Type::Operator),
                             Symbol("", "a", Type::Identifier),
                             Symbol("", "x", Type::Identifier)},
           Type::List);

static const Symbol match_in_list_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", "in", Type::Operator),
                             Symbol("", "a", Type::Identifier),
                             Symbol("", "l", Type::List)},
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
    if (std::holds_alternative<std::string>(p.first.value) &&
        std::get<std::string>(p.first.value) == ".") {
      continue;
    }
    if (std::holds_alternative<std::string>(p.second.value) &&
        std::get<std::string>(p.second.value) == ".") {
      continue;
    }
    if (p.first.type == Type::List) {
      if (p.second.type == Type::List) {
        is_structure_equal =
            is_structure_equal && compare_list_structure(p.first, p.second);
      } else
        return false;
    } else if (p.second.type == Type::List) {
      return false;
    }
  }
  return is_structure_equal;
}

std::list<std::pair<Symbol, Symbol>> rec_bind_list(Symbol lhs, Symbol rhs) {
  std::list<std::pair<Symbol, Symbol>> ret;
  auto fst = std::get<std::list<Symbol>>(lhs.value);
  auto snd = std::get<std::list<Symbol>>(rhs.value);
  std::list<std::pair<Symbol, Symbol>> zipped;
  std::transform(fst.begin(), fst.end(), snd.begin(),
                 std::back_inserter(zipped),
                 [](Symbol l, Symbol r) -> std::pair<Symbol, Symbol> {
                   return std::make_pair(l, r);
                 });
  for (auto p : zipped) {
    if (p.first.type != Type::List) {
      ret.push_back(p);
    } else {
      auto rec = rec_bind_list(p.first, p.second);
      ret.insert(ret.end(), rec.begin(), rec.end());
    }
  }
  return ret;
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
      if (ll.size() != otherl.size())
        return false;
      std::list<std::pair<Symbol, Symbol>> zipped;
      std::transform(
          ll.begin(), ll.end(), otherl.begin(), std::back_inserter(zipped),
          [](const Symbol l, const Symbol r) -> std::pair<Symbol, Symbol> {
            return std::make_pair(l, r);
          });
      for (auto [l, r] : zipped) {
        // special case for the 'in' token, which the parser understands as an
        // identifier, but we want to consider it as an operator.
        if ((l.type == Type::Identifier) && (r.type == Type::Identifier))
          continue;

        if (l.type == Type::Operator) {
          is_same_pattern = is_same_pattern && (l.value == r.value);
        } else if (r.type == Type::Operator) {
          is_same_pattern = is_same_pattern && (l.value == r.value);
        } else if (r.type == Type::Identifier) {
          is_same_pattern = is_same_pattern && (l.type != Type::List);
        } else if (l.type == Type::Identifier) {
          is_same_pattern = is_same_pattern && (r.type != Type::List);
        } else
          is_same_pattern = is_same_pattern && (l.type == r.type);
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
namespace fs = std::filesystem;
// just so the compiler doesn't complain about nonexistent PATH
// later in the 'procedures' map.
Symbol eval(Symbol root, const std::vector<std::string> &PATH, int line = 0);
;
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
  if (variables.empty()) {
    return std::nullopt;
  }
  for (auto stk : std::vector<std::map<std::string, Symbol>>(
           variables.rbegin(), variables.rend())) {
    if (stk.contains(std::get<std::string>(id.value))) {
      return std::optional<Symbol>(stk[std::get<std::string>(id.value)]);
    }
  }
  return std::nullopt;
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
  if (call_stack.empty()) return std::nullopt;
  if (!call_stack[call_stack.size() - 1].second.contains(
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
    return (s.size() > 1) && s[0] == '"' && s[s.length() - 1] == '"';
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
        p | as<str>(s) = [&] { return !(*s).empty(); },
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
       bool must_read = std::get<bool>(args.front().value);
       args.pop_front();
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
       Symbol result = rewind_pipe(node, PATH, must_read);
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
       std::ofstream out(std::get<std::string>(args.front().value));
       auto backup = std::cout.rdbuf();
       std::cout.rdbuf(out.rdbuf());
       std::cout << contents;
       std::cout.rdbuf(backup);
       return Symbol("", true, Type::Command);
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
       std::ofstream out(std::get<std::string>(args.front().value),
                         std::ios::app);
       auto backup = std::cout.rdbuf();
       std::cout.rdbuf(out.rdbuf());
       std::cout << contents;
       std::cout.rdbuf(backup);
       return Symbol("", true, Type::Command);
     }}},
    {"<<<", {[](std::list<Symbol> args, path PATH) -> Symbol {
       // manually pipe, write the string (args[1]) and execute
       // the program/pipe (args[0]):
       int fd[2];     // parent write() -> child read()
       int readfd[2]; // child write () -> parent read()
       auto ext = std::get<std::list<Symbol>>(args.front().value);
       args.pop_front();
       Symbol s;
       if (args.front().type != Type::String) {
         s = eval(args.front(), PATH);
       } else
         s = args.front();
       std::string str = std::get<std::string>(s.value);
       // parse the program call stored in 'ext'
       if (ext.front() == Symbol("", "->", Type::Operator)) {
         // pipe
         ext.pop_front();
         for (auto &e : ext) {
           auto progl = std::get<std::list<Symbol>>(e.value);
           auto lit =
               std::find_if(progl.begin(), progl.end(), [&](Symbol &s) -> bool {
                 return std::holds_alternative<std::string>(s.value) &&
                        get_absolute_path(std::get<std::string>(s.value),
                                          PATH) != std::nullopt;
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
         std::reverse(environment_variables.begin(),
                      environment_variables.end());
         pipe(fd);
         pipe(readfd);
         write(fd[1], str.c_str(), str.size());
         if (ext.empty()) {
           throw std::logic_error{
               "Exception in '<<<' (here-string): Invalid pipe!\n"};
         }
         Symbol first = ext.front();
         ext.pop_front();
         if (ext.empty()) {
           close(fd[1]);
           PidSym status =
               rewind_call_ext_program(first, PATH, true, readfd[1], fd[0]);
           while (waitpid(status.pid, nullptr, 0) != -1)
             ;
           close(readfd[1]);
           char buf[1024];
           std::string result;
           int cnt = 0;

           while ((cnt = read(readfd[0], buf, 1023))) {
             if (cnt == -1) {
               throw std::logic_error{"failed read in '<<<'!\n"};
             }
             if (cnt == 0) {
               break;
             }
             buf[cnt] = '\0';
             std::string tmp{buf};
             result += tmp;
           }
           close(readfd[0]);
           close(fd[0]);
           while (wait(nullptr) != -1)
             ;
           return Symbol("", result, Type::String);
         }
         close(fd[1]);
         PidSym status =
             rewind_call_ext_program(first, PATH, true, readfd[1], fd[0]);
         Symbol last = ext.back();
         ext.pop_back();
         int old_read_end;
         for (Symbol cmd : ext) {
           old_read_end = dup(readfd[0]);
           pipe(readfd);
           status = rewind_call_ext_program(cmd, PATH, true, readfd[1],
                                            old_read_end);
         }
         old_read_end = dup(readfd[0]);
         pipe(readfd);
         status =
             rewind_call_ext_program(last, PATH, true, readfd[1], old_read_end);
         close(readfd[1]);
         close(old_read_end);
         std::string result;
         int cnt;
         char buf[1024];
         while ((cnt = read(readfd[0], buf, 1023))) {
           buf[cnt] = '\0';
           if (cnt == 0) {
             break;
           }
           std::string tmp{buf};
           result += tmp;
         }
         while (wait(nullptr) != -1)
           ;
         close(fd[0]);
         return Symbol("", result, Type::String);
       }
       // not a pipe
       pipe(fd);
       pipe(readfd);
       write(fd[1], str.c_str(), str.size());
       auto lit = std::find_if(ext.begin(), ext.end(), [&](Symbol &s) -> bool {
         bool is_local_executable =
             std::holds_alternative<std::string>(s.value) &&
             (std::get<std::string>(s.value).substr(0, 2) == "./");
         bool is_in_path = std::holds_alternative<std::string>(s.value) &&
                           get_absolute_path(std::get<std::string>(s.value),
                                             PATH) != std::nullopt;
         return is_local_executable || is_in_path;
       });
       if (lit == ext.end()) {
         throw std::logic_error{"Unknown executable!\n"};
       }
       get_env_vars(Symbol("", ext, Type::List), PATH);
       auto rest = std::list<Symbol>(lit, ext.end());
       close(fd[1]);
       rest.pop_front();
       Symbol prog = Symbol(
           "", *get_absolute_path(std::get<std::string>(lit->value), PATH),
           Type::Identifier);
       rest.push_front(prog);
       PidSym status = rewind_call_ext_program(Symbol("", rest, Type::List),
                                               PATH, true, readfd[1], fd[0]);
       while (waitpid(status.pid, nullptr, 0) != -1)
         ;
       close(readfd[1]);
       std::string result;
       int cnt;
       char buf[1024];
       while ((cnt = read(readfd[0], buf, 1023))) {
         if (cnt == -1) {
           throw std::logic_error{"Read failed in '<<<' (here-string!)\n"};
         }
         if (cnt == 0) {
           break;
         }
         buf[cnt] = '\0';
         std::string tmp{buf};
         result += tmp;
       }
       close(readfd[0]);
       close(fd[0]);
       while (wait(nullptr) != -1)
         ;
       return Symbol("", result, Type::String);
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
    {"%", {[](std::list<Symbol> args) -> Symbol {
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
    {"=", {[](std::list<Symbol> args, path PATH) -> Symbol {
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
    {"!=", {[](std::list<Symbol> args, path PATH) -> Symbol {
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
         if (e.type == Type::List)
           lhs = eval(e, PATH);
         else
           lhs = e;
         is_true = is_true && (lhs.value != prev.value);
         prev = e;
       }
       return Symbol("", is_true, Type::Boolean);
     }}},
    {"and", {[](std::list<Symbol> args, path PATH) -> Symbol {
       bool is_true = true;
       Symbol clause;
       for (auto e : args) {
         clause = eval(e, PATH);
         if (clause.type != Type::Boolean) {
           throw std::logic_error{"Type mismatch in the 'and' operator: Only "
                                  "booleans are allowed!\n"};
         }
         is_true = is_true && std::get<bool>(clause.value);
         if (!is_true) {
           return Symbol("", false, Type::Boolean);
         }
       }
       return Symbol("", is_true, Type::Boolean);
     }}},
    {"or", {[](std::list<Symbol> args, path PATH) -> Symbol {
       bool is_true = false;
       Symbol clause;
       for (auto e : args) {
         clause = eval(e, PATH);
         if (clause.type != Type::Boolean) {
           throw std::logic_error{"Type mismatch in the 'and' operator: Only "
                                  "booleans are allowed!\n"};
         }
         is_true = is_true || std::get<bool>(clause.value);
         if (is_true) {
           return Symbol("", true, Type::Boolean);
         }
       }
       return Symbol("", is_true, Type::Boolean);
     }}},
    {"not", {[](std::list<Symbol> args) -> Symbol {
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
    {"tos", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 1) {
         throw std::logic_error{
             "Exception in 'tos': This function accepts only one argument!\n"};
       }
       return Symbol("", rec_print_ast(args.front()), Type::String);
     }}},
    {"chtoi", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 1) {
         throw std::logic_error{
             "Exception in 'chtoi': The function expects a single character!"};
       }
       if ((args.front().type != Type::Identifier) &&
           (args.front().type != Type::String)) {
         throw std::logic_error{
             "Exception in 'chtoi': The function expects a single character!"};
       }
       std::string s = std::get<std::string>(args.front().value);
       if (s.size() != 1) {
         throw std::logic_error{
             "Exception in 'chtoi': The function expects a single character!"};
       }
       return Symbol("", static_cast<long long signed int>(s[0]), Type::Number);
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
    {"ltos", {[](std::list<Symbol> args) -> Symbol {
       if ((args.size() != 1) ||
           (!std::holds_alternative<std::list<Symbol>>(args.front().value))) {
         throw std::logic_error{
             "'ltos' expects a list of strings to concatenate!\n"};
       }
       auto l = std::get<std::list<Symbol>>(args.front().value);
       std::string ret;
       for (auto e : l) {
         if ((e.type != Type::Identifier) && (e.type != Type::String) &&
             (e.type != Type::Operator)) {
           throw std::logic_error{
               "Exception in 'ltos'! Found a non-string list element.\n"};
         }
         ret += std::get<std::string>(e.value);
       }
       return Symbol("", ret, Type::String);
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
       return Symbol("", true, Type::Command);
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
         return args.front();
       }
       args.pop_front();
       return args.front();
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
         Symbol last = branch.back();
         branch.pop_back();
         Symbol clause_bool = eval(clause, PATH);
         if (convert_value_to_bool(clause_bool)) {
           for (auto e : branch) {
             result = eval(e, PATH);
           }
           return last;
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
           variables.push_back(std::map<std::string, Symbol>{});
           variables[variables.size() - 1].insert_or_assign("_", element);
           for (auto expr : branchl) {
             result = eval(expr, PATH);
           }
	   variables.pop_back();
           return result;
         } else if (weak_compare(match_less_than, pattern)) {
           Symbol value = std::get<std::list<Symbol>>(pattern.value).back();
           if (element.type != Type::Number) {
             throw std::logic_error{"Type mismatch in a 'match' block! "
                                    "Expected an integer in a condition!\n"};
           }
           if (value.type != Type::Number) {
             if (value.type == Type::List) {
               value = eval(value, PATH);
             } else
               throw std::logic_error{"Type mismatch in a 'match' block! "
                                      "Expected an integer in a pattern!\n"};
           }
           if (std::visit([]<class T, class U>(
                              T t, U u) -> bool { return std::cmp_less(t, u); },
                          get_int(element.value), get_int(value.value))) {
             Symbol result;
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
         } else if (weak_compare(match_less_than_capture, pattern)) {
           std::list<Symbol> expr = std::get<std::list<Symbol>>(pattern.value);
           Symbol value = expr.back();
           if (element.type != Type::Number) {
             throw std::logic_error{"Type mismatch in a 'match' block! "
                                    "Expected an integer in a condition!\n"};
           }
           if (value.type != Type::Number) {
             if (value.type == Type::List) {
               value = eval(value, PATH);
             } else
               throw std::logic_error{"Type mismatch in a 'match' block! "
                                      "Expected an integer in a pattern!\n"};
           }
           expr.pop_back();
           Symbol id = expr.back();
           variables.push_back(std::map<std::string, Symbol>{});
           variables[variables.size() - 1].insert_or_assign(
               std::get<std::string>(id.value), element);
           if (std::visit([]<class T, class U>(
                              T t, U u) -> bool { return std::cmp_less(t, u); },
                          get_int(element.value), get_int(value.value))) {
             Symbol result;
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
           variables.pop_back();
         } else if (weak_compare(match_eq, pattern)) {
           Symbol value = std::get<std::list<Symbol>>(pattern.value).back();
           bool is_true = false;
           if ((element.type != Type::Number) && (value.type != Type::Number)) {
             is_true = element.value == value.value;
           } else if (element.type == value.type) {
             is_true = std::visit(
                 []<class T, class U>(T t, U u) -> bool {
                   return std::cmp_equal(t, u);
                 },
                 get_int(element.value), get_int(value.value));
           }

           if (is_true) {
             Symbol result;
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
         } else if (weak_compare(match_eq_capture, pattern)) {
           auto expr = std::get<std::list<Symbol>>(pattern.value);
           auto value = expr.back();
           bool is_true = false;
           if ((element.type != Type::Number) && (value.type != Type::Number)) {
             is_true = element.value == value.value;
           } else if (element.type == value.type) {
             is_true = std::visit(
                 []<class T, class U>(T t, U u) -> bool {
                   return std::cmp_equal(t, u);
                 },
                 get_int(element.value), get_int(value.value));
           }

           expr.pop_back();
           auto id = expr.back();
           variables.push_back(std::map<std::string, Symbol>{});
           variables[variables.size() - 1].insert_or_assign(
               std::get<std::string>(id.value), element);
           if (is_true) {
             Symbol result;
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
           variables.pop_back();
         } else if (weak_compare(match_neq, pattern)) {
           Symbol value = std::get<std::list<Symbol>>(pattern.value).back();
           bool is_true = false;
           if ((element.type != Type::Number) || (value.type != Type::Number)) {
             is_true = element.value != value.value;
           } else if (element.type == value.type) {
             is_true = std::visit(
                 []<class T, class U>(T t, U u) -> bool {
                   return std::cmp_not_equal(t, u);
                 },
                 get_int(element.value), get_int(value.value));
           }

           if (is_true) {
             Symbol result;
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
         }

         else if (weak_compare(match_in_list, pattern)) {
           if (element.type == Type::List) {
             throw std::logic_error{
                 "The pattern matching 'in' operator doesn't accept lists as "
                 "a matched value!\n"};
           }
           auto expr = std::get<std::list<Symbol>>(pattern.value);
           auto value = expr.back();
           if (value.type != Type::List) {
             value = eval(value, PATH);
             if (value.type != Type::List)
               throw std::logic_error{"The second operand to the match 'in' "
                                      "operator must be a list!\n"};
           }
           auto l = std::get<std::list<Symbol>>(value.value);
           if (std::find_if(l.begin(), l.end(), [&](Symbol s) -> bool {
                 return s == element;
               }) != l.end()) {
             Symbol result;
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
         } else if (weak_compare(match_in_list_capture, pattern)) {
           if (element.type == Type::List) {
             throw std::logic_error{
                 "The pattern matching 'in' operator doesn't accept lists as "
                 "a matched value!\n"};
           }
           auto expr = std::get<std::list<Symbol>>(pattern.value);
           auto value = expr.back();
           if (value.type != Type::List) {
             value = eval(value, PATH);
             if (value.type != Type::List)
               throw std::logic_error{"The second operand to the match 'in' "
                                      "operator must be a list!\n"};
           }
           expr.pop_back();
           auto id = expr.back();
           variables.push_back(std::map<std::string, Symbol>{});
           variables[variables.size() - 1].insert_or_assign(
               std::get<std::string>(id.value), element);
           auto l = std::get<std::list<Symbol>>(value.value);
           if (std::find_if(l.begin(), l.end(), [&](const Symbol s) -> bool {
                 return s == element;
               }) != l.end()) {
             Symbol result;
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
           variables.pop_back();
         } else if (weak_compare(match_greater_than, pattern)) {
           auto expr = std::get<std::list<Symbol>>(pattern.value);
           auto value = expr.back();
           if (element.type != Type::Number) {
             throw std::logic_error{"Type mismatch in a 'match' block! "
                                    "Expected an integer in a condition!\n"};
           }
           if (value.type != Type::Number) {
             if (value.type == Type::List) {
               value = eval(value, PATH);
             } else
               throw std::logic_error{"Type mismatch in a 'match' block! "
                                      "Expected an integer in a pattern!\n"};
           }
           if (std::visit([]<class T, class U>(T t, U u)
                              -> bool { return std::cmp_greater(t, u); },
                          get_int(element.value), get_int(value.value))) {
             Symbol result;
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
         } else if (weak_compare(match_greater_than_capture, pattern)) {
           auto expr = std::get<std::list<Symbol>>(pattern.value);
           auto value = expr.back();
           if (element.type != Type::Number) {
             throw std::logic_error{"Type mismatch in a 'match' block! "
                                    "Expected an integer in a condition!\n"};
           }
           if (value.type != Type::Number) {
             if (value.type == Type::List) {
               value = eval(value, PATH);
             } else
               throw std::logic_error{"Type mismatch in a 'match' block! "
                                      "Expected an integer in a pattern!\n"};
           }
           if (std::visit([]<class T, class U>(T t, U u)
                              -> bool { return std::cmp_greater(t, u); },
                          get_int(element.value), get_int(value.value))) {
             Symbol result;
             std::string id;
             expr.pop_back();
             id = std::get<std::string>(expr.back().value);
             variables.push_back(std::map<std::string, Symbol>{});
             variables[variables.size() - 1].insert_or_assign(id, element);
             for (auto expr : branchl) {
               result = eval(expr, PATH);
             }
             return result;
           }
           variables.pop_back();
         } else if (pattern.type == Type::List) {
           if (!std::holds_alternative<std::list<Symbol>>(element.value)) {
             continue;
           }
           bool same_structure = compare_list_structure(pattern, element);
           if (same_structure) {
             auto vars = rec_bind_list(pattern, element);
             variables.push_back({});
             Symbol result;
             for (auto p : vars) {
               variables[variables.size() - 1].insert_or_assign(
                   std::get<std::string>(p.first.value), p.second);
             }
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
    {"defined", {[](std::list<Symbol> args, path PATH) -> Symbol {
       if (args.size() != 1) {
         throw std::logic_error{"the 'defined' boolean procedure expects "
                                "exactly one name to look up!\n"};
       }
       Symbol maybe_eval;
       if ((args.front().type != Type::String) &&
           (args.front().type != Type::Identifier)) {
         if (args.front().type == Type::List) {
           maybe_eval = eval(args.front(), PATH);
         } else
           throw std::logic_error{"the 'defined' boolean procedure expects an "
                                  "identifier or a string literal!\n"};
       }
       std::string name;
       if (std::holds_alternative<std::string>(maybe_eval.value)) {
         name = std::get<std::string>(maybe_eval.value);
       } else
         name = std::get<std::string>(args.front().value);
       if (!user_defined_procedures.empty() &&
           user_defined_procedures[user_defined_procedures.size() - 1].contains(
               name)) {
         return Symbol("", true, Type::Boolean);
       }
       if (variable_lookup(Symbol("", name, Type::Identifier)) !=
           std::nullopt) {
         return Symbol("", true, Type::Boolean);
       }
       return Symbol("", false, Type::Boolean);
     }}},
    {"print", {[](std::list<Symbol> args, path PATH) -> Symbol {
       for (auto e : args) {
         if (e.type == Type::Defunc)
           continue;
         std::cout << rec_print_ast(e);
       }
       return Symbol("", false, Type::Command);
     }}},
    {"flush", {[](std::list<Symbol> args) -> Symbol {
       std::flush(std::cout);
       return Symbol("", false, Type::Command);
     }}},
    {"read", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() > 1) {
         throw std::logic_error{
             "The 'read' utility expects zero or more identifiers!\n"};
       }
       signed long long int s = 0;
       unsigned long long int us = 0;
       Symbol ret;
       std::string in;
       std::getline(std::cin, in);
       if ((in == "true") || (in == "false")) {
         ret.type = Type::Boolean;
         ret.value = (in == "true") ? true : false;
         ret.name = "";
       } else if (auto [ptr, ec] =
                      std::from_chars(in.data(), in.data() + in.size(), s);
                  ec == std::errc()) {
         ret.value = s;
         ret.type = Type::Number;
         ret.name = "";
       } else if (auto [ptr, ec] =
                      std::from_chars(in.data(), in.data() + in.size(), us);
                  ec == std::errc()) {
         ret.value = us;
         ret.type = Type::Number;
         ret.name = "";
       } else {
         ret.type = Type::String;
         ret.value = in;
         ret.name = "";
       }
       if (args.empty()) {
         return ret;
       }
       if (variables.empty()) {
         throw std::logic_error{"Exception in 'read'! I tried to read into a "
                                "variable while having no defined names.\n"};
       }
       if (!variables[variables.size() - 1].contains(in)) {
         throw std::logic_error{"Exception in 'read'! I tried to read into a "
                                "variable that doesn't exist.\n"};
       }
       variables[variables.size() - 1].insert_or_assign(in, ret);
       return ret;
     }}},
    {"rawmode", {[](std::list<Symbol> args) -> Symbol {
       tcsetattr(STDIN_FILENO, TCSANOW, &immediate);
       return Symbol("", false, Type::Command);
     }}},
    {"cookedmode", {[](std::list<Symbol> args) -> Symbol {
       tcsetattr(STDIN_FILENO, TCSANOW, &original);
       return Symbol("", false, Type::Command);
     }}},
    {"readch", {[](std::list<Symbol> args) -> Symbol {
       // read the character
       int ch;
       int tmp[1];
       read(STDIN_FILENO, tmp, 1);
       ch = tmp[0];
       // restore the normal terminal mode
       auto ret = Symbol("", std::string{static_cast<char>(ch)}, Type::String);
       return ret;
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
       if (!std::holds_alternative<std::list<Symbol>>(args.front().value)) {
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
       return Symbol("", ret, args.front().type);
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
       if (!std::holds_alternative<std::list<Symbol>>(args.front().value)) {
         throw std::logic_error{
             "The second argument to 'tl' must be a list!\n"};
       }
       auto lst = std::get<std::list<Symbol>>(args.front().value);
       std::reverse(lst.begin(), lst.end());
       if (n > lst.size()) {
         return Symbol("", lst, args.front().type);
       }
       auto ret = std::list<Symbol>{};
       for (int i = 0; i < n; ++i) {
         ret.push_back(lst.front());
         lst.pop_front();
       }
       Symbol rets = Symbol("", ret, args.front().type);
       return rets;
     }}},
    {"first", {[](std::list<Symbol> args) -> Symbol {
       if (args.empty()) {
         return Symbol("", std::list<Symbol>{}, Type::List);
       }
       if ((!std::holds_alternative<std::list<Symbol>>(args.front().value)) ||
           (args.size() > 1)) {
         throw std::logic_error{"'first' expects a list of which to return "
                                "the first element!\n"};
       }
       auto lst = std::get<std::list<Symbol>>(args.front().value);
       if (lst.empty()) {
         return Symbol("", lst, Type::List);
       }
       return lst.front();
     }}},
    {"rest", {[](std::list<Symbol> args) -> Symbol {
       int n = 0;
       if (args.empty())
         throw std::logic_error{
             "'rest' expects only two arguments (a list, which is obligatory, "
             "and an integer, optionally)!\n"};
       if (args.size() > 2)
         throw std::logic_error{
             "'rest' expects only two arguments (a list, which is obligatory, "
             "and an integer, optionally)!\n"};

       auto sym = args.front();
       args.pop_front();
       if (!std::holds_alternative<std::list<Symbol>>(sym.value)) {
         throw std::logic_error{"'rest' expects a single list of which to "
                                "return all elements but the first!\n"};
       }
       auto l = std::get<std::list<Symbol>>(sym.value);
       if (args.size() == 1) {
         if (args.front().type != Type::Number) {
           throw std::logic_error{
               "The optional second argument to 'rest' must be an integer!\n"};
         }
         n = std::visit([](auto num) -> int { return num; },
                        get_int(args.front().value));
         for (int i = 0; i < n; ++i)
           l.pop_front();
         return Symbol("", l, sym.type);
       }
       l.pop_front();
       return Symbol("", l, sym.type);
     }}},
    {"delete", {[](std::list<Symbol> args) -> Symbol {
       if ((args.size() != 2) ||
           (!std::holds_alternative<std::list<Symbol>>(args.front().value)) ||
           (args.back().type != Type::Number)) {
         throw std::logic_error{"Exception: The 'delete' procedure accepts a "
                                "list and an index!\n"};
       }
       auto l = std::get<std::list<Symbol>>(args.front().value);
       auto new_t = args.front().type;
       args.pop_front();
       long long signed int sidx = 0;
       long long unsigned int uidx = 0;
       if (std::holds_alternative<long long signed int>(args.front().value)) {
         sidx = std::get<long long signed int>(args.front().value);
         long long signed int i = 0;
         for (std::list<Symbol>::iterator it = l.begin(); it != l.end(); ++it) {
           if (i == sidx) {
             l.erase(it);
             break;
           }
           i++;
         }
         return Symbol("", l, Type::List);
       } else {
         uidx = std::get<long long unsigned int>(args.front().value);
         long long unsigned int i = 0;
         for (std::list<Symbol>::iterator it = l.begin(); it != l.end(); ++it) {
           if (i == uidx) {
             l.erase(it);
             break;
           }
           i++;
         }
         return Symbol("", l, new_t);
       }
     }}},
    {"insert", {[](std::list<Symbol> args) -> Symbol {
       if ((args.size() != 3) ||
           (!std::holds_alternative<std::list<Symbol>>(args.front().value)) ||
           (args.back().type != Type::Number)) {
         throw std::logic_error{"Exception: The 'insert' procedure accepts a "
                                "list, an element, and an index!\n"};
       }
       auto l = std::get<std::list<Symbol>>(args.front().value);
       auto new_t = args.front().type;
       args.pop_front();
       long long signed int sidx = 0;
       long long unsigned int uidx = 0;
       if (std::holds_alternative<long long signed int>(args.back().value)) {
         sidx = std::get<long long signed int>(args.back().value);
         long long signed int i = 0;
         if (sidx >= l.size()) {
           l.insert(l.end(), args.front());
           return Symbol("", l, Type::List);
         }
         for (std::list<Symbol>::iterator it = l.begin(); it != l.end(); ++it) {
           if (i == sidx) {
             l.insert(it, args.front());
             break;
           }
           i++;
         }
         return Symbol("", l, new_t);
       } else {
         uidx = std::get<long long unsigned int>(args.back().value);
         long long unsigned int i = 0;
         if (uidx >= l.size()) {
           l.insert(l.end(), args.front());
           return Symbol("", l, Type::List);
         }
         for (std::list<Symbol>::iterator it = l.begin(); it != l.end(); ++it) {
           if (i == uidx) {
             l.insert(it, args.front());
             break;
           }
           i++;
         }
         return Symbol("", l, Type::List);
       }
     }}},
    {"reverse", {[](std::list<Symbol> args) -> Symbol {
       if ((args.size() != 1) ||
           (!std::holds_alternative<std::list<Symbol>>(args.front().value))) {
         throw std::logic_error{
             "Exception: The 'reverse' procedure only accepts a list!\n"};
       }
       auto l = std::get<std::list<Symbol>>(args.front().value);
       std::reverse(l.begin(), l.end());
       return Symbol("", l, args.front().type);
     }}},
    {"length", {[](std::list<Symbol> args) -> Symbol {
       if (args.empty()) {
         return Symbol("", 0, Type::Number);
       }
       if ((!std::holds_alternative<std::list<Symbol>>(args.front().value)) ||
           (args.size() > 1)) {
         throw std::logic_error{"'length' expects a list of which to return "
                                "the length!\n"};
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
       bool must_ret_ast = false;
       for (auto e : args) {
         if (e.type == Type::List)
           for (auto x : std::get<std::list<Symbol>>(e.value)) {
             l.push_back(x);
           }
         else {
           if (e.type == Type::RawAst) {
             must_ret_ast = true;
           }
           l.push_back(e);
         }
       }
       return Symbol("", l, (must_ret_ast) ? Type::RawAst : Type::List);
     }}},
    {"load", {[](std::list<Symbol> args, path PATH) -> Symbol {
       Symbol last_evaluated;
       for (auto e : args) {
         if ((e.type != Type::Identifier) && (e.type != Type::String)) {
           throw std::logic_error{"Arguments to 'load' must be either string "
                                  "literals or barewords!"};
         }
         std::string filename = std::get<std::string>(e.value);
         std::vector<std::pair<int, std::string>> expr_list =
             rewind_split_file(rewind_read_file(filename));
         for (auto expr : expr_list) {
           try {
             Symbol ast = get_ast(get_tokens(expr.second), PATH);
             last_evaluated = eval(ast, PATH);
           } catch (std::logic_error e) {
             std::cout << "Rewind: Exception in included file " << filename
                       << "at line " << expr.first << ": " << e.what() << "\n";
           }
         }
       }
       return last_evaluated;
     }}},
    {"eval", {[](std::list<Symbol> args, path PATH) -> Symbol {
       if ((args.size() != 1) || (args.front().type != Type::String)) {
         throw std::logic_error{
             "'eval' expects exactly one string to evaluate!\n"};
       }
       Symbol last_evaluated;
       std::string line = std::get<std::string>(args.front().value);
       if ((line == "exit") || (line == "(exit)")) {
         exit(EXIT_SUCCESS);
         return Symbol("", false, Type::Command);
       }
       try {
         Symbol ast = get_ast(get_tokens(line), PATH);
         last_evaluated = eval(ast, PATH);
         if ((last_evaluated.type != Type::Command) &&
             (last_evaluated.type != Type::CommandResult))
           rec_print_ast(last_evaluated);
       } catch (std::logic_error ex) {
         return Symbol("", ex.what(), Type::Error);
       }
       return last_evaluated;
     }}},
    {"typeof", {[](std::list<Symbol> args, path PATH) -> Symbol {
       if (args.size() != 1) {
         throw std::logic_error{
             "The 'typeof' procedure expects exactly one symbol!\n"};
       }

       Symbol sym;
       if (args.front().type == Type::RawAst) {
         sym = get_ast(get_tokens(rec_print_ast(args.front())), PATH);
       } else
         sym = args.front();
       switch (sym.type) {
       case Type::List:
         return Symbol("", "list", Type::String);
         break;
       case Type::Number:
         return Symbol("", "number", Type::String);
         break;
       case Type::Identifier:
         return Symbol("", "identifier", Type::String);
         break;
       case Type::String:
         return Symbol("", "string", Type::String);
         break;
       case Type::Boolean:
         return Symbol("", "boolean", Type::String);
         break;
       case Type::Operator:
         return Symbol("", "operator", Type::String);
         break;
       case Type::Error:
         return Symbol("", "error", Type::String);
       default:
         return Symbol("", "undefined", Type::String);
       }
     }}},
    {"ast", {[](std::list<Symbol> args, path PATH) -> Symbol {
       // what this function  does is, it takes a single string as input,
       // and it generates an ast representation of it to be used in coupling
       // with the "typeof" operator or otherwise for parsing purposes.
       std::string input;
       if (args.size() != 1) {
         throw std::logic_error{
             "Exception: The 'ast' procedure expects a single string!\n"};
       }
       if (args.front().type != Type::String) {
         throw std::logic_error{
             "Exception: The 'ast' procedure expects a single string!\n"};
       }
       input = std::get<std::string>(args.front().value);
       try {
         auto ast = get_ast(get_tokens(input), PATH);
         if (ast.type == Type::List) {
           ast.type = Type::RawAst;
           auto l = std::get<std::list<Symbol>>(ast.value);
           for (auto &e : l) {
             e.type = Type::RawAst;
           }
           ast.value = l;
         }
         return ast;
       } catch (std::logic_error ex) {
         return Symbol("", std::string(ex.what()), Type::Error);
       };
     }}},
    {"tokens", {[](std::list<Symbol> args) -> Symbol {
      // returns a list of tokens from a single string given as argument,
      // as if it went throught the ordinary lexing of some Rewind input
      // (because this is exactly what we're doing here)
      if (args.size() != 1) {
	throw std::logic_error {"The 'tokens' function only accepts a single string!\n"};
      }
      if (args.front().type != Type::String) {
	throw std::logic_error {"The 'tokens' function only accepts a single string!\n"};
      }
      std::vector<std::string> tks = get_tokens(std::get<std::string>(args.front().value));
      auto ret = std::list<Symbol>();
      for (auto tk : tks) {
	ret.push_back(Symbol("", tk, Type::String));
      }
      return Symbol("", ret, Type::List);
    }}}};

std::array<std::string, 10> special_forms = {
    "->", "let", "if", "$", "cond", "match", "<<<", "defined", "and", "or"};
