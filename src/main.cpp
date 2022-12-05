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
#include <functional>


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
  auto operator<=>(const Symbol&) const = default;
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
Symbol eval_primitive_node(Symbol node);
Symbol eval(Symbol node);


int main() {
  std::string temp;
  std::getline(std::cin, temp);
  Symbol ast = get_ast(get_tokens(temp));
  rec_print_ast(ast);
  std::cout << "\n";
  Symbol result = eval(ast);
  rec_print_ast(result);
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

// static std::vector<std::string> operators = {
//   "+", "-", "*", "/", "define", 
// };

// this will be used to store variables
std::map<std::string, _Type> variables;

using fnsig = std::function<Symbol(std::vector<Symbol>)>;
static std::map<std::string, fnsig> procedures = {
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
    "define",
    [](std::vector<Symbol> args) -> Symbol {
      if (args.size() > 2)
	throw std::logic_error{
	  "Wrong amount of arguments to 'define'!\n"
	};
      if (args[0].type != Type::Identifier)
	throw std::logic_error{
	  "First argument to 'define' must be an identifier!\n"
	};
      variables.insert_or_assign(std::get<std::string>(args[0].value),
				 args[1].value);
      return args[1];
    }
  }
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
	node.name = "root";
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
      } else if (procedures.contains(tok)) {
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
  std::cout << std::boolalpha;
  if (root.type == Type::List) {
    std::cout << "[ ";
    for (auto s: std::get<std::list<Symbol>>(root.value)) {
      if (s.type == Type::List) {
	rec_print_ast(s);
      }
      else {
	std::visit([]<class T>(T&& v) -> void {
	    if constexpr (std::is_same_v<std::decay_t<T>, std::monostate>) {}
	    else if constexpr (std::is_same_v<std::decay_t<T>, std::list<Symbol>>) {}
	    else std::cout << v << " ";
	  }, s.value);
      }
    }
    std::cout << "]";
  } else {
    std::visit([]<class T>(T&& v) -> void {
	if constexpr (std::is_same_v<std::decay_t<T>, std::monostate>) {}
	else if constexpr (std::is_same_v<std::decay_t<T>, std::list<Symbol>>) {}
	else std::cout << v << " ";
      }, root.value);
  }
}


// to use with nodes with only leaf children.
Symbol eval_primitive_node(Symbol node) {
  std::vector<Symbol> intermediate_results;
  Symbol result;
  for (auto e: std::get<std::list<Symbol>>(node.value)) {
    intermediate_results.push_back(e);
  }
  if (intermediate_results[0].type == Type::Operator) {
    Symbol op = intermediate_results[0];
    intermediate_results.erase(intermediate_results.begin());
    if (!procedures.contains(std::get<std::string>(op.value)))
      throw std::logic_error{"Unbound procedure!\n"};
    result = procedures[std::get<std::string>(op.value)](intermediate_results);
    return result;
  } else {
    // it's a list, not a function call.
    return node;
  }
}

Symbol eval(Symbol root) {
  Symbol result;
  std::stack<Symbol> node_stk;
  Symbol current_node;
  // results of intermediate nodes (i.e. nodes below the root)
  std::vector<Symbol> intermediate_results;
  // this is the data on which the actual computation takes place,
  // as we copy every intermediate result we get into this as a "leaf"
  // to get compute the value for each node, including the root.
  std::vector<std::vector<Symbol>> leaves;
  current_node = root;
  do {
    // for each node, visit each child and backtrack to the last parent node
    // when the last child is null, and continue with the second last node and so on
    if (current_node.type == Type::List) {
      if (std::get<std::list<Symbol>>(current_node.value).empty()) {
	// if we're back to the root node, and we don't have any
	// children left, we're done.
	if (leaves.empty() && (current_node.name == "root")) break;
	Symbol eval_temp_arg;
	// insert the intermediate results gotten so far into the leaves, so we
	// can compute the value for the current node.
	if (leaves.size() == 1) {
	  // we reached the top node, compute the final result:
	  if (!intermediate_results.empty()) {
	    leaves[leaves.size() - 1]
	      .insert(leaves[leaves.size() - 1].end(),
		      intermediate_results.begin(),
		      intermediate_results.end());
	  }
	}
	eval_temp_arg = Symbol(
			       "",
			       std::list(leaves[leaves.size() - 1].begin(),
					 leaves[leaves.size() - 1].end()),
			       Type::List
			       );
	result = eval_primitive_node(eval_temp_arg);
        intermediate_results.push_back(result);
        if (!node_stk.empty()) {
	  current_node = node_stk.top();
	  node_stk.pop();
	}
	leaves.pop_back();
      }
      else {
	Symbol child = std::get<std::list<Symbol>>(current_node.value).back();
	auto templ = std::get<std::list<Symbol>>(current_node.value);
	templ.pop_back();
	current_node.value = templ;
	node_stk.push(current_node);
	current_node = child;
	if ((child.type == Type::List) || leaves.empty())
	  leaves.push_back(std::vector<Symbol>{});
      }
    } else {
      if (!leaves.empty())
	leaves[leaves.size() - 1].push_back(current_node);
      else
	leaves.push_back(std::vector<Symbol>{current_node});
      current_node = node_stk.top();
      node_stk.pop();
    }
  } while (1);
  return result;
}

