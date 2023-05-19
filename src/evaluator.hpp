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
#include <filesystem>
#include <iostream>
#include <iterator>
#include <list>
#include <sys/wait.h>
#include <unistd.h>
#include <variant>
Symbol eval(Symbol root, const std::vector<std::string> &PATH);
Symbol eval_primitive_node(Symbol node, const std::vector<std::string> &PATH);

std::optional<Symbol> check_for_tail_recursion(std::string name, Symbol funcall,
                                               path &PATH) {
  if (funcall.type != Type::List) return std::nullopt;
  auto lst = std::get<std::list<Symbol>>(funcall.value);
  if (lst.empty()) return std::nullopt;
  auto fstnode = lst.front();
  if ((fstnode.type != Type::Identifier) && (fstnode.type != Type::Operator)) {
    return std::nullopt;
  }
  auto nodename = std::get<std::string>(fstnode.value);
  if ((nodename == "if") || (nodename == "cond")) {
    lst.pop_front();
    auto branch = procedures[nodename](lst, PATH);
    return check_for_tail_recursion(name, branch, PATH);
  }
  if (nodename == name) {
    return funcall;
  }
  return std::nullopt;
}

Symbol eval_function(Symbol node, const std::vector<std::string> &PATH) {
  Symbol result;
  auto argl = std::get<std::list<Symbol>>(node.value);
  auto dummy = argl.front();
  std::string op = std::get<std::string>(argl.front().value);
  argl.pop_front();
  if (!user_defined_procedures[user_defined_procedures.size() - 1].contains(
          op)) {
    throw std::logic_error{"Unbound procedure " + op + "!\n"};
  }
  Symbol params =
      user_defined_procedures[user_defined_procedures.size() - 1][op].first;
  auto paraml = std::get<std::list<Symbol>>(params.value);
  Symbol body =
      user_defined_procedures[user_defined_procedures.size() - 1][op].second;
  if (argl.size() != paraml.size()) {
    throw std::logic_error{"Wrong amount of arguments to procedure " + op +
                           "!\n"};
  }
  std::map<std::string, Symbol> frame = {};
  size_t paramls = paraml.size();
  std::list<std::pair<Symbol, Symbol>> zipped;
  std::transform(
      paraml.begin(), paraml.end(), argl.begin(), std::back_inserter(zipped),
      [](const Symbol &lhs, const Symbol &rhs) -> std::pair<Symbol, Symbol> {
        return std::make_pair(lhs, rhs);
      });
  for (auto p : zipped) {
    frame.insert_or_assign(std::get<std::string>(p.first.value),
                           eval(p.second, PATH));
  }
  if (node.type != Type::Funcall)
    call_stack.push_back(std::make_pair(op, frame));
  else {
    call_stack.pop_back();
    call_stack.push_back(std::make_pair(op, frame));
  }
  std::list<Symbol> body_list = std::get<std::list<Symbol>>(body.value);
  Symbol last = body_list.back();
  body_list.pop_back();
  for (auto e : body_list) {
    result = eval(e, PATH);
  }
  if (auto last_call = check_for_tail_recursion(op, last, PATH);
      last_call == std::nullopt) {
    result = eval(last, PATH);
    result = eval(result, PATH);
  } else {
    last = *last_call;
    last.type = Type::Funcall;
    return last;
  }
  call_stack.pop_back();
  return result;
}

// to use with nodes with only leaf children.
Symbol eval_primitive_node(Symbol node, const std::vector<std::string> &PATH) {
  Symbol result;
  std::optional<std::string> absolute; // absolute path of an executable, if any
  auto it = PATH.begin();
  auto l = std::get<std::list<Symbol>>(node.value);
  if (l.empty())
    return node;
  Symbol op = l.front();
  if ((op.type == Type::Number) || (op.type == Type::String)) {
    return node;
  }
  if (node.type == Type::RawAst) {
    result.type = Type::List;
    return node;
  }
  if (op.type == Type::Operator) {
    l.pop_front();
    node.value = l;
    if (procedures.contains(std::get<std::string>(op.value))) {
      Functor fun = procedures[std::get<std::string>(op.value)];
      result = fun(std::get<std::list<Symbol>>(node.value), PATH);
      if (result.type == Type::RawAst) {
	result.type = Type::List;
	return result;
      }
      return eval(result, PATH);
    } else
      throw std::logic_error{"Unbound procedure!\n"};
  } else if ((op.type == Type::Identifier) &&
             !user_defined_procedures.empty() &&
             user_defined_procedures[user_defined_procedures.size() - 1]
                 .contains(std::get<std::string>(op.value))) {
    result = eval_function(node, PATH);
    return result;
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
    if (node.name != "root") {
      pipel = Symbol("", std::list<Symbol>{pipel}, Type::List);
      auto result = rewind_pipe(pipel, PATH);
      active_pids = {};
      return result;
    }
    auto result = rewind_call_ext_program(pipel, PATH, false);
    while (wait(nullptr) != -1)
      ;
    active_pids = {};
    return result;
  }
  return node;
}

Symbol eval(Symbol root, const std::vector<std::string> &PATH) {
  Symbol result;
  std::stack<Symbol> node_stk;
  Symbol current_node;
  // results of intermediate nodes (i.e. nodes below the root)
  // this is the data on which the actual computation takes place,
  // as we copy every intermediate result we get into this as a "leaf"
  // to get compute the value for each node, including the root.
  std::vector<std::list<Symbol>> leaves;
  current_node = root;
  do {
    // for each node, visit each child and backtrack to the last parent node
    // when the last child is null, and continue with the second last node and
    // so on
    if (current_node.type == Type::List) {
      if (std::get<std::list<Symbol>>(current_node.value).empty()) {
        // if we're back to the root node, and we don't have any
        // children left, we're done.
        if (leaves.empty()) {
          if ((current_node.type == Type::List) &&
              (std::get<std::list<Symbol>>(current_node.value).empty())) {
            return current_node;
          }
          break;
        }
        Symbol eval_temp_arg;
        // clean up for any function definition...
        std::erase_if(leaves[leaves.size() - 1], [](Symbol &s) -> bool {
          return (s.type == Type::Defunc) || (s.type == Type::Command);
        });

        eval_temp_arg =
            Symbol(current_node.name, leaves[leaves.size() - 1], Type::List);
        result = eval_primitive_node(eval_temp_arg, PATH);
        leaves.pop_back();

        // main trampoline
        if (result.type == Type::Funcall) {
          while (result.type == Type::Funcall) {
            result = eval_function(result, PATH);
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
          if (leaves.empty() ||
              ((child.type == Type::List) &&
               !(std::get<std::list<Symbol>>(child.value).empty())))
            leaves.push_back(std::list<Symbol>{});
          else if ((child.type == Type::List) &&
                   (std::get<std::list<Symbol>>(child.value).empty())) {
            if (leaves.empty()) {
              leaves.push_back(std::list<Symbol>{child});
            } else {
              leaves[leaves.size() - 1].push_back(child);
            }
            current_node = node_stk.top();
            node_stk.pop();
          }
        }
      }
    } else {
      if ((current_node.type == Type::Operator) &&
          (std::find(special_forms.begin(), special_forms.end(),
                     std::get<std::string>(current_node.value)) !=
           special_forms.end())) {
        // delay the evaluation of special forms
        auto spfl = std::get<std::list<Symbol>>(node_stk.top().value);
        if (leaves.empty())
          leaves.push_back(std::list<Symbol>());
        leaves[leaves.size() - 1] = spfl;
        leaves[leaves.size() - 1].push_front(current_node);
        Symbol dummy =
            Symbol(node_stk.top().name, std::list<Symbol>(), Type::List);
        node_stk.pop();
        node_stk.push(dummy);
      } else if ((leaves.empty() || leaves[leaves.size() - 1].empty()) &&
                 (current_node.type == Type::Identifier) &&
                 (!user_defined_procedures.empty()) &&
                 (!user_defined_procedures[user_defined_procedures.size() - 1]
                       .contains(std::get<std::string>(current_node.value))) &&
                 (get_absolute_path(std::get<std::string>(current_node.value),
                                    PATH) != std::nullopt) &&
                 (!node_stk.empty())) {
        if (leaves.empty())
          leaves.push_back(std::list<Symbol>());
        auto extcall = std::get<std::list<Symbol>>(node_stk.top().value);
        leaves[leaves.size() - 1] = extcall;
        leaves[leaves.size() - 1].push_front(current_node);
        Symbol dummy;
        dummy = Symbol(node_stk.top().name, std::list<Symbol>(), Type::List);
        node_stk.pop();
        node_stk.push(dummy);
      } else if ((leaves.empty() || leaves[leaves.size() - 1].empty()) &&
                 (current_node.type == Type::Identifier)) {
        auto opt = variable_lookup(current_node);
        auto popt = procedure_lookup(current_node);
        auto csopt = callstack_variable_lookup(current_node);
        if (csopt != std::nullopt) {
          if (!leaves.empty())
            leaves[leaves.size() - 1].push_front(*csopt);
          else
            leaves.push_back(std::list<Symbol>{*csopt});
        } else if (opt != std::nullopt) {
          if (!leaves.empty())
            leaves[leaves.size() - 1].push_front(*opt);
          else
            leaves.push_back(std::list<Symbol>{*opt});
        } else {
          if (!leaves.empty())
            leaves[leaves.size() - 1].push_front(current_node);
          else
            leaves.push_back(std::list<Symbol>{current_node});
        }
      } else if (current_node.type == Type::Identifier) {
        auto opt = variable_lookup(current_node);
        auto popt = procedure_lookup(current_node);
        auto csopt = callstack_variable_lookup(current_node);
        if (csopt != std::nullopt) {
          if (!leaves.empty())
            leaves[leaves.size() - 1].push_back(*csopt);
          else
            leaves.push_back(std::list<Symbol>{*csopt});
        } else if (opt != std::nullopt) {
          if (!leaves.empty())
            leaves[leaves.size() - 1].push_back(*opt);
          else
            leaves.push_back(std::list<Symbol>{*opt});
        } else {
          if (!leaves.empty())
            leaves[leaves.size() - 1].push_back(current_node);
          else
            leaves.push_back(std::list<Symbol>{current_node});
        }
      } else {
        if (!leaves.empty())
          leaves[leaves.size() - 1].push_back(current_node);
        else
          leaves.push_back(std::list<Symbol>{current_node});
      }
      if (node_stk.empty())
        return leaves[leaves.size() - 1].front();
      current_node = node_stk.top();
      node_stk.pop();
    }
  } while (1);
  return result;
}
