#pragma once
#include "types.hpp"
#include <map>
#include <exception>
#include <stdexcept>
std::map<std::string, Symbol> variables;

std::map<std::string, fnsig> procedures = {
  {
    "+",
    [](std::vector<Symbol> args) -> Symbol {
      int r = 0;
      for (auto e: args) {
	if (e.type != Type::Number) {
	  throw std::logic_error {
	    "Unexpected operand to the '+' procedure!\n"
	  };
	}
	r += std::get<int>(e.value);
      }
      Symbol ret("", r, Type::Number);
      return ret;
    }
  },
  {
    "-",
    [](std::vector<Symbol> args) -> Symbol {
      int r;
      if (!std::holds_alternative<int>(args[0].value)) {
	throw std::logic_error {
	  "Unexpected operand to the '-' procedure!\n"
	};
      }
      r = std::get<int>(args[0].value);
      for (int i = 1; i < args.size(); ++i) {
	if (args[i].type != Type::Number) {
	  throw std::logic_error{
	    "Unexpected operand to the '-' procedure!\n"
	  };
	}
	r -= std::get<int>(args[i].value);
      }
      Symbol ret("", r, Type::Number);
      return ret;
    }
  },
  {
    "/",
    [](std::vector<Symbol> args) -> Symbol {
      int r;
      if (!std::holds_alternative<int>(args[0].value)) {
	throw std::logic_error {
	  "Unexpected operand to the '/' procedure!\n"
	};
      }
      r = std::get<int>(args[0].value);
      for (int i = 1; i < args.size(); ++i) {
	if (args[i].type != Type::Number) {
	  throw std::logic_error{
	    "Unexpected operand to the '/' procedure!\n"
	  };
	}
	r /= std::get<int>(args[i].value);
      }
      Symbol ret("", r, Type::Number);
      return ret;
    }
  },
  {
    "*",
    [](std::vector<Symbol> args) -> Symbol {
      int r;
      if (!std::holds_alternative<int>(args[0].value)) {
	throw std::logic_error {
	  "Unexpected operand to the '*' procedure!\n"
	};
      }
      r = std::get<int>(args[0].value);
      for (int i = 1; i < args.size(); ++i) {
	if (args[i].type != Type::Number) {
	  throw std::logic_error{
	    "Unexpected operand to the '*' procedure!\n"
	  };
	}
	r *= std::get<int>(args[i].value);
      }
      Symbol ret("", r, Type::Number);
      return ret;
    }
  },
  {
    "let",
    [](std::vector<Symbol> args) -> Symbol {
      if (args.size() > 2)
	throw std::logic_error{
	  "Wrong amount of arguments to 'define'!\n"
	};
      if (args[0].type != Type::Identifier)
	throw std::logic_error{
	  "First argument to 'let' must be an identifier!\n"
	};
      variables.insert_or_assign(std::get<std::string>(args[0].value),
				 args[1]);
      return args[1];
    }
  }
};
