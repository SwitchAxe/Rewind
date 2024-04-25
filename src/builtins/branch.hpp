#pragma once
#include "boolean.hpp"
#include <compare>



static const Symbol match_any = Symbol("", "_", Type::Identifier);
static const Symbol match_eq =
    Symbol("",
           std::list<Symbol>{Symbol("", "=", Type::Operator),
                             Symbol("", "x", Type::Identifier)},
           Type::List);

static const Symbol match_neq =
    Symbol("",
           std::list<Symbol>{Symbol("", "!=", Type::Operator),
                             Symbol("", "x", Type::Identifier)},
           Type::List);

static const Symbol match_in_list =
    Symbol("",
           std::list<Symbol>{Symbol("", "in", Type::Operator),
                             Symbol("", "l", Type::Identifier)},
           Type::List);

static Symbol match_less_than =
    Symbol("",
           std::list<Symbol>{Symbol("", "<", Type::Operator),
                             Symbol("", "x", Type::Number)},
           Type::List);
static const Symbol match_less_than_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", "<", Type::Operator),
                             Symbol("", "a", Type::Number),
                             Symbol("", "b", Type::Number)},
           Type::List);

static const Symbol match_greater_than =
    Symbol("",
           std::list<Symbol>{Symbol("", ">", Type::Operator),
                             Symbol("", "b", Type::Number)},
           Type::List);

static const Symbol match_greater_than_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", ">", Type::Operator),
                             Symbol("", "a", Type::Number),
                             Symbol("", "b", Type::Number)},
           Type::List);

static const Symbol match_eq_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", "=", Type::Operator),
                             Symbol("", "a", Type::Identifier),
                             Symbol("", "x", Type::Identifier)},
           Type::List);

static const Symbol match_neq_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", "!=", Type::Operator),
                             Symbol("", "a", Type::Identifier),
                             Symbol("", "x", Type::Identifier)},
           Type::List);

static const Symbol match_in_list_capture =
    Symbol("",
           std::list<Symbol>{Symbol("", "in", Type::Operator),
                             Symbol("", "a", Type::Identifier),
                             Symbol("", "l", Type::Identifier)},
           Type::List);

static const Symbol match_head_tail =
  Symbol("",
	 std::list<Symbol>{
	   Symbol("", "cons", Type::Operator),
	   Symbol("", "a", Type::Identifier),
	   Symbol("", "b", Type::Identifier)},
	 Type::List);


bool weak_compare(Symbol fst, Symbol other) {
  if (fst.type == Type::Operator)
    return std::get<std::string>(fst.value) ==
	   std::get<std::string>(other.value);
  if (other.type == Type::Operator)
    return std::get<std::string>(fst.value) ==
	   std::get<std::string>(other.value);
  if (fst.type == Type::Identifier)
    return true;
  if (other.type == Type::Identifier)
    return true;
  if (fst.type == other.type) {
    if ((fst.type == Type::List) || (fst.type == Type::ListLiteral)) {
      bool is_same_pattern = true;
      auto ll = std::get<std::list<Symbol>>(fst.value);
      auto otherl = std::get<std::list<Symbol>>(other.value);
      if (ll.size() != otherl.size())
        return false;
      std::list<std::pair<Symbol, Symbol>> zipped;
      std::transform(
          ll.begin(), ll.end(), otherl.begin(), std::back_inserter(zipped),
          [](const Symbol l, const Symbol r) -> std::pair<Symbol, Symbol> {
            return std::make_pair(l, r);
          });
      for (auto [l, r] : zipped) {
        is_same_pattern = is_same_pattern && weak_compare(l, r);
      }
      return is_same_pattern;
    } else
      return true;
  }
  return false;
}

bool compare_list_structure(Symbol l, Symbol r) {
  if ((l.type != Type::List) && (l.type != Type::ListLiteral))
    throw std::logic_error{"First operand to 'compare_list_structure' "
                           "(internal function) is not a list!\n"};
  if ((r.type != Type::List) && (r.type != Type::ListLiteral))
    throw std::logic_error{"Second operand to 'compare_list_structure' "
                           "(internal function) is not a list!\n"};
  auto ll = std::get<std::list<Symbol>>(l.value);
  auto rl = std::get<std::list<Symbol>>(r.value);
  if (ll.size() != rl.size())
    return false;
  if (ll.empty())
    return true;
  // Zip the two lists together and then iterate on the result
  std::list<std::pair<Symbol, Symbol>> zipped;
  std::transform(
      ll.begin(), ll.end(), rl.begin(), std::back_inserter(zipped),
      [](const Symbol &lle, const Symbol &rle) -> std::pair<Symbol, Symbol> {
        return std::make_pair(lle, rle);
      });
  bool is_structure_equal = true;
  for (auto p : zipped) {
    if (std::holds_alternative<std::string>(p.first.value) &&
        std::get<std::string>(p.first.value) == "_") {
      continue;
    }
    if (std::holds_alternative<std::string>(p.second.value) &&
        std::get<std::string>(p.second.value) == "_") {
      continue;
    }
    if ((p.first.type == Type::List) || (p.first.type == Type::ListLiteral)) {
      if ((p.second.type == Type::List) || (p.second.type == Type::ListLiteral)) {
        is_structure_equal =
            is_structure_equal && compare_list_structure(p.first, p.second);
      } else
        return false;
    } else if (p.second.type == Type::List) {
      return false;
    }
  }
  return is_structure_equal;
}

std::list<std::pair<Symbol, Symbol>> rec_bind_list(Symbol lhs, Symbol rhs) {
  std::list<std::pair<Symbol, Symbol>> ret;
  auto fst = std::get<std::list<Symbol>>(lhs.value);
  auto snd = std::get<std::list<Symbol>>(rhs.value);
  std::list<std::pair<Symbol, Symbol>> zipped;
  std::transform(fst.begin(), fst.end(), snd.begin(),
                 std::back_inserter(zipped),
                 [](Symbol l, Symbol r) -> std::pair<Symbol, Symbol> {
                   return std::make_pair(l, r);
                 });
  for (auto p : zipped) {
    if (p.first.type != Type::List) {
      ret.push_back(p);
    } else {
      auto rec = rec_bind_list(p.first, p.second);
      ret.insert(ret.end(), rec.begin(), rec.end());
    }
  }
  return ret;
}

// some auxillary functions to not repeat code in the
// following map.

std::optional<bool> do_ordering_match(Symbol s1, Symbol s2,
				      std::strong_ordering ord) {
  if (ord == std::strong_ordering::equal)
    return std::optional<bool>{s1.value == s2.value};
  if (s2.type != Type::Number)
    return std::nullopt;
  if (s1.type != Type::Number)
    return std::nullopt;
  
  // truncate both numbers to signed ints
  long long int a = std::visit(overloaded {
    [](long long int n) -> long long int { return n; },
    [](long long unsigned int n) -> long long int { return n; },
    [](auto) -> long long int { return 0; }
  }, s1.value);

  long long int b = std::visit(overloaded {
    [](long long int n) -> long long int { return n; },
    [](long long unsigned int n) -> long long int { return n; },
    [](auto) -> long long int { return 0; }
  }, s2.value);

  if (ord == std::strong_ordering::less)
    return std::optional<bool>{a < b};
  else return std::optional<bool>{a > b};
}

std::list<std::pair<Symbol, std::function<std::optional<Symbol>(Symbol, std::list<Symbol>, variables,
				      const path&, std::list<Symbol>)>>>
patterns = {
  std::pair{match_in_list, [](Symbol matched, std::list<Symbol> l, variables vs,
			      const path& PATH,
			      std::list<Symbol> body) -> std::optional<Symbol> {
    auto s = l.back();
    if (s.type != Type::List)
      throw std::logic_error {"'in' (match): expected a list!\n"};
    auto x = std::get<std::list<Symbol>>(s.value);
    if (std::find_if(x.begin(), x.end(),
                     [&](Symbol y) -> bool {
                       return matched.value == y.value;
                     }) != x.end()) {
      Symbol result;
      for (auto e : body)
	result = eval(e, PATH, vs);
      return result;
    }
    return std::nullopt;
  }},
  std::pair{match_less_than, [](Symbol matched, std::list<Symbol> l, variables vs,
				const path& PATH,
				std::list<Symbol> body) -> std::optional<Symbol> {
    Symbol s = l.back();
    auto opt = do_ordering_match(matched, eval(s, PATH, vs),
				 std::strong_ordering::less);
    if ((opt != std::nullopt) && *opt) {  Symbol result;
      for (auto e: body)
	result = eval(e, PATH, vs);
      return result;
    }
    return std::nullopt;
  }},
  std::pair{match_less_than_capture, [](Symbol matched, std::list<Symbol> l, variables vs,
					const path& PATH,
					std::list<Symbol> body) -> std::optional<Symbol> {
    l.pop_front();
    Symbol var = l.front();
    l.pop_front();
    Symbol s = l.front();
    auto opt = do_ordering_match(matched, eval(s, PATH, vs),
				 std::strong_ordering::less);
    if ((opt != std::nullopt) && *opt) {
      Symbol result;
      vs.insert({std::get<std::string>(var.value), matched});
      for (auto e : body)
	result = eval(e, PATH, vs);
      return result;
    }
    return std::nullopt;
  }},
  std::pair{match_greater_than, [](Symbol matched, std::list<Symbol> l, variables vs,
				   const path& PATH,
				   std::list<Symbol> body) -> std::optional<Symbol> {
    Symbol s = l.back();
    auto opt = do_ordering_match(matched, eval(s, PATH, vs),
				 std::strong_ordering::greater);
    if ((opt != std::nullopt) && *opt) {
      Symbol result;
      for (auto e: body)
	result = eval(e, PATH, vs);
      return result;
    }
    return std::nullopt;
  }},
  std::pair{match_greater_than_capture, [](Symbol matched, std::list<Symbol> l,
					   variables vs,
					   const path& PATH,
					   std::list<Symbol> body) -> std::optional<Symbol> {
    l.pop_front();
    Symbol var = l.front();
    l.pop_front();
    Symbol s = l.front();
    auto opt = do_ordering_match(matched, eval(s, PATH, vs),
				 std::strong_ordering::greater);
    if ((opt != std::nullopt) && *opt) {
      Symbol result;
      vs.insert({std::get<std::string>(var.value), matched});
      for (auto e : body)
	result = eval(e, PATH, vs);
      return result;
    }
    return std::nullopt;
  }},
  std::pair{match_eq, [](Symbol matched, std::list<Symbol> l, variables vs,
			 const path& PATH,
			 std::list<Symbol> body) -> std::optional<Symbol> {
    Symbol s = l.back();
    auto opt = do_ordering_match(matched, eval(s, PATH, vs),
				 std::strong_ordering::equal);
    if ((opt != std::nullopt) && *opt) {
      Symbol result;
      for (auto e: body)
	result = eval(e, PATH, vs);
      return result;
    }
    return std::nullopt;
  }},
  std::pair{match_eq_capture, [](Symbol matched, std::list<Symbol> l, variables vs,
				 const path& PATH,
				 std::list<Symbol> body) -> std::optional<Symbol> {
    l.pop_front();
    Symbol var = l.front();
    l.pop_front();
    Symbol s = l.front();
    auto opt = do_ordering_match(matched, eval(s, PATH, vs),
				 std::strong_ordering::equal);
    if ((opt != std::nullopt) && *opt) {
      Symbol result;
      vs.insert({std::get<std::string>(var.value), matched});
      for (auto e : body)
	result = eval(e, PATH, vs);
      return result;
    }
    return std::nullopt;
  }},
  std::pair{match_neq, [](Symbol matched, std::list<Symbol> l, variables vs,
			  const path& PATH,
			  std::list<Symbol> body) -> std::optional<Symbol> {
    Symbol s = l.back();
    auto opt = do_ordering_match(matched, eval(s, PATH, vs),
				 std::strong_ordering::equal);
    if ((opt != std::nullopt) && !*opt) {
      Symbol result;
      for (auto e: body)
	result = eval(e, PATH, vs);
      return result;
    }
    return std::nullopt;
  }},
  std::pair{match_neq_capture, [](Symbol matched, std::list<Symbol> l, variables vs,
				  const path& PATH,
				  std::list<Symbol> body) -> std::optional<Symbol> {
    l.pop_front();
    Symbol var = l.front();
    l.pop_front();
    Symbol s = l.front();
    auto opt = do_ordering_match(matched, eval(s, PATH, vs),
				 std::strong_ordering::equal);
    if ((opt != std::nullopt) && !*opt) {
      Symbol result;
      vs.insert({std::get<std::string>(var.value), matched});
      for (auto e : body)
	result = eval(e, PATH, vs);
      return result;
    }
    return std::nullopt;
  }},
  std::pair{match_head_tail, [](Symbol matched, std::list<Symbol> l, variables vs,
				const path& PATH,
				std::list<Symbol> body) -> std::optional<Symbol> {
    if ((matched.type != Type::List) && (matched.type != Type::ListLiteral))
      throw std::logic_error {"in 'cons' (match): expected a list to destructure!\n"};
    auto x = std::get<std::list<Symbol>>(matched.value);
    l.pop_front();
    std::string head = std::get<std::string>(l.front().value);
    std::string tail = std::get<std::string>(l.back().value);
    vs.insert(std::pair{head, x.front()});
    x.pop_front();
    vs.insert(std::pair{tail, Symbol("", x, matched.type)});
    Symbol result;
    for (auto e : body)
      result = eval(e, PATH, vs);
    return result;
  }},
  std::pair{match_in_list_capture, [](Symbol matched, std::list<Symbol> l, variables vs,
				      const path& PATH,
				      std::list<Symbol> body) -> std::optional<Symbol> {
    l.pop_front();
    std::string id = std::get<std::string>(l.front().value);
    auto x = l.back();
    x = eval(x, PATH, vs);
    auto y = std::get<std::list<Symbol>>(x.value);
    Symbol result;
    if (auto it = std::find_if(y.begin(),
				y.end(),
				[&](Symbol a) -> bool {
				  return a.value == matched.value; }); it != y.end()) {
      vs.insert(std::pair{id, matched});
      for (auto e : body)
	result = eval(e, PATH, vs);
      return result;
    }
    return std::nullopt;
  }}
};

std::map<std::string, Functor> branching = {
  std::pair{"cond",
            Functor{[](std::list<Symbol> args,
                       const path& PATH,
                       variables& vs) {
              Symbol result = Symbol("", false, Type::Boolean);
              for (auto e: args) {
                if (e.type != Type::List) {
                  throw std::logic_error {
                    "Wrong expression in 'cond'! Expected "
                    "[<clause> <consequent>].\n" };
                }
                auto l = std::get<std::list<Symbol>>(e.value);
                if (l.size() < 2) {
                  throw std::logic_error {
                    "Wrong expression in 'cond'! Expected "
                    "[<clause> <consequent>].\n" };
                }
                Symbol clause = l.front();
                l.pop_front();
                clause = eval(clause, PATH, vs, clause.line);
                if (convert_value_to_bool(clause)) {
                  for (auto e : l)
                    result = eval(e, PATH, vs, e.line);
                  break;
		}
              }
              return result;
            }}},
  std::pair{"match", Functor{[](std::list<Symbol> args, const path& PATH, variables& vs) {
    Symbol matched = args.front();
    args.pop_front();
    matched = eval(matched, PATH, vs);
    Symbol result;
    for (auto e : args) {
      auto l = std::get<std::list<Symbol>>(e.value);
      auto pat = l.front();
      l.pop_front();

      // a special rule for the 'any' ("_") fallback pattern.
      if (pat.type == Type::Identifier)
	if (std::get<std::string>(pat.value) == "_") {
	  vs.insert({"_", matched});
	  for (auto x : l)
	    result = eval(x, PATH, vs);
	  return result;
	}
      
      if ((pat.type != Type::List)) {
	if (pat.value == matched.value) {
	  for (auto x : l) {
	    result = eval(x, PATH, vs);
	  }
	  return result;
	}
      }
      auto x = std::get<std::list<Symbol>>(pat.value);
      if ((matched.type == Type::List) || (matched.type == Type::ListLiteral)) {
	if ((pat.type == Type::ListLiteral) &&
	    compare_list_structure(pat, matched)) {
	  auto ps = rec_bind_list(pat, matched);
	  for (auto [var, val] : ps) {
	    vs.insert({std::get<std::string>(var.value), val});
	  }
	  for (auto x : l)
	    result = eval(x, PATH, vs);
	  return result;
	}
      }
      for (auto [k, v] : patterns) {
	if (weak_compare(pat, k)) {
	  auto opt = v(matched, x, vs, PATH, l);
	  if (opt == std::nullopt) break;
	  return *opt;
	}
      }
    }
    return Symbol("", false, Type::Boolean);
  }}}
};
