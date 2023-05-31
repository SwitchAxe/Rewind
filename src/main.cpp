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
#include "src/external.hpp"
#include "src/lexer.hpp"
#include "src/parser.hpp"
#include "src/procedures.hpp"
#include <exception>
#include <signal.h>
#include <string>
#include <sys/types.h>
#include <termios.h>
static void catch_SIGINT(int sig) {
  for (auto p : active_pids) {
    kill(p, SIGINT);
  }
  active_pids = {};
}
int main(int argc, char **argv) {
  tcgetattr(STDIN_FILENO, &original);
  // enable some sort of "pseudo raw" mode where characters are
  // available immediately, without modifying anything else
  memcpy(&immediate, &original, sizeof(termios));
  immediate.c_lflag &= ~ICANON;
  immediate.c_lflag &= ~ECHO;
  immediate.c_cc[VMIN] = 1;
  signal(SIGINT, catch_SIGINT);
  auto PATH = rewind_get_system_PATH();
  std::optional<Symbol> conf;
  if (PATH != std::nullopt)
    conf = rewind_read_config(*PATH);
  else {
    conf = rewind_read_config({});
  }
  if ((argc > 2) && (std::string{argv[1]} == "--silent")) {
    // Rewind will execute the single expression taken as input, and then exit.
    if (PATH != std::nullopt) {
      try {
        std::cout << rec_print_ast(
            eval(get_ast(get_tokens(std::string{argv[2]}), *PATH), *PATH));
      } catch (std::exception e) {
        std::cout << "Exception: " << e.what() << "\n";
      }
      tcsetattr(STDIN_FILENO, TCSANOW, &original);
      return 0;
    }
    try {
      std::cout << rec_print_ast(
          eval(get_ast(get_tokens(std::string{argv[2]}), {}), {}));
    } catch (std::exception e) {
      std::cout << "Exception: " << e.what() << "\n";
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
    return 0;
  }

  if ((argc > 1) && (std::string{argv[1]} == "--")) {
    for (int i = 1; i < argc; ++i) {
      std::string __argvi{argv[i]};
      Symbol sym = Symbol("", __argvi, Type::String);
      cmdline_args.insert({std::to_string(i - 1), eval(sym, *PATH)});
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
  } else if (argc > 2) {
    std::string filename{argv[1]};
    std::string expr = rewind_read_file(filename);
    auto expr_list = rewind_split_file(expr);
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
        Symbol ast;
        try {
          ast = get_ast(get_tokens(s.second), *PATH);
        } catch (std::logic_error ex) {
          std::cout << "Rewind: Exception at line " << s.first << "of file"
                    << filename << "; " << ex.what() << "\n";
          break;
        }
        ast = eval_dispatch(ast, *PATH, s.first);
        if ((ast.type != Type::Command) && (ast.type != Type::Defunc)) {
          std::cout << rec_print_ast(ast);
          std::cout << "\n";
        }
      }
    } else {
      for (auto s : expr_list) {
        Symbol ast;
        try {
          ast = get_ast(get_tokens(s.second), {});
        } catch (std::logic_error ex) {
          std::cout << "Rewind: Exception at line " << s.first << "of file"
                    << filename << "; " << ex.what() << "\n";
          break;
        }
        ast = eval_dispatch(ast, {}, s.first);
        if ((ast.type != Type::Command) && (ast.type != Type::Defunc)) {
          std::cout << rec_print_ast(ast);
          std::cout << "\n";
        }
      }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
    return 0;
  } else if (argc > 1) {
    std::string filename{argv[1]};
    std::string expr = rewind_read_file(filename);
    auto expr_list = rewind_split_file(expr);
    if (PATH != std::nullopt) {
      for (auto s : expr_list) {
        Symbol ast;
        try {
          ast = get_ast(get_tokens(s.second), *PATH);
        } catch (std::logic_error ex) {
          std::cout << "Rewind: Exception at line " << s.first << "of file"
                    << filename << "; " << ex.what() << "\n";
          break;
        }
        ast = eval_dispatch(ast, *PATH, s.first);
        if ((ast.type != Type::Command) && (ast.type != Type::Defunc)) {
          std::cout << rec_print_ast(ast);
          std::cout << "\n";
        }
      }
    } else {
      for (auto s : expr_list) {
        Symbol ast;
        try {
          ast = get_ast(get_tokens(s.second), {});
        } catch (std::logic_error ex) {
          std::cout << "Rewind: Exception at line " << s.first << "of file"
                    << filename << "; " << ex.what() << "\n";
          break;
        }
        ast = eval_dispatch(ast, {}, s.first);
        if ((ast.type != Type::Command) && (ast.type != Type::Defunc)) {
          std::cout << rec_print_ast(ast);
          std::cout << "\n";
        }
      }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
    return 0;
  }
  tcsetattr(STDIN_FILENO, TCSANOW, &original);
  rewind_sh_loop();
  return 0;
}
