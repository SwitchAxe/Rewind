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
#include <charconv>
#include <iostream>
#include <stack>
#include <type_traits>





Symbol get_ast(std::vector<std::string> tokens) {
  int round_balance = 0;  // for paren balancing
  int square_balance = 0; // see above
  bool list_entered = false;
  Symbol node;
  std::stack<Symbol> node_stk;
  int depth = 0;
  for (auto tok : tokens) {
    if ((tok == "(") || (tok == "[")) {
      if (tok == "(")
        round_balance++;
      else
        square_balance++;
      if (std::holds_alternative<std::monostate>(node.value)) {
        node.type = Type::List;
        node.name = "root";
        node.value = std::list<Symbol>();
        node.depth = ++depth;
      } else {
        if (!std::holds_alternative<std::list<Symbol>>(node.value)) {
          throw std::logic_error{"Failed to parse the input expression!"
                                 "Unexpected list...\n"};
        } else {
          // append a new list at the end and update
          // the current node 'node'.
          node_stk.push(node);
          node.value = std::list<Symbol>();
          node.depth = ++depth;
        }
      }
    } else if ((tok == ")") || (tok == "]")) {
      if (tok == ")")
        round_balance--;
      else
        square_balance--;
      // we finished parsing the subexpression/sublist, so we can
      // append the resulting tree to the previous one.
      depth--;
      Symbol child = node;
      if (!node_stk.empty()) {
        node = node_stk.top();
        node_stk.pop();
        if (node.type != Type::List) {
          throw std::logic_error{
              "I have honestly no clue of how you got this error.\n"};
        }
        auto temp = std::get<std::list<Symbol>>(node.value);
        temp.push_back(child);
        node.value = temp;
      } else
        return node;
    } else {
      // identifiers, operators, literals and so on
      Symbol child;
      int v;
      if (auto [ptr, ec] =
              std::from_chars(tok.data(), tok.data() + tok.size(), v);
          ptr == tok.data() + tok.size()) {
        // a number!
        child.type = Type::Number;
        child.value = v;
      } else if (procedures.contains(tok)) {
        child.type = Type::Operator;
        child.value = tok;
      } else if (tok[0] == '"') {
        child.type = Type::String;
        child.value = tok;
      } else if ((tok == "false") || (tok == "true")) {
        child.type = Type::Boolean;
        child.value = (tok == "false") ? false : true;
      } else {
        child.type = Type::Identifier;
        child.value = tok;
      }
      child.depth = depth;
      if (std::holds_alternative<std::list<Symbol>>(node.value)) {
        auto temp = std::get<std::list<Symbol>>(node.value);
        temp.push_back(child);
        node.value = temp;
      } else {
        node = child;
      }
    }
  }
  if (depth)
    throw std::logic_error{"Missing closing parenthesis for a list!\n"};
  if (round_balance) {
    throw std::logic_error{"Missing closing parenthesis for an list!\n"};
  }
  if (square_balance) {
    throw std::logic_error{"Missing closing square bracket for a list!\n"};
  }
  return node;
}

// DEBUG PURPOSES ONLY and for printing the final result until i
// make an iterative version of this thing

void rec_print_ast(Symbol root) {
  std::cout << std::boolalpha;
  if (root.type == Type::List) {
    std::cout << "[ ";
    for (auto s : std::get<std::list<Symbol>>(root.value)) {
      if (s.type == Type::List) {
        rec_print_ast(s);
      } else {
        std::visit(
            [&]<class T>(T &&v) -> void {
              if constexpr (std::is_same_v<std::decay_t<T>, std::monostate>) {
              } else if constexpr (std::is_same_v<std::decay_t<T>,
                                                  std::list<Symbol>>) {
              } else if constexpr (std::is_same_v<std::decay_t<T>,
                                                  std::string>) {
                std::cout << process_escapes(v) << " ";
              } else
                std::cout << v << " ";
            },
            s.value);
      }
    }
    std::cout << "]";
  } else {
    std::visit(
        [&]<class T>(T &&v) -> void {
          if constexpr (std::is_same_v<std::decay_t<T>, std::monostate>) {
          } else if constexpr (std::is_same_v<std::decay_t<T>,
                                              std::list<Symbol>>) {
          } else
            std::cout << v << " ";
        },
        root.value);
  }
}
