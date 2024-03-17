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
#include "types.hpp"
#include <cctype>
#include <exception>
#include <ios>
#include <stdexcept>
#include <vector>
#include <charconv>
#include <sstream>
std::string process_escapes(const std::string &s) {
  std::string r;
  for (int i = 0; i < s.length(); ++i) {
    if (!(s[i] == '\\')) {
      r += s[i];
      continue;
    }
    switch (s[i + 1]) {
    case 'n':
      r += '\n';
      break;
    case '\\':
      r += '\\';
      break;
    case 'a':
      r += '\a';
      break;
    case 'b':
      r += '\b';
      break;
    case 'v':
      r += '\v';
      break;
    case 'f':
      r += '\f';
      break;
    case 'r':
      r += '\r';
      break;
    case 'e':
      r += '\x1b';
      break;
    case 't':
      r += '\t';
      break;
    case '0': {
      // 3 digits of octal numerical code must follow
      std::stringstream ss{s.substr(i + 2, 3)};
      std::stringstream as_oct;
      as_oct << std::oct << ss.str();
      int n;
      auto [ptr, ec] = std::from_chars(
          as_oct.str().data(), as_oct.str().data() + as_oct.str().size(), n);
      r += static_cast<char>(n);
      i += 3;
      break;
    }
    default:
      r += s[i];
      r += s[i + 1];
      break;
    }
    i++;
  }
  return r;
}

struct Token {
  std::string tk;
  int line;
};

std::vector<Token> get_tokens(std::string stream) {
  bool in_identifier = false;
  bool in_numlit = false;
  bool crossed_lparen = false;
  bool crossed_rparen = false;
  std::vector<Token> tokens;
  std::vector<char> special_tokens = {
    '[', ']',
    '(', ')',
    ',', ';',
    '{', '}',
    '|',
  };
  std::string temp;
  int line = 0;
  for (int i = 0; i < stream.length(); ++i) {
    if (stream[i] == '\n') {
      in_identifier = false;
      in_numlit = false;
      if (temp != "")
	tokens.push_back(Token{.tk = temp, .line = line});
      temp = "";
      line++;
    }
    if ((stream[i] == ' ') || (stream[i] == '\t')) {
      if (temp != "")
        tokens.push_back(Token{.tk = temp, .line = line});
      in_identifier = false;
      in_numlit = false;
      temp = "";
    } else if (isgraph(stream[i])) {
      if (auto s = std::string{stream[i], stream[i+1]}; (s == "'(") || (s == "'[")) {
	if (temp != "") {
	  tokens.push_back(Token{.tk = std::string{temp}, .line = line});
	  temp = "";
	}
	tokens.push_back(Token{.tk = s, .line = line});
	i++;
      } else if (std::find(special_tokens.begin(), special_tokens.end(),
                    stream[i]) != special_tokens.end()){
	if (temp != "") {
	  tokens.push_back(Token{.tk = std::string{temp}, .line = line});
	  temp = "";
	}
	tokens.push_back(Token{.tk = std::string{stream[i]}, .line = line});
      }
      else if ((stream[i] == '"') || (stream[i] == '\'')) {
        if (in_numlit) {
          throw std::logic_error{"Failed to parse the input stream!\n"
                                 "Found double quotes in a numeric literal!\n"};
        } else if (in_identifier) {
          throw std::logic_error{
              "Failed to parse the input stream!\n"
              "Found double quotes in an identifier (illegal character)!\n"};
        } else {
          in_identifier = false;
          in_numlit = false;
          int e = 0;
          for (e = i + 1; (e < stream.length()) &&
                          (stream[e] != (stream[i] == '"' ? '"' : '\''));
               ++e) {
            if (stream[e] == '\\') {
              e++;
            }
          }
          std::string tmp = stream.substr(i, e - i + 1);
          i = e;
          tokens.push_back(Token{.tk = tmp, .line = line});
          tmp = "";
          temp = "";
        }
      } else if (in_identifier) {
        temp += stream[i];
      } else if (in_numlit) {
        if (!isdigit(stream[i]))
          throw std::logic_error{"Failed to parse the input stream!\n"
                                 "Found a character in a numeric literal!\n"};
        temp += stream[i];
      } else {
        in_identifier = true;
        temp += stream[i];
      }
    }
  }
  if (temp != "") {
    tokens.push_back(Token{.tk = temp, .line = line});
  }
  for (auto &tk : tokens) {
    if (tk.tk[0] == '"') {
      tk = Token {.tk = process_escapes(tk.tk), .line = tk.line};
    }
  }
  return tokens;
}
