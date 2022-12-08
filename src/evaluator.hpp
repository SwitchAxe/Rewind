#pragma once
#include "parser.hpp"
#include "procedures.hpp"
#include <iostream>
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
	// first, delete the most recent (most inner) scope:
	if (!variables.empty()) variables.pop_back();
	
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
      Symbol maybe_ref;
      if ((current_node.type == Type::Identifier) &&
	  (std::get<std::string>(leaves[leaves.size() - 1][0].value) != "let")) {
	int last_scope = variables.size() - 1;
	std::string ref_value = std::get<std::string>(current_node.value);
	if (last_scope < 0) {
	  throw std::logic_error{"Unbound identifier " + ref_value + "\n!"};
	}
	if (!variables[last_scope].contains(ref_value)) {
	  throw std::logic_error {"Unbound identifier " + ref_value + "!\n"};
	}
	maybe_ref = variables[last_scope][ref_value];
	if (!leaves.empty())
	  leaves[leaves.size() - 1].push_back(maybe_ref);
	else
	  leaves.push_back(std::vector<Symbol>{maybe_ref});
      } else {
	if (!leaves.empty())
	  leaves[leaves.size() - 1].push_back(current_node);
	else
	  leaves.push_back(std::vector<Symbol>{current_node});
      }
      current_node = node_stk.top();
      node_stk.pop();
    }
  } while (1);
  return result;
}
