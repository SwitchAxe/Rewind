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
#include "src/external.hpp"
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <readline/readline.h>
#include <readline/history.h>


std::string rewind_readline() {
  auto cur = fs::current_path();
  std::string prompt = cur.string() + "> ";
  char* read_line = readline(prompt.c_str());
  std::string line{read_line};
  free(read_line);
  return line;
}

std::string rewind_read_file(std::string filename) {
  std::string expr;
  std::string temp;
  std::ifstream in(filename);
  while (std::getline(in, temp)) {
    expr += temp;
  }
  in.close();
  return expr;
}

std::optional<std::string>
rewind_get_env_var(const std::string& query) {
  const char* r = std::getenv(query.c_str());
  if (r) return std::optional<std::string> {std::string(r)};
  return std::nullopt;
}

std::optional<std::vector<std::string>>
rewind_get_system_PATH() {
  auto opt = rewind_get_env_var("PATH");
  if (opt == std::nullopt) return std::nullopt;
  std::vector<std::string> path_v;
  std::string temp;
  int pos = 0;
  while ((pos = (*opt).find(':')) != std::string::npos) {
    temp = (*opt).substr(0, pos);
    path_v.push_back(temp);
    (*opt).erase(0, pos+1);
  }
  if (!(*opt).empty()) path_v.push_back(*opt);
  return std::optional<std::vector<std::string>> {path_v};
}

//returns the full path of an executable on success,
//or std::nullopt on failure.
void rewind_sh_loop() {
  std::string line;
  auto PATH = rewind_get_system_PATH();
  if (PATH == std::nullopt)
    throw std::logic_error {
      "The system PATH is empty! I can't proceed. Aborting... \n"
    };
  do {
    line = rewind_readline();
    if ((line == "exit") ||
	(line == "(exit)"))
      break;
    try {
      Symbol ast = eval(get_ast(get_tokens(line)), *PATH);
      if (ast.type != Type::Command) rec_print_ast(ast);
      std::cout << "\n";
    } catch (std::logic_error ex) {
      std::cout << ex.what() << "\n";
      continue;
    }
  } while (1);
}
