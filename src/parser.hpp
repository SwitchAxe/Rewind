#pragma once
// Copyright 2023 Sofia Cerasuoli (@SwitchAxe)
/*
  This file is part of Rewind.
  Rewind is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation, either version 3 of the license, or (at your
  option) any later version.
  Rewind is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  Rewind. If not, see <https://www.gnu.org/licenses/>.
*/
#include "lexer.hpp"
#include "types.hpp"
#include <charconv>
#include <iostream>
#include <stack>
#include <type_traits>
#include <optional>


template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;


std::string rec_print_ast(Symbol root, bool debug);

bool is_strlit(std::string s) {
  return (s.size() > 1) && (((s[0] == '\'') && (s.back() == '\'')) ||
                            ((s[0] == '"') && (s.back() == '"')));
}

std::optional<long long signed int> try_convert_num(std::string n) {
  long long int v;
  auto [ptr, ec] = std::from_chars(n.data(), n.data() + n.size(), v);
  if (ptr == n.data() + n.size()) {
    return v;
  }
  return std::nullopt;
}

// information we need to keep track of in get_ast_aux
struct RecInfo {
  Symbol result;
  int end_index;
  int line;
};

std::string format_line(int l) { return "(line " + std::to_string(l) + ")"; }

RecInfo dispatch_parse(std::vector<Token> tokens, int si);

Symbol parse_identifier(std::string tk) {
  return Symbol("", tk, Type::Identifier);
}

Symbol parse_number(signed long long int n) {
  return Symbol("", n, Type::Number);
}

Symbol parse_strlit(std::string tk) {
  return Symbol("", tk.substr(1, tk.size() - 2), Type::String);
}

Symbol parse_bool(std::string tk) {
  return Symbol("", tk == "true" ? true : false, Type::Boolean);
}

RecInfo parse_list_expr(std::vector<Token> tokens, int si);
RecInfo parse_list(std::vector<Token> tks, int i);

RecInfo parse_list_literal(std::vector<Token> tokens, int si) {
  // parses a (possibly recursive) list from start to finish, and then
  // returns.
  std::list<Symbol> ret;
  int i = 0;
  for (i = si+1; i < tokens.size(); ++i) {
    auto tok = tokens[i];
    auto tk = tok.tk;
    if ((tk == "(") || (tk == "[")) {
      auto got = parse_list_expr(tokens, i);
      ret.push_back(got.result);
      i = got.end_index;
    } else if ((tk == ")") || (tk == "]")) {
      return RecInfo {
	.result = Symbol("", ret, Type::List),
	.end_index = i,
	.line = tok.line };
    } else if ((tk == "'(") || (tk == "'[")) {
      auto got = parse_list_literal(tokens, i);
      ret.push_back(got.result);
      i = got.end_index;
    } else {
      auto got = dispatch_parse(std::vector<Token>{tokens[i]}, 0);
      ret.push_back(got.result);
    }
  }
  throw std::logic_error {format_line(tokens[i-1].line) +
			  " Unclosed list!\n"};
}

RecInfo parse_list_expr(std::vector<Token> tokens, int si) {
  auto got = parse_list_literal(tokens, si);
  auto l = std::get<std::list<Symbol>>(got.result.value);
  if (l.empty())
    throw std::logic_error {"Empty function call!\n"};
  auto op = l.front();
  if (op.type != Type::Identifier)
    throw std::logic_error {"Invalid operator in a list-expression!\n"};
  l.pop_front();
  op.type = Type::Operator;
  l.push_front(op);
  got.result.value = l;
  return got;
}

RecInfo parse_block_function(std::vector<Token> tokens, int si) {
  std::list<Symbol> body;
  int i = 0;
  for (int i = si + 1; i < tokens.size(); ++i) {
    auto tk = tokens[i];
    if (tk.tk == "}")
      return RecInfo {
	.result = Symbol("", body, Type::List),
	.end_index = i,
	.line = tk.line };
    auto got = dispatch_parse(tokens, i);
    body.push_back(got.result);
    i = got.end_index;
  }
  throw std::logic_error {format_line(tokens[si-1].line) +
			  " Unclosed '}' in a function definition!\n"};
}

RecInfo parse_function_call(std::vector<Token> tokens, int si) {
  std::list<Symbol> fcall;

  if (tokens.size() == 1) {
    fcall.push_back(Symbol("", tokens[si].tk, Type::Operator));
    auto sym = Symbol("", fcall, Type::List);
    return RecInfo {.result = sym, .end_index = si, .line = tokens[si].line};
  }
  int i = 0;
  for (i = si; i < tokens.size(); ++i) {
    Token tk = tokens[i];
    if (tk.tk == ";") {
      if (fcall.size() > 0) {
	auto op = fcall.front();
	fcall.pop_front();
	op.type = Type::Operator;
	fcall.push_front(op);
      }
      return RecInfo {
	.result = Symbol("", fcall, Type::List),
	.end_index = i,
	.line = tk.line };
    }

    if ((tk.tk == "(") || (tk.tk == "[")) {
      auto got = parse_list(tokens, i);
      fcall.push_back(got.result);
      i = got.end_index;
    } else if ((tk.tk == "'(") || (tk.tk == "'[")) {
      auto got = parse_list_literal(tokens, i);
      fcall.push_back(got.result);
      i = got.end_index;
    } else {
      auto got = dispatch_parse(std::vector<Token>{tk}, 0);
      fcall.push_back(got.result);
    }
  }
  if (i == tokens.size()) {
    if (fcall.size() > 0) {
      auto op = fcall.front();
      fcall.pop_front();
      op.type = Type::Operator;
      fcall.push_front(op);
    }
    return RecInfo {
      .result = Symbol("", fcall, Type::List),
      .end_index = i,
      .line = tokens.back().line };
  }
  throw std::logic_error {format_line(tokens[i].line) +
			  " Missing semicolon ';' after a function call!\n"};
}

RecInfo parse_function_body(std::vector<Token> tokens, int si) {
  if (tokens[si].tk == "{") {
    // block function
    return parse_block_function(tokens, si);
  }
  auto got = dispatch_parse(tokens, si);
  // wrap it inside a list to match the block functions
  return RecInfo {
    .result = Symbol("", std::list<Symbol>{got.result}, Type::List),
    .end_index = got.end_index,
    .line = got.line
  };
}

RecInfo parse_function(std::vector<Token> tokens, int si) {
  // (args...) => statements...
  std::list<Symbol> f;
  RecInfo args = parse_list_literal(tokens, si);
  si = args.end_index + 1;
  auto l = std::get<std::list<Symbol>>(args.result.value);
  f.push_back(Symbol("", l, Type::List));
  if (tokens[si].tk != "=>")
    throw std::logic_error {format_line(tokens[si].line) +
			    " Invalid syntax for a function definition:\n"
			    "Missing '=>' after the parameter list!\n"};
  si++;
  RecInfo body = parse_function_body(tokens, si);
  auto v = body.result;
  si = body.end_index;
  f.push_back(v);
  return RecInfo {
    .result = Symbol("", f, Type::Function),
    .end_index = si,
    .line = tokens[si-1].line
  };
}

// this parses either a list expression or a function definition, depending on
// what comes after the list
RecInfo parse_list(std::vector<Token> tks, int i) {
  auto got = parse_list_expr(tks, i);
  auto idx = got.end_index;
  if (tks[idx+1].tk == "=>") {
    // a function.
    return parse_function(tks, i);
  }
  // otherwise, we just parsed a list expression:
  return got;
}

RecInfo parse_let(std::vector<Token> tokens, int si) {
  // let <name> = <any value, also functions>
  si++; // skip the "let" keyword
  Symbol name = parse_identifier(tokens[si].tk);
  si++;
  if (tokens[si].tk != "=")
    throw std::logic_error {format_line(tokens[si].line) +
			   + " Missing '=' in a let-binding!\n"};
  si++;
  RecInfo any_v = dispatch_parse(tokens, si);
  if (any_v.end_index == si)
    if (tokens[si+1].tk != ";")
      throw std::logic_error {"Missing semicolon at the end of a let-binding!\n"};
    else si++;
  else si = any_v.end_index;
  std::list<Symbol> ret = {
    Symbol("", "let", Type::Operator),
    name,
    any_v.result
  };

  return RecInfo {
    .result = Symbol("", ret, Type::List),
    .end_index = si,
    .line = tokens[si].line };
}

RecInfo parse_branch_section(std::vector<Token> tokens, int i, bool expr = false) {
  RecInfo part;
  if ((tokens[i].tk == "(") || (tokens[i].tk == "[")) {
    part = parse_list_expr(tokens, i);
    part.end_index++;
  }
  else if ((tokens[i].tk == "'(") || (tokens[i].tk == "'[")) {
    part = parse_list_literal(tokens, i);
    part.end_index++;
  } else if (tokens[i].tk == "{") {
    part = parse_block_function(tokens, i);
    part.end_index++;
  }
  else {
    // if it's not a list, we assume that it's not a function call!
    part = dispatch_parse(std::vector<Token>{tokens[i]}, 0);
    i++;
    part.end_index = i;
    part.result = Symbol("", std::list<Symbol>{part.result}, Type::List);
    if (expr)
      if ((tokens[i].tk != ",") && (tokens[i].tk != ";"))
	throw std::logic_error {"Missing end-or-branch (',' or ';') token!\n"};
  }
  return part;
}

RecInfo parse_branch(std::vector<Token> tokens, int i) {
  std::list<Symbol> l = {};
  i++; // skip the "|"

  RecInfo cond = parse_branch_section(tokens, i);
  i = cond.end_index;
  
  if (tokens[i].tk != "=>")
    throw std::logic_error {"Missing '=>' token in a branch!\n"};
  i++;
  RecInfo body = parse_branch_section(tokens, i, true);
  i = body.end_index;
  l = {cond.result, body.result};
  return RecInfo {
    .result = Symbol("", l, Type::List),
    .end_index = i,
    .line = tokens[i].line
  };
}

RecInfo parse_match(std::vector<Token> tokens, int i) {
  // we parse a value, and branches afterwards.
  i++; // skip the "match" keyword
  RecInfo matched = dispatch_parse(tokens, i);
  i = matched.end_index;

  std::list<Symbol> l = {
    Symbol("", "match", Type::Operator),
    matched.result
  };
  RecInfo got;
  do {
    got = parse_branch(tokens, i + 1);
    i = got.end_index;
    l.push_back(got.result);
  } while (tokens[i].tk != ";");

  return RecInfo {
    .result = Symbol("", l, Type::List),
    .end_index = i,
    .line = tokens[i].line
  };
}

RecInfo parse_cond(std::vector<Token> tokens, int i) {
  auto l = std::list<Symbol>{Symbol("", "cond", Type::Operator)};
  RecInfo got;
  do {
    got = parse_branch(tokens, i + 1);
    i = got.end_index;
    l.push_back(got.result);
  } while (tokens[i].tk != ";");
  return RecInfo {
    .result = Symbol("", l, Type::List),
    .end_index = i,
    .line = tokens[i].line
  };
}

RecInfo dispatch_parse(std::vector<Token> tks, int i) {
  if (auto n = try_convert_num(tks[i].tk); n != std::nullopt)
    return RecInfo {
      .result = parse_number(*n),
      .end_index = i,
      .line = tks[i].line };
  if (is_strlit(tks[i].tk))
    return RecInfo {
      .result = parse_strlit(tks[i].tk),
      .end_index = i,
      .line = tks[i].line };

  if (tks[i].tk[0] == '@')
    return RecInfo {
      .result = parse_identifier(tks[i].tk.substr(1)),
      .end_index = i,
      .line = tks[i].line};
  
  if (tks.size() == 1)
    return RecInfo {
      .result = parse_identifier(tks[i].tk),
      .end_index = i,
      .line = tks[i].line };
  
  if ((tks[i].tk == "(") || (tks[i].tk == "[")) {
    return parse_list(tks, i);
  }
  if ((tks[i].tk == "'(") || (tks[i].tk == "'[")) {
    return parse_list_literal(tks, i);
  }
  if (tks[i].tk == "let")
    return parse_let(tks, i);
  if (tks[i].tk == "match")
    return parse_match(tks, i);
  if (tks[i].tk == "cond")
    return parse_cond(tks, i);

  if (tks[i].tk == "{")
    return parse_block_function(tks, i);
  
  if (tks.size() > 1)
    return parse_function_call(tks, i);
  throw std::logic_error {"Doesn't reach here!\n"};
}

// .result contains the result of the parsing.
// .end_index is needed if we plan to parse multiple consecutive
// expressions, so that we know where we left off the last time.
RecInfo parse(std::vector<Token> tokens, int i = 0) {
  return dispatch_parse(tokens, i);
}

// DEBUG PURPOSES ONLY and for printing the final result until i
// make an iterative version of this thing

std::string rec_print_ast(Symbol root, bool debug) {
  std::string res;
  if (std::holds_alternative<std::list<Symbol>>(root.value)) {
    if (debug)
      res += std::string{(root.type == Type::RawAst) ? "(Ast)" : "(List) "} +
             " [ ";
    else
      res += "[ ";
    for (auto s : std::get<std::list<Symbol>>(root.value)) {
      res += rec_print_ast(s, debug) + " ";
    }
    res += "]";
  } else {
    std::visit(overloaded{
      [&](std::monostate) -> void {},
      [&](std::string s) -> void {
	res = debug ? "(Str) " + s : s;
      },
      [&](unsigned long long int n) -> void {
	auto s = std::to_string(n);
	res = debug ? "(Num) " + s : s;
      },
      [&](signed long long int n) -> void {
	auto s = std::to_string(n);
	res = debug ? "(Num) " + s : s;
      },
      [&](bool b) -> void {
	std::string s = b ? "true" : "false";
	res = debug ? "(Bool) " + s : s;
      },
      [&](auto) -> void {}
    }, root.value);
  }
  return res;
}
