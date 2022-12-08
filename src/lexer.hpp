#pragma once
#include <exception>
#include <vector>
#include <cctype>
#include "types.hpp"
#include "procedures.hpp"

std::vector<std::string>
get_tokens(std::string stream) {
  bool in_identifier = false;
  bool in_string = false;
  bool in_numlit = false;
  bool crossed_lparen = false;
  bool crossed_rparen = false;
  std::vector<std::string> tokens;
  std::vector<char> special_tokens = {'[', ']', '(', ')'};
  std::string temp;
  for (auto ch: stream) {
    if ((ch == ' ') || (ch == '\t')) {
      if (in_identifier || in_numlit) {
	in_identifier = false;
	in_numlit = false;
	tokens.push_back(temp);
	temp = "";
      }
      else if (in_string) {
	temp += ch;
      }
    }
    else if (isgraph(ch)) {
      if (ch == '"') {
	if (in_string) {
	  in_string = false;
	  tokens.push_back(temp + std::string{ch});
	  temp = "";
	}
	else if (in_numlit) {
	  throw std::logic_error{"Failed to parse the input stream!\n" 
				 "Found double quotes in a numeric literal!\n"};
	}
	else if (in_identifier) {
	  throw std::logic_error{"Failed to parse the input stream!\n" 
				 "Found double quotes in an identifier (illegal character)!\n"};
	}
	else {
	  in_string = true;
	  temp += ch;
	}
      }
      else if (in_identifier) {
        if (std::find(special_tokens.begin(),
		      special_tokens.end(),
		      ch) != special_tokens.end()) {
	  in_identifier = false;
	  tokens.push_back(temp);
	  temp = "";
	  tokens.push_back(std::string{ch});
	} else {
	  temp += ch;
	}
      }
      else if (in_numlit) {
	if (!isdigit(ch))
	  throw std::logic_error{"Failed to parse the input stream!\n" 
				 "Found a character in a numeric literal!\n"};
	temp += ch;
      }
      else if (in_string) {
	temp += ch;
      }
      else {
	if (std::find(special_tokens.begin(),
		      special_tokens.end(),
		      ch) != special_tokens.end()) {
	  tokens.push_back(std::string{ch});
	} else {
	  in_identifier = true;
	  temp += ch;
	}
      }
    }
  }
  if (in_string) {
    throw std::logic_error{
      "Unclosed string!\n"
    };
  }
  return tokens;
}
