#pragma once
#include "../include.hpp"

std::map<std::string, Functor> code = {
  std::pair{"load", Functor{[](std::list<Symbol> args, path PATH, variables& vars) -> Symbol {
    Symbol last_evaluated;
    Symbol last_expr;
    for (auto e : args) {
      if ((e.type != Type::Identifier) && (e.type != Type::String)) {
        throw std::logic_error{"Arguments to 'load' must be either string "
                               "literals or barewords!"};
      }
      std::string filename = std::get<std::string>(e.value);

      std::vector<Token> tks = get_tokens(rewind_read_file(filename));
      Symbol ast;
      try {
	ast = parse(tks);
      } catch (std::logic_error ex) { throw std::logic_error {"(file " +
                                                              filename + ")" + ex.what() }; }
      for (auto x : std::get<std::list<Symbol>>(ast.value))
            try {
              last_evaluated = eval(x, PATH, vars, x.line);
            } catch (std::logic_error ex) {
              throw std::logic_error {"(file " + filename + ")\n" + ex.what()};
            }
    }
    return last_evaluated;
  }}},

  std::pair{"eval", Functor{[](std::list<Symbol> args, path PATH) -> Symbol {
    if ((args.size() != 1) || (args.front().type != Type::String)) {
      throw std::logic_error{
        "'eval' expects exactly one string to evaluate!\n"};
    }
    Symbol last_evaluated;
    std::string line = std::get<std::string>(args.front().value);
    if ((line == "exit") || (line == "(exit)")) {
      exit(EXIT_SUCCESS);
      return Symbol("", false, Type::Command);
    }
    Symbol ast;
    variables vars = constants;
    try {
      ast = parse(get_tokens(line));
      ast = std::get<std::list<Symbol>>(ast.value).front();
      last_evaluated = eval(ast, PATH, vars, ast.line);
      if ((last_evaluated.type != Type::Command) &&
          (last_evaluated.type != Type::CommandResult))
        std::cout << rec_print_ast(last_evaluated);
    } catch (std::logic_error ex) {
      return Symbol("", ex.what(), Type::Error);
    }
    return last_evaluated;
  }}},
};
