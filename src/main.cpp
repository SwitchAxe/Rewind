// Copyright 2022-2023 Sofia Cerasuoli (@SwitchAxe)
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
#include "shell/shell.hpp"
#include "src/evaluator.hpp"
#include "src/lexer.hpp"
#include "src/parser.hpp"
#include "src/procedures.hpp"
#include <string>
int main(int argc, char **argv) {
  auto PATH = rewind_get_system_PATH();
  if ((argc > 1) && (std::string{argv[1]} == "--")) {
    for (int i = 1; i < argc; ++i) {
      std::string __argvi{argv[i]};
      Symbol sym = Symbol("", __argvi, Type::String);
      cmdline_args.insert({std::to_string(i - 1), eval(sym, *PATH)});
    }
  } else if (argc > 2) {
    std::string filename{argv[1]};
    std::string expr = rewind_read_file(filename);
    std::vector<std::string> expr_list = rewind_split_file(expr);
    if (std::string{argv[2]} != "--") {
      throw std::logic_error{
          "please separate the script name from the arguments with '--'!\n"};
    }
    for (int i = 2; i < argc; ++i) {
      std::string __argvi{argv[i]};
      Symbol sym = Symbol("", __argvi, Type::String);
      cmdline_args.insert({std::to_string(i - 2), eval(sym, *PATH)});
    }
    if (PATH != std::nullopt) {
      for (auto s : expr_list) {
        Symbol ast = eval(get_ast(get_tokens(s)), *PATH);
        if ((ast.type != Type::Command) && (ast.type != Type::Defunc)) {
          rec_print_ast(ast);
          std::cout << "\n";
        }
      }
    } else {
      for (auto s : expr_list) {
        Symbol ast = eval(get_ast(get_tokens(s)), {});
        if ((ast.type != Type::Command) && (ast.type != Type::Defunc)) {
          rec_print_ast(ast);
          std::cout << "\n";
        }
      }
    }
    return 0;
  } else if (argc > 1) {
    std::string filename{argv[1]};
    std::string expr = rewind_read_file(filename);
    std::vector<std::string> expr_list = rewind_split_file(expr);
    if (PATH != std::nullopt) {
      for (auto s : expr_list) {
        Symbol ast = eval(get_ast(get_tokens(s)), *PATH);
        if ((ast.type != Type::Command) && (ast.type != Type::Defunc)) {
          rec_print_ast(ast);
          std::cout << "\n";
        }
      }
    } else {
      for (auto s : expr_list) {
        Symbol ast = eval(get_ast(get_tokens(s)), {});
        if ((ast.type != Type::Command) && (ast.type != Type::Defunc)) {
          rec_print_ast(ast);
          std::cout << "\n";
        }
      }
    }
    return 0;
  }
  rewind_sh_loop();
  return 0;
}
