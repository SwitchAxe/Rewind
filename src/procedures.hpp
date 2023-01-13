#pragma once
#include "matchit.h"
#include "types.hpp"
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>
std::vector<std::map<std::string, Symbol>> variables;
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
void rec_print_ast(Symbol root);
namespace fs = std::filesystem;
// just so the compiler doesn't complain about nonexistent PATH
// later in the 'procedures' map.
std::string process_escapes(const std::string &s) {
  std::string r;
  for (int i = 0; i < s.length(); ++i) {
    if (!(s[i] == '\\')) {
      r += s[i];
      continue;
    }
    switch (s[i + 1]) {
    case 'n':
      r += '\n';
      break;
    case '\\':
      r += '\\';
      break;
    case 'a':
      r += '\a';
      break;
    case 'b':
      r += '\b';
      break;
    case 'v':
      r += '\v';
      break;
    case 'f':
      r += '\f';
      break;
    case 'r':
      r += '\r';
      break;
    case 'e':
      r += '\x1b';
      break;
    case 't':
      r += '\t';
      break;
    default:
      r += s[i];
      r += s[i + 1];
      break;
    }
    i++;
  }
  return r;
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
       return Symbol("", fs::current_path(), Type::String,
                     args.front().depth);
     }}},
    {"set", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 2)
         throw std::logic_error{
             "The 'set' builtin command expects precisely two arguments!\n"};
       std::string var = std::get<std::string>(args.front().value);
       std::string val = std::get<std::string>(args.back().value);
       return Symbol("", setenv(var.c_str(), val.c_str(), 1), Type::Number,
                     args.front().depth);
     }}},
    {"get", {[](std::list<Symbol> args) -> Symbol {
       if (args.size() != 1)
         throw std::logic_error{
             "The 'get' builtin command expects precisely one argument!"};
       char *s = std::getenv(std::get<std::string>(args.front().value).c_str());
       if (s)
         return Symbol("", std::string(s), Type::String,
                       args.front().depth - 1);
       return Symbol("", "Nil", Type::String, args.front().depth);
     }}},
    {"->", {[](std::list<Symbol> args, path PATH) -> Symbol {
       auto it = PATH.begin();
       for (auto &e : args) {
         auto progl = std::get<std::list<Symbol>>(e.value);
         auto prog = std::get<std::string>(progl.front().value);
         progl.pop_front();
         it = std::find_if(PATH.begin(), PATH.end(),
                           [&](const std::string &query) -> bool {
                             std::string full_path;
                             full_path = query + "/" + prog;
                             return fs::directory_entry(full_path).exists();
                           });
         if (it == PATH.end())
           throw std::logic_error{"Unknown executable " + prog + "!\n"};
         std::string full_path;
         if (procedures.contains(prog))
           full_path = prog;
         else
           full_path = (*it) + "/" + prog;
         progl.push_front(Symbol("", full_path, Type::Identifier));
         e.value = progl;
       }
       Symbol node = Symbol("", args, Type::List);
       Symbol result = rewind_pipe(node, PATH);
       return Symbol("", result.value, result.type, args.front().depth);
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
       Symbol ret("", r, Type::Number, args.front().depth);
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
       Symbol ret("", r, Type::Number, args.front().depth);
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
       Symbol ret("", r, Type::Number, args.front().depth);
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
       Symbol ret("", r, Type::Number, args.front().depth);
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
       return Symbol("", is_true, Type::Boolean, args.front().depth);
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
         return Symbol("", true, Type::Defunc, args.front().depth);
       }
       Symbol id = args.front();
       args.pop_front();
       if (variables.empty()) {
         variables.push_back(std::map<std::string, Symbol>());
       }
       variables[variables.size() - 1].insert_or_assign(
           std::get<std::string>(id.value), args.front());
       return Symbol("", args.front().value, args.front().type,
                     args.front().depth - 1);
     }}},
    {"if", {[](std::list<Symbol> args) -> Symbol {
       // (if <clause> (expr1 ... exprn) (else1 ... elsen))
       // if <clause> converts to Cpp's "true" then return
       // (expr1 ... exprn) to the caller, and the other
       // list otherwise.
       if (args.size() != 3) {
         throw std::logic_error{
             "An if statement must have precisely three arguments!\n"};
       }
       bool clause = convert_value_to_bool(args.front());
       args.pop_front();
       if (clause) {
         return args.front();
       }
       args.pop_front();
       return Symbol("", args.front().value, args.front().type,
                     args.front().depth);
     }}}};

std::array<std::string, 3> special_forms = {
  "->", "let", "if"
};
