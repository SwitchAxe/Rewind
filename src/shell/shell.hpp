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
#include "src/procedures.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ostream>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdexcept>
#include <string>
#include <variant>
std::string rewind_read_file(std::string filename) {
  std::string expr;
  std::string temp;
  std::ifstream in(filename);
  bool is_comment = false;
  while (std::getline(in, temp)) {
    // comments are skipped
    for (auto ch : temp) {
      if ((ch != ' ') && (ch != '\t')) {
	if (ch == '#') {
	  is_comment = true;
	} else is_comment = false;
	break;
      }
    }
    if (!is_comment) expr += temp + "\n";
  }
  in.close();
  return expr;
}

std::vector<std::pair<int, std::string>> rewind_split_file(std::string content) {
  int bracket_balance = 0;
  std::vector<std::pair<int, std::string>> ret;
  int current_line = 0;
  std::string temp;
  bool in_string = false;
  for (int i = 0; i < content.size(); ++i) {
    if (content[i] == '\n') {
      current_line++;
    }
    if (content[i] == '"') {
      temp += '"';
      in_string = !in_string;
      continue;
    }
    if (in_string) {
      temp += content[i];
      continue;
    }
    if ((content[i] == '(') || (content[i] == '[')) {
      bracket_balance++;
      temp += content[i];
    } else if ((content[i] == ')') || (content[i] == ']')) {
      bracket_balance--;
      temp += content[i];
      if (!bracket_balance) {
        ret.push_back(std::make_pair(current_line, temp));
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
      throw std::logic_error{"Failed to locate the config file! Using none..."};
      return std::nullopt;
    }
    home = "/home/" + *user + "/.config/rewind/config.re";
  }
  home = *home + "/.config/rewind/config.re";
  return home;
};

std::optional<Symbol> rewind_read_config(const path &PATH) {
  auto conf = rewind_config_file();
  if (conf == std::nullopt) {
    return std::nullopt;
  }
  std::string content = rewind_read_file(*conf);
  auto expr_vec = rewind_split_file(content);
  Symbol last_evaluated;
  Symbol last_expr;
  for (auto expr : expr_vec) {
    try {
      last_expr = get_ast(get_tokens(expr.second));
      last_evaluated = eval(last_expr, PATH);
    } catch (std::logic_error e) {
      throw std::logic_error { "(line " + std::to_string(expr.first) + "): error in the Rewind config file!\n" + e.what() + "\n"};
    }
  }
  return last_expr;
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

std::string
rewind_readline(std::optional<Symbol> maybe_prompt,
                const std::optional<std::vector<std::string>> &PATH) {
  std::string prompt;
  if (maybe_prompt != std::nullopt) {
    Symbol sym = *maybe_prompt;
    if (sym.type == Type::String) {
      prompt = std::get<std::string>(sym.value);
    } else if (sym.type == Type::List) {
      Symbol evaluated;
      if (PATH != std::nullopt) {
        evaluated = eval(sym, *PATH);
      } else {
        evaluated = eval(sym, {});
      }
      if (evaluated.type == Type::String) {
        prompt = std::get<std::string>(evaluated.value);
      }
    }
  } else {
    auto cur = fs::current_path();
    prompt = cur.string() + "> ";
  }
  char *lptr = readline(prompt.c_str());
  std::string line{lptr};
  free(lptr);
  return line;
}

void rewind_sh_loop() {
  std::string line;
  auto PATH = rewind_get_system_PATH();
  std::optional<Symbol> maybe_prompt;
  if (PATH != std::nullopt)
    maybe_prompt = rewind_read_config(*PATH);
  else
    maybe_prompt = rewind_read_config({});
  if (PATH == std::nullopt)
    throw std::logic_error{
        "The system PATH is empty! I can't proceed. Aborting... \n"};
  do {
    line = rewind_readline(maybe_prompt, PATH);
    if ((line == "exit") || (line == "(exit)"))
      break;
    if (line.empty())
      continue;
    try {
      Symbol ast = eval(get_ast(get_tokens(line)), *PATH);
      if ((ast.type != Type::Command) && (ast.type != Type::Defunc)) {
        std::cout << rec_print_ast(ast);
      }
    } catch (std::logic_error ex) {
      procedures["cookedmode"]({}, {});
      std::cout << ex.what() << "\n";
      continue;
    }
  } while (1);
}
