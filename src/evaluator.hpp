#pragma once
#include "parser.hpp"
#include "procedures.hpp"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <unistd.h>
#include "external.hpp"
Symbol eval(Symbol root, const std::vector<std::string>& PATH);

Symbol eval_function(Symbol node, const std::vector<std::string>& PATH) {
  Symbol result;
  auto argl = std::get<std::list<Symbol>>(node.value);
  std::string op = std::get<std::string>(argl.front().value);
  argl.pop_front();
  if (!user_defined_procedures[user_defined_procedures.size() - 1]
      .contains(op)) {
    throw std::logic_error{"Unbound procedure " + op + "!\n"};
  }
  Symbol params = user_defined_procedures
    [user_defined_procedures.size() - 1][op]
    .first;
  auto paraml = std::get<std::list<Symbol>>(params.value);
  Symbol body = user_defined_procedures
    [user_defined_procedures.size() - 1][op]
    .second;
  if (argl.size() != paraml.size()) {
    throw std::logic_error {"Wrong amount of arguments to procedure " +
			    op + "!\n"};
  }
  std::map<std::string, Symbol> frame = {};
  size_t paramls = paraml.size();
  for (int i = 0; i < paramls; ++i) {
    frame.insert_or_assign(
			   std::get<std::string>(paraml.front().value),
			   argl.front());
    paraml.pop_front();
    argl.pop_front();
  }
  call_stack.push_back(std::make_pair(op, frame));
  for (auto e: std::get<std::list<Symbol>>(body.value)) {
    result = eval(e, PATH);
  }
  call_stack.pop_back();
  return result;
}

// to use with nodes with only leaf children.
Symbol
eval_primitive_node(Symbol node, const std::vector<std::string>& PATH) {
  Symbol result;
  auto it = PATH.begin();
  Symbol op = std::get<std::list<Symbol>>(node.value).front();
  if (op.type == Type::Operator) {
    auto temp = std::get<std::list<Symbol>>(node.value);
    temp.pop_front();
    node.value = temp;
    if (std::get<std::string>(op.value) == "if") {
      Symbol branch = procedures
	[std::get<std::string>(op.value)]
	(std::get<std::list<Symbol>>(node.value));
      using namespace matchit;
      Id<int> i;
      Id<std::string> s;
      Id<std::list<Symbol>> l;
      Id<bool> b;
      #define p pattern
      if (std::holds_alternative<std::list<Symbol>>(branch.value)) {
	return eval(branch, PATH);
      }
      return branch;
    }
    else if (procedures.contains(std::get<std::string>(op.value))) {
      result = procedures
	[std::get<std::string>(op.value)]
	(std::get<std::list<Symbol>>(node.value));
      return result;
    }
    else
      throw std::logic_error{"Unbound procedure!\n"};
  }
  else if ((builtin_commands.contains(std::get<std::string>(op.value))) ||
	   (it = std::find_if(PATH.begin(), PATH.end(),
			[&](const std::string& entry) -> bool {
			  namespace fs = std::filesystem;
			  std::string full_path;
			  auto prog = std::get<std::string>(op.value);
			  full_path = entry + "/" + prog;
			  if (fs::directory_entry(full_path).exists())
			    return true;
			  return false;
			})) != PATH.end()) {
    auto temp = std::get<std::list<Symbol>>(node.value);
    std::string full_path;
    auto prog = std::get<std::string>(op.value);
    if (!builtin_commands.contains(std::get<std::string>(op.value))) {
      full_path = *it + "/" + prog;
    } else full_path = prog;
    temp.pop_front();
    temp.push_front(Symbol("", full_path, Type::Identifier));
    node.value = temp;
    auto status = rewind_call_ext_program(node, PATH);
    wait(nullptr);
    return status;
  }
  else if (std::get<std::string>(op.value) == "->") {
    // a pipe operator!
    // append a path to each executable, if there's any valid one
    auto temp = std::get<std::list<Symbol>>(node.value);
    temp.pop_front();
    for (auto& e: temp) {
      auto progl =
	std::get<std::list<Symbol>>(e.value);
      auto prog = 
	std::get<std::string>(progl.front().value);
      progl.pop_front();
      it = std::find_if(PATH.begin(), PATH.end(),
			[&](const std::string& entry) -> bool {
			  namespace fs = std::filesystem;
			  std::string full_path;
			  full_path = entry + "/" + prog;
			  return
			    fs::directory_entry(full_path).exists();
			});
      if (it != PATH.end()) {
	std::string full_path;
	if (!builtin_commands.contains(prog)) {
	  full_path = *it + "/" + prog;
	} else full_path = prog;
	progl.push_front(Symbol("", full_path, Type::Identifier));
	e.value = progl;
      } else {
	throw std::logic_error {
	  "Unknown command/executable " + prog + "!\n"
	};
      }
    }
    node.value = temp;
    return rewind_pipe(node, PATH); 
  } else if (std::get<std::string>(op.value) == "+>") {
    //redirect with overwrite into a file
    auto temp = std::get<std::list<Symbol>>(node.value);
    temp.pop_front();
    node.value = temp;
    return rewind_redirect_overwrite(node, PATH);
  } else if (std::get<std::string>(op.value) == "++>") {
    //redirect by appending into a file
    auto temp = std::get<std::list<Symbol>>(node.value);
    temp.pop_front();
    node.value = temp;
    return rewind_redirect_append(node, PATH);
  }
  else if (user_defined_procedures[user_defined_procedures.size() - 1]
	   .contains(std::get<std::string>(op.value))) {
    result = eval_function(node, PATH);
    return result;
  }
  return node;
}

Symbol eval(Symbol root, const std::vector<std::string>& PATH) {
  Symbol result;
  std::stack<Symbol> node_stk;
  Symbol current_node;
  // results of intermediate nodes (i.e. nodes below the root)
  std::vector<std::vector<Symbol>> intermediate_results;
  // this is the data on which the actual computation takes place,
  // as we copy every intermediate result we get into this as a "leaf"
  // to get compute the value for each node, including the root.
  std::vector<std::list<Symbol>> leaves;
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
	if (!call_stack.empty())
	  bool must_pop_let = (call_stack[call_stack.size() - 1].first == "let");
	// insert the intermediate results gotten so far into the leaves, so we
	// can compute the value for the current node.
	if (!intermediate_results.empty() &&
	    !(intermediate_results[intermediate_results.size() - 1]
	      .empty())) {
	  leaves[leaves.size() - 1]
	    .insert(leaves[leaves.size() - 1].end(),
		    intermediate_results[intermediate_results.size() - 1]
		    .begin(),
		    intermediate_results[intermediate_results.size() - 1]
		    .end());
	}
	// clean up for any function definition...
	std::erase_if(leaves[leaves.size() - 1],
		      [](Symbol& s) -> bool {
			return
			  (s.type == Type::Defunc) ||
			  (s.type == Type::Command);
		      });
	eval_temp_arg = Symbol("", leaves[leaves.size() - 1], Type::List);
	result = eval_primitive_node(eval_temp_arg, PATH);
	if (!intermediate_results.empty())
	  intermediate_results.pop_back();
	if (intermediate_results.empty())
	  intermediate_results.push_back(std::vector<Symbol>{result});
	else
	  intermediate_results[intermediate_results.size() - 1]
	    .push_back(result);
	if (!variables.empty()) {
	  variables.pop_back();
	}
	if (!node_stk.empty()) {
	  current_node = node_stk.top();
	  node_stk.pop();
	} else return result;

	// check and pop if there was a 'let' somewhere before this point
	leaves.pop_back();
	if (leaves.empty() && (current_node.name == "root")) break;
      }
      else {
	Symbol child = std::get<std::list<Symbol>>(current_node.value)
	  .front();
	auto templ = std::get<std::list<Symbol>>(current_node.value);
	templ.pop_front();
	current_node.value = templ;
	node_stk.push(current_node);
	current_node = child;
	if ((child.type == Type::List) || leaves.empty())
	  leaves.push_back(std::list<Symbol>{});
      }
    } else {
      if ((current_node.type == Type::Operator) &&
	  (std::get<std::string>(current_node.value) == "if")) {
	// evaluate the if special form correctly, i.e. delaying the evaluation
	// of the second/first block depending on the branch.
	auto ifl = std::get<std::list<Symbol>>(node_stk.top().value);
	if (ifl.size() != 3)
	  throw std::logic_error {
	    "Wrong number of arguments to the 'if' procedure!"
	  };
	leaves[leaves.size() - 1] = ifl;
	leaves[leaves.size() - 1].push_front(current_node);
	Symbol dummy = Symbol(node_stk.top().name,
			      std::list<Symbol>(),
			      Type::List);
	node_stk.pop();
	node_stk.push(dummy);	
      }
      else if ((current_node.type == Type::Identifier) &&
	       (std::get<std::string>(current_node.value) == "->")) {
	auto pipel =
	  std::get<std::list<Symbol>>(node_stk.top().value);
	if (pipel.size() == 0) {
	  throw std::logic_error {
	    "A pipe must have at least one argument!\n"
	  };
	}
	leaves[leaves.size() - 1] = pipel;
	leaves[leaves.size() - 1].push_front(current_node);
	Symbol dummy = Symbol(node_stk.top().name,
			      std::list<Symbol>(),
			      Type::List);
	node_stk.pop();
	node_stk.push(dummy);
	
      }
      else if ((leaves.empty() || leaves[leaves.size() - 1].empty()) &&
	  (current_node.type == Type::Identifier)) {
	// TODO: find a way to not duplicate variable lookup (see a few blocks below)
	auto opt = variable_lookup(current_node);
	auto popt = procedure_lookup(current_node);
	auto csopt = callstack_variable_lookup(current_node);
	if (csopt != std::nullopt) {
	  if (!leaves.empty())
	    leaves[leaves.size() - 1].push_front(*csopt);
	  else
	    leaves.push_back(std::list<Symbol>{*csopt});
	} else if (opt != std::nullopt) {
	  if (!leaves.empty())
	    leaves[leaves.size() - 1].push_front(*opt);
	  else
	    leaves.push_back(std::list<Symbol>{*opt});
	} else {
	  if (!leaves.empty())
	    leaves[leaves.size() - 1].push_front(current_node);
	  else
	    leaves.push_back(std::list<Symbol>{current_node});
	}
      }
      else if (current_node.type == Type::Operator &&
	  std::get<std::string>(current_node.value) == "let") {
	// might be a function definition...
	auto letl = std::get<std::list<Symbol>>(node_stk.top().value);
        if (letl.size() < 2) {
	  // function/constant definition:
	  throw std::logic_error {"Insufficient arguments to the 'let'"
				  " procedure!\n"};
	}
        leaves[leaves.size() - 1] = letl;
	leaves[leaves.size() - 1].push_front(current_node);
        Symbol dummy = Symbol(node_stk.top().name, std::list<Symbol>(), Type::List);
	node_stk.pop();
	node_stk.push(dummy);
      }
      else if ((current_node.type == Type::Identifier) &&
	       (std::get<std::string>(leaves[leaves.size() - 1].front().value) != "let")) {
        auto opt = variable_lookup(current_node);
	auto popt = procedure_lookup(current_node);
	auto csopt = callstack_variable_lookup(current_node);
	if (csopt != std::nullopt) {
	  if (!leaves.empty())
	    leaves[leaves.size() - 1].push_back(*csopt);
	  else
	    leaves.push_back(std::list<Symbol>{*csopt});
	} else if (opt != std::nullopt) {
	  if (!leaves.empty())
	    leaves[leaves.size() - 1].push_back(*opt);
	  else
	    leaves.push_back(std::list<Symbol>{*opt});
	} else {
	  if (!leaves.empty())
	    leaves[leaves.size() - 1].push_back(current_node);
	  else
	    leaves.push_back(std::list<Symbol>{current_node});
	}
      } else {
	if (!leaves.empty())
	  leaves[leaves.size() - 1].push_back(current_node);
	else
	  leaves.push_back(std::list<Symbol>{current_node});
      }
      if (node_stk.empty()) return leaves[leaves.size() - 1].front();
      current_node = node_stk.top();
      node_stk.pop();
    }
  } while (1);
  return result;
}
