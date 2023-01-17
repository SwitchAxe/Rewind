#include "src/external.hpp"
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <readline/readline.h>
#include <readline/history.h>


std::string rewind_readline() {
  char* read_line = readline("> ");
  std::string line{read_line};
  free(read_line);
  return line;
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
