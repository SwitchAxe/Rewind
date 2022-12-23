#pragma once
#include "types.hpp"
#include "matchit.h"
#include <map>
#include <exception>
#include <stdexcept>
#include <utility>
#include <iostream>
#include <optional>
std::vector<std::map<std::string, Symbol>> variables;
std::vector<std::pair<std::string,
		      std::map<std::string,
			       Symbol>>> call_stack;
std::vector<std::map<std::string, std::pair<Symbol, Symbol>>>
user_defined_procedures;

std::optional<Symbol> variable_lookup(Symbol id) {
  if (!std::holds_alternative<std::string>(id.value))
    return std::nullopt;
  if (variables.empty() ||
      !variables[variables.size() - 1]
      .contains(std::get<std::string>(id.value))) {
    return std::nullopt;
  }
  return std::optional<Symbol>
    {variables
     [variables.size() - 1]
     [std::get<std::string>(id.value)]};
}

std::optional<std::pair<Symbol, Symbol>> procedure_lookup(Symbol id) {
  if (!std::holds_alternative<std::string>(id.value))
    return std::nullopt;
  if (user_defined_procedures.empty() ||
      !user_defined_procedures[user_defined_procedures.size() - 1]
      .contains(std::get<std::string>(id.value))) {
    return std::nullopt;
  }
  return std::optional<std::pair<Symbol, Symbol>>
    {user_defined_procedures
     [user_defined_procedures.size() - 1]
     [std::get<std::string>(id.value)]};
}

std::optional<Symbol> callstack_variable_lookup(Symbol id) {
  if (!std::holds_alternative<std::string>(id.value))
    return std::nullopt;
  if (call_stack.empty() ||
      !call_stack[call_stack.size() - 1]
      .second
      .contains(std::get<std::string>(id.value))) {
    return std::nullopt;
  }
  return std::optional<Symbol> {
    call_stack[call_stack.size() - 1]
    .second[std::get<std::string>(id.value)]
  };
}

bool convert_value_to_bool(Symbol sym) {
  using namespace matchit;
#define p pattern
  auto is_strlit = [](const std::string& s) -> bool {
    return !s.empty() && s[0] == '"' && s[s.length() - 1] == '"';
  };
  using str = std::string;
  using lst = std::list<Symbol>;
  Id<str> s;
  Id<lst> l;
  Id<int> i;
  Id<bool> b;
  auto maybe_id = variable_lookup(sym);
  auto maybe_cs_id = callstack_variable_lookup(sym);
  bool clause;
  if (maybe_cs_id != std::nullopt) {
    clause = convert_value_to_bool(*maybe_cs_id);
  } else if (maybe_id != std::nullopt) {
    clause = convert_value_to_bool(*maybe_id);
  } else
    clause = match (sym.value)
      (p | as<int>(i) = [&] {return *i != 0;},
       p | as<str>(s) = [&] {return (is_strlit(*s) &&
				     ((*s).length() > 2));},
       p | as<bool>(b) = [&] {return *b;},
       p | as<lst>(l) = [&] {return (!(*l).empty());}
       );
#undef p
  return clause;
}



std::map<std::string, fnsig> procedures = {
  {
    "+",
    [](std::list<Symbol> args) -> Symbol {
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
    [](std::list<Symbol> args) -> Symbol {
      int r;
      if (!std::holds_alternative<int>(args.front().value)) {
	throw std::logic_error {
	  "Unexpected operand to the '-' procedure!\n"
	};
      }
      r = std::get<int>(args.front().value);
      args.pop_front();
      for (auto e: args) {
	if (e.type == Type::Defunc) continue;
	if (e.type != Type::Number) {
	  throw std::logic_error{
	    "Unexpected operand to the '-' procedure!\n"
	  };
	}
	r -= std::get<int>(e.value);
      }
      Symbol ret("", r, Type::Number);
      return ret;
    }
  },
  {
    "/",
    [](std::list<Symbol> args) -> Symbol {
      int r;
      if (!std::holds_alternative<int>(args.front().value)) {
	throw std::logic_error {
	  "Unexpected operand to the '/' procedure!\n"
	};
      }
      r = std::get<int>(args.front().value);
      args.pop_front();
      for (auto e: args) {
	if (e.type == Type::Defunc) continue;
	if (e.type != Type::Number) {
	  throw std::logic_error{
	    "Unexpected operand to the '/' procedure!\n"
	  };
	}
	r /= std::get<int>(e.value);
      }
      Symbol ret("", r, Type::Number);
      return ret;
    }
  },
  {
    "*",
    [](std::list<Symbol> args) -> Symbol {
      int r;
      if (!std::holds_alternative<int>(args.front().value)) {
	throw std::logic_error {
	  "Unexpected operand to the '*' procedure!\n"
	};
      }
      r = std::get<int>(args.front().value);
      args.pop_front();
      for (auto e: args) {
	if (e.type == Type::Defunc) continue;
	if (e.type != Type::Number) {
	  throw std::logic_error{
	    "Unexpected operand to the '*' procedure!\n"
	  };
	}
	r *= std::get<int>(e.value);
      }
      Symbol ret("", r, Type::Number);
      return ret;
    }
  },
  {
    "let",
    [](std::list<Symbol> args) -> Symbol {
      if (args.front().type != Type::Identifier)
	throw std::logic_error{
	  "First argument to 'let' must be an identifier!\n"
	};
      if (args.size() > 2) {
	// function definition
	std::string name = std::get<std::string>(args.front().value);
	args.pop_front();
	Symbol arguments = args.front();
        args.pop_front();
	Symbol stmts = Symbol("", args, Type::List);
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
      Symbol id = args.front();
      args.pop_front();
      call_stack.push_back(
			   std::pair<std::string,
			   std::map<std::string, Symbol>> {
			     "let",
			     {
			       {
				 std::get<std::string>(id.value),
				 args.front()
			       }
			     }
			   });
      if (variables.empty()) {
	variables.push_back(std::map<std::string, Symbol>());
      }
      variables[variables.size() - 1]
	.insert_or_assign(std::get<std::string>(id.value),
			  args.front());
      return args.front();
    }
  },
  {
    "if",
    [](std::list<Symbol> args) -> Symbol {
      // (if <clause> (expr1 ... exprn) (else1 ... elsen))
      // if <clause> converts to Cpp's "true" then return
      // (expr1 ... exprn) to the caller, and the other
      // list otherwise.
      if (args.size() != 3) {
	throw std::logic_error {
	  "An if statement must have precisely three arguments!\n"
	};
      }
      bool clause = convert_value_to_bool(args.front());
      args.pop_front();
      if (clause) {
	return args.front();
      }
      args.pop_front();
      return args.front();
    }
  }
};
