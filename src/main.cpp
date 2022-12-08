#include "evaluator.hpp"

int main(int argc, char** argv) {
  std::string temp;
  std::getline(std::cin, temp);
  rec_print_ast(eval(get_ast(get_tokens(temp))));
  return 0;
}
