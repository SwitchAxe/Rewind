#pragma once
#include "../include.hpp"

std::map<std::string, Functor> misc = {
  std::pair{"tokens", Functor{[](std::list<Symbol> args) -> Symbol {
    // returns a list of tokens from a single string given as argument,
    // as if it went throught the ordinary lexing of some Rewind input
    // (because this is exactly what we're doing here)
    if (args.size() != 1) {
      throw std::logic_error{
        "The 'tokens' function only accepts a single string!\n"};
    }
    if (args.front().type != Type::String) {
      throw std::logic_error{
        "The 'tokens' function only accepts a single string!\n"};
    }
    std::vector<Token> tks =
      get_tokens(std::get<std::string>(args.front().value));
    auto ret = std::list<Symbol>();
    for (auto tk : tks) {
      ret.push_back(Symbol("", tk.tk, Type::String));
    }
    return Symbol("", ret, Type::List, true);
  }}},
  std::pair{"ast", Functor{[](std::list<Symbol> args) -> Symbol {
    if (args.size() != 1)
      throw std::logic_error {"The 'ast' function accepts a single list of tokens!\n"};
    if ((args.front().type != Type::List) && (args.front().type != Type::ListLiteral))
      throw std::logic_error {"The 'ast' function accepts a single list of tokens!\n"};
    auto l = std::get<std::list<Symbol>>(args.front().value);
    std::vector<Token> tks;
    for (auto x : l) {
      tks.push_back(Token {.tk = std::get<std::string>(x.value),
                          .line = x.line });
    }
    try {
      Symbol ast = parse(tks);
      if (ast.type != Type::List) return ast;
      auto l = std::get<std::list<Symbol>>(ast.value);
      if (l.empty()) return ast;
      return l.front();
    } catch (std::logic_error e) {
      return Symbol("", std::list<Symbol>{}, Type::List);
    }
  }}},
  std::pair{"typeof", Functor{[](std::list<Symbol> args) -> Symbol {
    if (args.size() != 1) {
      throw std::logic_error{
        "The 'typeof' procedure expects exactly one symbol!\n"};
    }

    Symbol ast;
    if (args.front().type == Type::RawAst) {
      ast = parse(get_tokens(rec_print_ast(args.front())));
      ast = std::get<std::list<Symbol>>(ast.value).front();
    } else
      ast = args.front();
    switch (ast.type) {
    case Type::List:
      return Symbol("", "list", Type::String);
      break;
    case Type::Number:
      return Symbol("", "number", Type::String);
      break;
    case Type::Identifier:
      return Symbol("", "identifier", Type::String);
      break;
    case Type::String:
      return Symbol("", "string", Type::String);
      break;
    case Type::Boolean:
      return Symbol("", "boolean", Type::String);
      break;
    case Type::Operator:
      return Symbol("", "operator", Type::String);
      break;
    case Type::Error:
      return Symbol("", "error", Type::String);
    default:
      return Symbol("", "undefined", Type::String);
    }
  }}},
  std::pair{"return", Functor{[](std::list<Symbol> args) {
    if (args.size() != 1)
      throw std::logic_error {"The 'return' builtin expects one argument!\n"};
    return args.front();
  }}},
  std::pair{"defined", Functor{[](std::list<Symbol> args, path PATH, variables vars) -> Symbol {
    if (args.size() != 1) {
      throw std::logic_error{"the 'defined' boolean procedure expects "
                             "exactly one name to look up!\n"};
    }
    Symbol maybe_eval;
    std::string name;
    if ((args.front().type != Type::String) &&
        (args.front().type != Type::Identifier)) {
      if (args.front().type == Type::List) {
        maybe_eval = eval(args.front(), PATH, vars);
        if ((maybe_eval.type != Type::String) &&
            (maybe_eval.type != Type::Identifier))
          throw std::logic_error {"Exception in 'defined':\n"
                                  "expected a string!\n"};
        name = std::get<std::string>(maybe_eval.value);
      } else
        throw std::logic_error{"the 'defined' boolean procedure expects an "
                               "identifier or a string!\n"};
    } else name = std::get<std::string>(args.front().value);
    if (constants.contains(name)) return Symbol("", true, Type::Boolean);
    if (vars.contains(name)) return Symbol("", true, Type::Boolean);
    return Symbol("", false, Type::Boolean);
  }}},
  std::pair{"let", Functor{[](std::list<Symbol> args, path PATH, variables& vars) -> Symbol {
    Symbol global = args.front();
    args.pop_front();
    Symbol id = args.front();
    auto s = std::get<std::string>(id.value);
    args.pop_front();
    Symbol result = eval(args.front(), PATH, vars);
    if (std::get<bool>(global.value)) {
      constants.insert(std::pair{s, result});
    } else
      vars.insert(std::pair{s, result});
    return Symbol("", true, Type::Command);
  }}}
};
