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
#include "parser.hpp"
#include "procedures.hpp"
#include "src/external.hpp"
#include "src/types.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <list>
#include <sys/wait.h>
#include <unistd.h>
#include <variant>
Symbol eval(Symbol root, const path &PATH, int line);
Symbol eval_primitive_node(Symbol node, const path &PATH,
                           int line = 0);
Symbol eval_dispatch(Symbol node, const path &PATH,
                     int line);
std::pair<bool, Symbol>
check_for_tail_recursion(std::string name, Symbol funcall, const path &PATH) {
  if (funcall.type != Type::List)
    return {false, funcall};
  auto lst = std::get<std::list<Symbol>>(funcall.value);
  if (lst.empty())
    return {false, funcall};
  auto fstnode = lst.front();
  if ((fstnode.type != Type::Identifier) && (fstnode.type != Type::Operator)) {
    return {false, funcall};
  }
  auto nodename = std::get<std::string>(fstnode.value);
  if ((nodename == "if") || (nodename == "cond")) {
    lst.pop_front();
    auto branch = procedures[nodename](lst, PATH);
    return check_for_tail_recursion(name, branch, PATH);
  }
  if (nodename == name) {
    return {true, funcall};
  }
  return {false, funcall};
}

Symbol eval_function(Symbol node, const path& PATH, int line) {
  auto as_list = std::get<std::list<Symbol>>(node.value);
  Symbol func = *variable_lookup(as_list.front());
  std::string op = std::get<std::string>(as_list.front().value);
  auto func_as_l = std::get<std::list<Symbol>>(func.value);
  // get the various parts of the function
  auto parameters = std::get<std::list<Symbol>>(func_as_l.front().value);
  func_as_l.pop_front();
  auto body = std::get<std::list<Symbol>>(func_as_l.front().value);
  func_as_l.pop_front();
  as_list.pop_front();

  if (parameters.size() != as_list.size())
    throw std::logic_error {"Expected arity ( " +
			    std::to_string(parameters.size()) + ")" +
			    " and supplied number of arguments (" +
			    std::to_string(as_list.size()) +
			    ") for call to " + op + " don't match!\n"};
  std::map<std::string, Symbol> frame = {};
  while (parameters.size() > 0) {
    auto p = std::get<std::string>(parameters.front().value);
    frame.insert(std::make_pair(p, as_list.front()));
    parameters.pop_front();
    as_list.pop_front();
  }

  if (node.type != Type::RecFunCall)
    call_stack.push_back(std::make_pair(op, frame));
  else {
    if (!call_stack.empty())
      call_stack.pop_back();
    call_stack.push_back(std::make_pair(op, frame));
  }
  auto last = body.back();
  body.pop_back();
  Symbol result;
  for (auto e : body) {
    result = eval(e, PATH, line);
  }

  if (auto last_call = check_for_tail_recursion(op, last, PATH);
      last_call.first == false) {
      result = eval(last_call.second, PATH, line);
  } else {
    last = last_call.second;
    last.type = Type::RecFunCall;
    return last;
  }
  call_stack.pop_back();
  return result;
}

// to use with nodes with only leaf children.
Symbol eval_primitive_node(Symbol node, const path &PATH,
                           int line) {
  Symbol result;
  std::optional<std::string> absolute; // absolute path of an executable, if any
  auto it = PATH.begin();
  auto l = std::get<std::list<Symbol>>(node.value);
  if (l.empty())
    return node;
  Symbol op = l.front();
  if ((op.type == Type::Number) ||
      (op.type == Type::String) ||
      (op.type == Type::Boolean)) {
    return node;
  }
  if (node.type == Type::RawAst) {
    return node;
  }
  if (auto x = variable_lookup(op); x != std::nullopt)
    if (x->type == Type::Function)
      return eval_function(node, PATH, line);  

  
  if (op.type == Type::Operator) {
    l.pop_front();
    node.value = l;
    auto s = std::get<std::string>(op.value);
    if (procedures.contains(s)) {
      Functor fun = procedures[s];
      if (s == "->") {
        l.push_front(Symbol("", node.name != "root", Type::Boolean));
      }
      try {
        result = fun(l, PATH);
      } catch (std::logic_error ex) {
        throw std::logic_error{"Rewind (line " + std::to_string(line) +
                               "): " + ex.what()};
      }
      if (result.type == Type::RawAst) {
        return result;
      }
      if (result.is_lit)
        return result;
      result = eval(result, PATH, line);
      return result;
    } else
      throw std::logic_error{"Rewind (line" + std::to_string(line) +
                             "): Unbound procedure " + s + "!\n"};
  } else if (auto lit = std::find_if(
                 l.begin(), l.end(),
                 [&](Symbol &s) -> bool {
                   bool is_local_executable =
                       std::holds_alternative<std::string>(s.value) &&
                       (std::get<std::string>(s.value).substr(0, 2) == "./");
                   bool is_in_path =
                       std::holds_alternative<std::string>(s.value) &&
                       get_absolute_path(std::get<std::string>(s.value),
                                         PATH) != std::nullopt;
                   return is_local_executable || is_in_path;
                 });
             lit != l.end()) {
    get_env_vars(node, PATH);
    auto rest = std::list<Symbol>(lit, l.end());
    rest.pop_front();
    auto ext = *lit;
    if (std::get<std::string>((*lit).value).substr(0, 2) != "./")
      ext.value = *get_absolute_path(std::get<std::string>((*lit).value), PATH);
    rest.push_front(ext);
    Symbol pipel = Symbol("", rest, Type::List);
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
    if (node.name != "root") {
      pipel = Symbol("", std::list<Symbol>{pipel}, Type::List);
      auto result = rewind_pipe(pipel, PATH, true);
      active_pids = {};
      return result;
    }
    auto result = rewind_call_ext_program(pipel, PATH, false);
    while (waitpid(result.pid, nullptr, 0) > 0)
      ;
    active_pids = {};
    return result.s;
  }
  return node;
}

Symbol eval(Symbol root, const path &PATH, int line) {
  Symbol result;
  std::stack<Symbol> node_stk;
  Symbol current_node;
  // results of intermediate nodes (i.e. nodes below the root)
  // this is the data on which the actual computation takes place,
  // as we copy every intermediate result we get into this as a "leaf"
  // to compute the value for each node, including the root.
  std::vector<std::list<Symbol>> leaves;
  current_node = root;
  if (root.is_lit) return root;
  switch (root.type) {
  case Type::Number:
  case Type::String:
  case Type::Boolean:
  case Type::Function:
  case Type::RawAst:
    return root;
  default: break;
  }
  if (root.type == Type::List)
    if (std::get<std::list<Symbol>>(root.value).empty())
      return root;
  do {
    // for each node, visit each child and backtrack to the last parent node
    // when the last child is null, and continue with the second last node and
    // so on
    if (current_node.is_lit) {
      if (node_stk.empty())
        return current_node;
      if (leaves.empty())
        leaves.push_back({});
      leaves[leaves.size() - 1].push_back(current_node);
      current_node = node_stk.top();
      node_stk.pop();
    }
    if (current_node.type == Type::List) {
      if (std::get<std::list<Symbol>>(current_node.value).empty()) {
        // if we're back to the root node, and we don't have any
        // children left, we're done.
        if (leaves.empty())
          break;
        Symbol eval_temp_arg;
        eval_temp_arg =
          Symbol(current_node.name, leaves[leaves.size() - 1], Type::List);
        result = eval_primitive_node(eval_temp_arg, PATH, line);
        leaves.pop_back();

        // main trampoline
        if (result.type == Type::RecFunCall) {
          while (result.type == Type::RecFunCall) {
            result = eval_function(result, PATH, line);
          }
        }
        if (leaves.empty())
          return result;
        leaves[leaves.size() - 1].push_back(result);

        if (!node_stk.empty()) {
          current_node = node_stk.top();
          node_stk.pop();
          if (std::holds_alternative<std::list<Symbol>>(current_node.value) &&
              std::get<std::list<Symbol>>(current_node.value).empty()) {
            if (variables.size() > 1) {
              variables.pop_back();
            }
          }
        } else
          return result;
      } else {
        Symbol child = std::get<std::list<Symbol>>(current_node.value).front();
        auto templ = std::get<std::list<Symbol>>(current_node.value);
        templ.pop_front();
        current_node.value = templ;
        if (child.type == Type::List) {
          auto l = std::get<std::list<Symbol>>(child.value);
          if (l.empty()) {
            if (leaves.empty())
              leaves.push_back({});
            leaves[leaves.size() - 1].push_back(child);
            continue;
          }
          auto frnt = l.front();
          if ((frnt.type == Type::Number) || (frnt.type == Type::String) ||
              (frnt.type == Type::Boolean)) {
            if (leaves.empty())
              leaves.push_back({});
            leaves[leaves.size() - 1].push_back(child);
            continue;
          }
        }
        // if the first element of the 'templ' list is another list then
        // we might have an external program call with env vars, so we must
        // act carefully and delay the evaluation up until 'eval_primitive_node'
        // is invoked.
        if ((child.type == Type::List) && leaves.empty()) {
          leaves.push_back(templ);
          leaves[leaves.size() - 1].push_front(child);
          Symbol dummy;
          if (!node_stk.empty())
            dummy =
                Symbol(node_stk.top().name, std::list<Symbol>(), Type::List);
          else
            dummy = Symbol("", std::list<Symbol>(), Type::List);
          current_node = dummy;
          node_stk.push(current_node);
        } else {
          node_stk.push(current_node);
          current_node = child;
          std::list<Symbol> l;
          if (child.type == Type::List)
            l = std::get<std::list<Symbol>>(child.value);
          if (leaves.empty() || ((child.type == Type::List) && !l.empty()))
            leaves.push_back(std::list<Symbol>{});
          else if ((child.type == Type::List) && l.empty()) {
            if (leaves.empty()) {
              leaves.push_back(l);
            } else {
              leaves[leaves.size() - 1].push_back(child);
            }
            current_node = node_stk.top();
            node_stk.pop();
          }
        }
      }
    } else {
      if (current_node.type == Type::RawAst) {
        return current_node;
      }
      if ((current_node.type == Type::Boolean) ||
          (current_node.type == Type::Number) ||
          (current_node.type == Type::String)) {

        if (leaves.empty())
          leaves.push_back(std::list<Symbol>{});
        leaves[leaves.size() - 1].push_back(current_node);

        if (node_stk.empty())
          return leaves[leaves.size() - 1].front();
        current_node = node_stk.top();
        node_stk.pop();
        continue;
      }
      std::string op;
      if ((current_node.type == Type::Identifier) ||
          (current_node.type == Type::Operator)) {
        op = std::get<std::string>(current_node.value);
      }
      if ((current_node.type == Type::Operator) &&
          (std::find(special_forms.begin(), special_forms.end(), op) !=
           special_forms.end()) &&
          (!node_stk.empty())) {
        // delay the evaluation of special forms
        auto spfl = std::get<std::list<Symbol>>(node_stk.top().value);
        if ((!leaves.empty()) && (!leaves.back().empty())) {
          leaves[leaves.size() - 1].push_back(current_node);
        } else {
          if (leaves.empty())
            leaves.push_back(std::list<Symbol>());
          leaves[leaves.size() - 1] = spfl;
          leaves[leaves.size() - 1].push_front(current_node);
          Symbol dummy =
              Symbol(node_stk.top().name, std::list<Symbol>(), Type::List);
          node_stk.pop();
          node_stk.push(dummy);
        }
      } else if (auto p_opt = callstack_variable_lookup(current_node);
                 p_opt != std::nullopt) {
        if (leaves.empty())
          leaves.push_back(std::list<Symbol>{});
	if (p_opt->type != Type::Function)
          leaves[leaves.size() - 1].push_back(*p_opt);
	else leaves[leaves.size() - 1].push_back(current_node);
      } else if (current_node.type == Type::Identifier) {
        if (op[0] == '$') {
          if (auto var_opt =
                  variable_lookup(Symbol("", op.substr(1), Type::Identifier));
              var_opt != std::nullopt) {
            if (leaves.empty())
              leaves.push_back(std::list<Symbol>{});
            leaves[leaves.size() - 1].push_back(*var_opt);
          } else
            throw std::logic_error{"Unbound variable " + op.substr(1) + "!"};
        } else {
          if (leaves.empty())
            leaves.push_back(std::list<Symbol>{});
          leaves[leaves.size() - 1].push_back(current_node);
        }
      } else {
        if (leaves.empty())
          leaves.push_back(std::list<Symbol>{});
        leaves[leaves.size() - 1].push_back(current_node);
      }
      if (node_stk.empty())
        return leaves[leaves.size() - 1].front();
      current_node = node_stk.top();
      node_stk.pop();
    }
  } while (1);
  return result;
}
