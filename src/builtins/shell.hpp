#include "../include.hpp"

std::map<std::string, Functor> shell = {
  std::pair{"cd", Functor{[](std::list<Symbol> args) -> Symbol {
    if ((args.size() != 1) || ((args.front().type != Type::Identifier) &&
                               (args.front().type != Type::String)))
      throw std::logic_error{
        "The 'cd' builtin command expects precisely one path!"};
    auto cur = fs::current_path();
    if (cur.is_absolute()) {
      fs::current_path(
		       std::string{std::get<std::string>(args.front().value).c_str()});
    } else if (fs::directory_entry(std::string{cur.c_str()} + "/" +
                                   std::get<std::string>(args.front().value))
                      .exists()) {
      fs::current_path(std::string{cur.c_str()} + "/" +
                       std::get<std::string>(args.front().value));
    } else {
      throw std::logic_error{"Unknown path!\n"};
    }
    setenv("PWD", std::string{fs::current_path()}.c_str(), 1);
    return Symbol("", fs::current_path(), Type::Command);
  }}},
  std::pair{"set", Functor{[](std::list<Symbol> args) -> Symbol {
    if (args.size() != 2)
      throw std::logic_error{"The 'set' builtin command expects "
                             "precisely two arguments!\n"};
    std::string var = std::get<std::string>(args.front().value);
    std::string val = std::get<std::string>(args.back().value);
    return Symbol("", setenv(var.c_str(), val.c_str(), 1), Type::Number);
  }}},
  std::pair{"get", Functor{[](std::list<Symbol> args) -> Symbol {
    if (args.size() != 1)
      throw std::logic_error{
        "The 'get' builtin command expects precisely one argument!"};
    char *s = std::getenv(std::get<std::string>(args.front().value).c_str());
    if (s)
      return Symbol("", std::string(s), Type::String);
    return Symbol("", "Nil", Type::String);
  }}},
  std::pair{">", Functor{[](std::list<Symbol> args, path PATH) -> Symbol {
    if (args.size() != 2) {
      throw std::logic_error{
        "Expected exactly two arguments to the '>' operator!\n"};
    }
    if (args.front().type != Type::String) {
      throw std::logic_error{"Invalid first argument to the '>' operator!\n"
                             "Expected a string!.\n"};
    }
    auto contents = std::get<std::string>(args.front().value);

    args.pop_front();
    if (args.front().type != Type::Identifier) {
      throw std::logic_error{"Invalid second argument to the '>' operator!\n"
                             "Expected a file name.\n"};
    }
    std::ofstream out(std::get<std::string>(args.front().value));
    auto backup = std::cout.rdbuf();
    std::cout.rdbuf(out.rdbuf());
    std::cout << contents;
    std::cout.rdbuf(backup);
    return Symbol("", true, Type::Command);
  }}},
  std::pair{">>", Functor{[](std::list<Symbol> args) -> Symbol {
    if (args.size() != 2) {
      throw std::logic_error{
        "Expected exactly two arguments to the '>>' operator!\n"};
    }
    if (args.front().type != Type::String) {
      throw std::logic_error{"Invalid first argument to the '>>' operator!\n"
                             "Expected a string!.\n"};
    }
    auto contents = std::get<std::string>(args.front().value);

    args.pop_front();
    if (args.front().type != Type::Identifier) {
      throw std::logic_error{"Invalid second argument to the '>' operator!\n"
                             "Expected a file name.\n"};
    }
    std::ofstream out(std::get<std::string>(args.front().value),
                      std::ios::app);
    auto backup = std::cout.rdbuf();
    std::cout.rdbuf(out.rdbuf());
    std::cout << contents;
    std::cout.rdbuf(backup);
    return Symbol("", true, Type::Command);
  }}}
};
