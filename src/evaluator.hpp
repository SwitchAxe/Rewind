#pragma once
#include "parser.hpp"
#include "procedures.hpp"
#include <iostream>
#include <algorithm>
#include <optional>

Symbol eval(Symbol root);

Symbol eval_function(Symbol node) {
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
  variables.push_back(std::map<std::string, Symbol> {});
  size_t paramls = paraml.size();
  for (int i = 0; i < paramls; ++i) {
    variables[variables.size() - 1]
      .insert_or_assign(
			std::get<std::string>(paraml.front().value),
			argl.front());
    paraml.pop_front();
    argl.pop_front();
  }
  for (auto e: std::get<std::list<Symbol>>(body.value)) {
    result = eval(e);
  }
  return result;
}

// to use with nodes with only leaf children.
Symbol eval_primitive_node(Symbol node) {
  std::vector<Symbol> intermediate_results;
  Symbol result;
  for (auto e: std::get<std::list<Symbol>>(node.value)) {
    intermediate_results.push_back(e);
  }
  Symbol op = intermediate_results[0];
  intermediate_results.erase(intermediate_results.begin());
  if (op.type == Type::Operator) {
    if (procedures.contains(std::get<std::string>(op.value)))
      result = procedures[
			  std::get<std::string>(op.value)
			  ](intermediate_results);
    else
      throw std::logic_error{"Unbound procedure!\n"};
    return result;
  }
  else if (user_defined_procedures[user_defined_procedures.size() - 1]
	   .contains(std::get<std::string>(op.value))) {
    auto l = std::list<Symbol>(intermediate_results.begin(),
			       intermediate_results.end());
    l.push_front(op);
    Symbol fn("", l, Type::List);
    result = eval_function(fn);
    return result;
  }
  return node;
}

std::optional<Symbol> variable_lookup(Symbol id) {
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
  bool in_function_def = false;
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
	if (!intermediate_results.empty()) {
	  leaves[leaves.size() - 1]
	    .insert(leaves[leaves.size() - 1].end(),
		    intermediate_results.begin(),
		    intermediate_results.end());
	}
	// clean up for any function definition...
	std::erase_if(leaves[leaves.size() - 1],
		      [](Symbol& s) -> bool {
			return s.type == Type::Defunc;
		      }); 
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
      if ((leaves.empty() || leaves[leaves.size() - 1].empty()) &&
	  (current_node.type == Type::Identifier)) {
	// TODO: find a way to not duplicate variable lookup (see a few blocks below)
	auto opt = variable_lookup(current_node);
	auto popt = procedure_lookup(current_node);
	if (opt == std::nullopt) {
	  if (popt == std::nullopt) {
	    throw std::logic_error {"Unbound Identifier " +
				    std::get<std::string>(current_node.value) +
				    "!\n"};
	  } else {
	    if (!leaves.empty())
	      leaves[leaves.size() - 1].push_back(current_node);
	    else
	      leaves.push_back(std::vector<Symbol>{current_node});
	    
	  }
	} else
	  if (!leaves.empty())
	    leaves[leaves.size() - 1].push_back(*opt);
	  else
	    leaves.push_back(std::vector<Symbol>{*opt});
      }
      else if (current_node.type == Type::Operator &&
	  std::get<std::string>(current_node.value) == "let") {
	// might be a function definition...
	auto letl = std::get<std::list<Symbol>>(node_stk.top().value);
        if (letl.size() >= 2) {
	  // function definition:
	  for (auto e: letl) {
	    leaves[leaves.size() - 1].push_back(e);
	  }
	  if (!leaves.empty())
	    leaves[leaves.size() - 1].push_back(current_node);
	  else
	    leaves.push_back(std::vector<Symbol>{current_node});
	  std::reverse(leaves[leaves.size() - 1].begin(),
		       leaves[leaves.size() - 1].end());
	} else {
	  throw std::logic_error {"Insufficient arguments to the 'let'"
				  " procedure!\n"};
	}
	Symbol dummy = node_stk.top();
	node_stk.pop();
	auto dummyl = std::get<std::list<Symbol>>(dummy.value);
	dummyl.clear();
	dummy.value = dummyl;
	node_stk.push(dummy);
      }
      else if ((current_node.type == Type::Identifier) &&
	  (std::get<std::string>(leaves[leaves.size() - 1][0].value) != "let")) {
	auto opt = variable_lookup(current_node);
	if (opt == std::nullopt) {
	  throw std::logic_error {"Unbound Identifier " +
				  std::get<std::string>(current_node.value) +
				  "!\n"};
	}
	if (!leaves.empty())
	  leaves[leaves.size() - 1].push_back(*opt);
	else
	  leaves.push_back(std::vector<Symbol>{*opt});
      } else {
	if (!leaves.empty())
	  leaves[leaves.size() - 1].push_back(current_node);
	else
	  leaves.push_back(std::vector<Symbol>{current_node});
      }
      if (node_stk.empty()) return leaves[leaves.size() - 1][0];
      current_node = node_stk.top();
      node_stk.pop();
    }
  } while (1);
  return result;
}
