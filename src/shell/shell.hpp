#include "src/evaluator.hpp"

std::string rewind_readline() {
  std::string line;
  std::getline(std::cin, line);
  return line;
}
void rewind_sh_loop() {
  std::string line;
  std::cout << "> " << std::flush;
  do {
    line = rewind_readline();
    if ((line == "exit") ||
	(line == "(exit)"))
      break;
    try {
      rec_print_ast(eval(get_ast(get_tokens(line))));
      std::cout << "\n> " << std::flush;
    } catch (std::logic_error ex) {
      std::cout << ex.what() << "\n> " << std::flush;
      continue;
    }
  } while (1);
}
