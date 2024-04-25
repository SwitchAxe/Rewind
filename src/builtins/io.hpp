#pragma once
#include "../include.hpp"
#include <unistd.h>
#include <termios.h>

termios original;  // this will contain the "cooked" terminal mode
termios immediate; // raw terminal mode

std::map<std::string, Functor> io = {
  std::pair{"print", Functor{[](std::list<Symbol> args, path PATH) -> Symbol {
    for (auto e : args) {
      if (e.type == Type::Defunc)
	continue;
      std::cout << rec_print_ast(e);
    }
    return Symbol("", false, Type::Command);
  }}},
  std::pair{"flush", Functor{[](std::list<Symbol> args) -> Symbol {
    std::flush(std::cout);
    return Symbol("", false, Type::Command);
  }}},
  std::pair{"read", Functor{[](std::list<Symbol> args) -> Symbol {
    if (args.size() > 0) {
      throw std::logic_error{
        "The 'read' utility expects no arguments!\n"};
    }
    signed long long int s = 0;
    unsigned long long int us = 0;
    Symbol ret;
    std::string in;
    std::getline(std::cin, in);
    if ((in == "true") || (in == "false")) {
      ret.type = Type::Boolean;
      ret.value = (in == "true") ? true : false;
      ret.name = "";
    } else if (auto [ptr, ec] =
      std::from_chars(in.data(), in.data() + in.size(), s);
                  ec == std::errc()) {
      ret.value = s;
      ret.type = Type::Number;
      ret.name = "";
    } else if (auto [ptr, ec] =
      std::from_chars(in.data(), in.data() + in.size(), us);
                  ec == std::errc()) {
      ret.value = us;
      ret.type = Type::Number;
      ret.name = "";
    } else {
      ret.type = Type::String;
      ret.value = in;
      ret.name = "";
    }
    return ret;
  }}},
  std::pair{"rawmode", Functor{[](std::list<Symbol> args) -> Symbol {
    tcsetattr(STDIN_FILENO, TCSANOW, &immediate);
    return Symbol("", false, Type::Command);
  }}},
  std::pair{"cookedmode", Functor{[](std::list<Symbol> args) -> Symbol {
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
    return Symbol("", false, Type::Command);
  }}},
  std::pair{"readch", Functor{[](std::list<Symbol> args) -> Symbol {
    // read the character
    int ch;
    int tmp[1];
    if (read(STDIN_FILENO, tmp, 1) == -1)
      throw std::logic_error{"Read error!\n"};
    ch = tmp[0];
    // restore the normal terminal mode
    auto ret = Symbol("", std::string{static_cast<char>(ch)}, Type::String);
    return ret;
  }}},
};
