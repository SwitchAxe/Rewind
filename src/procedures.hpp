#pragma once
#include "types.hpp"
#include <map>
#include <exception>
#include <stdexcept>
#include <utility>
#include <iostream>

std::vector<std::map<std::string, Symbol>> variables;

std::vector<std::map<std::string, std::pair<Symbol, Symbol>>>
user_defined_procedures;

std::map<std::string, fnsig> procedures = {
  {
    "+",
    [](std::vector<Symbol> args) -> Symbol {
      int r = 0;
      for (auto e: args) {
	if (e.type == Type::Defunc) continue;
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
	if (args[i].type == Type::Defunc) continue;
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
	if (args[i].type == Type::Defunc) continue;
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
	if (args[i].type == Type::Defunc) continue;
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
      if (args[0].type != Type::Identifier)
	throw std::logic_error{
	  "First argument to 'let' must be an identifier!\n"
	};
      if (args.size() > 2) {
	// function definition
	std::string name = std::get<std::string>(args[0].value);
	args.erase(args.begin());
	Symbol arguments = args[0];
	args.erase(args.begin());
	std::list<Symbol> stmtsl = std::list<Symbol>(
						    args.begin(),
						    args.end());
	Symbol stmts = Symbol("", stmtsl, Type::List);
        if (user_defined_procedures.empty()) {
	  user_defined_procedures
	    .push_back(std::map<std::string, std::pair<Symbol, Symbol>>{
		{name, std::make_pair(arguments, stmts)}
	      });
	} else {
	  user_defined_procedures[user_defined_procedures.size() - 1]
	    .insert_or_assign(name, std::make_pair(arguments, stmts));
	}
	return Symbol("", true, Type::Defunc);
      }
      if (variables.empty()) {
	variables.push_back(std::map<std::string, Symbol>());
      }
      variables[variables.size() - 1]
	.insert_or_assign(std::get<std::string>(args[0].value),
			  args[1]);
      return args[1];
    }
  }
};
