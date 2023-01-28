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
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <readline/history.h>
#include <readline/readline.h>


std::string rewind_readline() {
  auto cur = fs::current_path();
  std::string prompt = cur.string() + "> ";
  char *read_line = readline(prompt.c_str());
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

std::vector<std::string> rewind_split_file(std::string content) {
  int bracket_balance = 0;
  std::vector<std::string> ret;
  std::string temp;
  for (int i = 0; i < content.size(); ++i) {
    if ((content[i] == '(') || (content[i] == '[')) {
      bracket_balance++;
      temp += content[i];
    } else if ((content[i] == ')') || (content[i] == ']')) {
      bracket_balance--;
      temp += content[i];
      if (!bracket_balance) {
        ret.push_back(temp);
        temp = "";
      }
    } else
      temp += content[i];
  }
  return ret;
}

std::optional<std::string> rewind_get_env_var(const std::string &query) {
  const char *r = std::getenv(query.c_str());
  if (r)
    return std::optional<std::string>{std::string(r)};
  return std::nullopt;
}

std::optional<std::string> rewind_config_file() {
  std::optional<std::string> home = rewind_get_env_var("HOME");
  if (home == std::nullopt) {
    std::optional<std::string> user = rewind_get_env_var("USER");
    if (user == std::nullopt) {
      throw std::logic_error {"Failed to locate the config file! Using none..."};
      return std::nullopt;
    }
    home = "/home/" + *user + "/.config/rewind/config.re";
  }
  home = *home + "/.config/rewind/config.re";
  return home;
};

std::optional<Symbol> rewind_read_config(const path& PATH) {
  auto conf = rewind_config_file();
  if (conf == std::nullopt) {
    return std::nullopt;
  }
  std::string content = rewind_read_file(*conf);
  auto expr_vec = rewind_split_file(content);
  Symbol last_evaluated;
  for (auto expr : expr_vec) {
    try {
      last_evaluated = eval(get_ast(get_tokens(expr)), PATH);
    } catch (std::exception e) {
      std::cout << "error in the Rewind config file!\n" << e.what() << "\n";
    }
  }
  return last_evaluated;
}

std::optional<std::vector<std::string>> rewind_get_system_PATH() {
  auto opt = rewind_get_env_var("PATH");
  if (opt == std::nullopt)
    return std::nullopt;
  std::vector<std::string> path_v;
  std::string temp;
  int pos = 0;
  while ((pos = (*opt).find(':')) != std::string::npos) {
    temp = (*opt).substr(0, pos);
    path_v.push_back(temp);
    (*opt).erase(0, pos + 1);
  }
  if (!(*opt).empty())
    path_v.push_back(*opt);
  return std::optional<std::vector<std::string>>{path_v};
}

// returns the full path of an executable on success,
// or std::nullopt on failure.
void rewind_sh_loop() {
  std::string line;
  auto PATH = rewind_get_system_PATH();
  if (PATH == std::nullopt)
    throw std::logic_error{
        "The system PATH is empty! I can't proceed. Aborting... \n"};
  do {
    line = rewind_readline();
    if ((line == "exit") || (line == "(exit)"))
      break;
    try {
      Symbol ast = eval(get_ast(get_tokens(line)), *PATH);
      if (ast.type != Type::Command)
        rec_print_ast(ast);
      std::cout << "\n";
    } catch (std::logic_error ex) {
      std::cout << ex.what() << "\n";
      continue;
    }
  } while (1);
}
