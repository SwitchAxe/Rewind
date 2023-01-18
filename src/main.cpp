#include "shell/shell.hpp"
#include "src/evaluator.hpp"
#include "src/lexer.hpp"
#include "src/parser.hpp"
#include "src/procedures.hpp"
int main(int argc, char** argv) {
  if (argc == 2) {
    std::string filename{argv[1]};
    std::string expr = rewind_read_file(filename);
    auto PATH = rewind_get_system_PATH();
    if (PATH != std::nullopt)
      rec_print_ast(eval(get_ast(get_tokens(expr)), *PATH));
    else
      rec_print_ast(eval(get_ast(get_tokens(expr)), {}));
    std::cout << "\n";
    return 0;
  }
  rewind_sh_loop();
  return 0;
}
