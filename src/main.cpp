#include <iostream>
#include <vector>
#include <array>
#include <utility>
#include <map>
#include <string>
#include <variant>
#include <list>
#include <cctype>
#include <exception>
#include <algorithm>
#include <stack>
#include <charconv>
#include <type_traits>


enum Type {
  String,
  Number,
  Boolean,
  List,
  Operator,
  Identifier,
};
struct Symbol;

using _Type = std::variant<std::monostate,
			   int, std::string,
			   std::list<Symbol>,
			   bool>;
struct Symbol {
  Symbol() = default;
  Symbol(std::string _n, _Type _v, Type _t) {
    name = _n;
    value = _v;
    type = _t;
  }

  std::string name; // empty string if not present
  _Type value;
  Type type;
};

std::vector<std::string> get_tokens(std::string stream);
Symbol get_ast(std::vector<std::string> tokens);
void rec_print_ast(Symbol root);

int main() {
  std::string temp;
  std::getline(std::cin, temp);
  Symbol ast = get_ast(get_tokens(temp));
  rec_print_ast(ast);
  return 0;
}

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

static std::array<std::string, 4> operators = {
  "+", "-", "*", "/",
};

Symbol get_ast(std::vector<std::string> tokens) {
  int round_balance = 0; // for paren balancing
  int square_balance = 0; // see above
  bool list_entered = false;
  Symbol node;
  std::stack<Symbol> node_stk;
  for (auto tok: tokens) {
    if ((tok == "(") || (tok == "[")) {
      if (tok == "(") round_balance++;
      else square_balance++;
      if (std::holds_alternative
	  <std::monostate>(node.value)) {
	node.type = Type::List;
	node.value = std::list<Symbol>();
      }
      else {
	if (!std::holds_alternative
	    <std::list<Symbol>>(node.value)) {
	  throw std::logic_error{
	    "Failed to parse the input expression!"
	    "Unexpected list...\n"
	  };
	} else {
	  // append a new list at the end and update
	  // the current node 'node'.
	  node_stk.push(node);
	  node.value = std::list<Symbol>();
	}
      }
    } else if ((tok == ")") || (tok == "]")) {
      if (tok == ")") round_balance--;
      else square_balance--;
      // we finished parsing the subexpression/sublist, so we can
      // append the resulting tree to the previous one.
      Symbol child = node;
      if (!node_stk.empty()) {
	node = node_stk.top();
	node_stk.pop();
	if (node.type != Type::List) {
	  throw std::logic_error{
	    "I have honestly no clue of how you got this error.\n"
	  };
	}
	auto temp = std::get<std::list<Symbol>>(node.value);
	temp.push_front(child);
        node.value = temp;
      } else return node;
    } else {
      // identifiers, operators, literals and so on
      Symbol child;
      int v;
      if (auto [ptr, ec] = std::from_chars(tok.data(),
					   tok.data() + tok.size(),
					   v); ptr == tok.data() + tok.size()) {
	// a number!
        child.type = Type::Number;
        child.value = v;
      } else if (std::find(operators.begin(),
			   operators.end(),
			   tok) != operators.end()) {
	child.type = Type::Operator;
	child.value = tok;
      } else if (tok[0] == '"') {
	child.type = Type::String;
	child.value = tok;
      } else if ((tok == "false") || (tok == "true")) {
	child.type = Type::Boolean;
	child.value = (tok == "false") ? false : true;
      } else {
	child.type = Type::Identifier;
	child.value = tok;
      }
      if (std::holds_alternative<std::list<Symbol>>(node.value)) {
	auto temp = std::get<std::list<Symbol>>(node.value);
	temp.push_front(child);
	node.value = temp;
      }
      else {
	node = child;
      }
    }
  }
  if (round_balance) {
    throw std::logic_error{"Missing closing parenthesis for an expression!\n"};
  }
  if (square_balance) {
    throw std::logic_error{"Missing closing square bracket for a list!\n"};
  }

  return node;
}

void rec_print_ast(Symbol root) {
  std::cout << "[ ";
  for (auto s: std::get<std::list<Symbol>>(root.value)) {
    if (s.type == Type::List) {
      rec_print_ast(s);
    } else {
      std::visit([]<class T>(T&& v) -> void {
	  if constexpr (std::is_same_v<std::decay_t<T>, std::monostate>) {}
	  else if constexpr (std::is_same_v<std::decay_t<T>, std::list<Symbol>>) {}
	  else std::cout << v << " ";
	}, s.value);
    }
  }
  std::cout << " ]";
}
