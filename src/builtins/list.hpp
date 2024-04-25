#pragma once
#include "../include.hpp"

// functions on lists

std::map<std::string, Functor> list = {
    std::pair{"hd", Functor{[](std::list<Symbol> args) -> Symbol {
      if ((args.front().type != Type::List) && (args.front().type != Type::ListLiteral))
        throw std::logic_error {"'hd': Expected a list!\n"};
      auto l = std::get<std::list<Symbol>>(args.front().value);
      return l.front();
    }}},
    std::pair{"tl", Functor{[](std::list<Symbol> args) -> Symbol {
      if ((args.front().type != Type::List) && (args.front().type != Type::ListLiteral))
        throw std::logic_error {"'tl': Expected a list!\n"};
      auto l = std::get<std::list<Symbol>>(args.front().value);
      l.pop_front();
      return Symbol("", l, Type::List);
    }}},
    std::pair{"reverse", Functor{[](std::list<Symbol> args) -> Symbol {
      if ((args.front().type != Type::List) && (args.front().type != Type::ListLiteral))
        throw std::logic_error {"'reverse': Expected a list!\n"};
      auto l = std::get<std::list<Symbol>>(args.front().value);
      std::reverse(l.begin(), l.end());
      return Symbol("", l, Type::List);
    }}},
    std::pair{"delete", Functor{[](std::list<Symbol> args) -> Symbol {
      if ((args.front().type != Type::List) && (args.front().type != Type::ListLiteral))
        throw std::logic_error {"'delete': Expected a list as"
                                " first argument!\n"};
      auto l = std::get<std::list<Symbol>>(args.front().value);
      args.pop_front();
      long long signed int idx =
        std::visit(overloaded{
          [](long long int n) -> long long int { return n; },
          [](long long unsigned int n) -> long long int {
            return n;
          },
          [](auto) -> long long int { return 0; }
        }, args.front().value);
      long long int i = 0;
      auto it = l.begin();
      while (i < idx) {
        it++;
        i++;
      }
      l.erase(it);
      return Symbol("", l, Type::List);
    }}},
    std::pair{"insert", Functor{[](std::list<Symbol> args) -> Symbol {
      if (args.size() != 3)
        throw std::logic_error {"'insert': expected 3 argumets!\n"};
      if ((args.front().type != Type::List) && (args.front().type != Type::ListLiteral))
        throw std::logic_error {"'insert': Expected a list as"
                                " first argument!\n"};
      auto l = std::get<std::list<Symbol>>(args.front().value);
      args.pop_front();
      if (args.front().type != Type::Number)
        throw std::logic_error {"'insert': second argument "
                                "should be a number!\n"};
      long long signed int idx =
        std::visit(overloaded{
          [](long long int n) -> long long int { return n; },
          [](long long unsigned int n) -> long long int {
            return n;
          },
          [](auto) -> long long int { return 0; }
        }, args.front().value);
      args.pop_front();
      long long int i = 0;
      auto it = l.begin();
      while (i < idx) {
        it++;
        i++;
      }
      l.insert(it, args.front());
      return Symbol("", l, Type::List);
    }}},
    std::pair{"ltos", Functor{[](std::list<Symbol> args) -> Symbol {
      if (args.size() != 1)
        throw std::logic_error {"'ltos': Expected one argument!\n"};
      if ((args.front().type != Type::List) && (args.front().type != Type::ListLiteral))
        throw std::logic_error {"'ltos': Expected a list!\n"};
      std::string s;
      for (auto x : std::get<std::list<Symbol>>(args.front().value)) {
        if ((x.type == Type::String) ||
            (x.type == Type::Identifier))
          s += std::get<std::string>(x.value);
        else
          throw std::logic_error {
            "'ltos': expected a list of strings!\n"
          };
      }
      return Symbol("", s, Type::String);
    }}},
    std::pair{"ltof", Functor{[](std::list<Symbol> args) -> Symbol {
      if (args.size() != 1)
        throw std::logic_error {"'ltof': Expected one argument!\n"};
      if ((args.front().type != Type::List) && (args.front().type != Type::ListLiteral))
        throw std::logic_error {"'ltof': Expected a list!\n"};
      auto l = std::get<std::list<Symbol>>(args.front().value);
      if (l.size() != 2)
	throw std::logic_error {"'ltof': The correct format is '[<arguments> <body>]"};
      return Symbol("", args.front().value, Type::Function);
    }}},
    std::pair{"++", Functor{[](std::list<Symbol> args) -> Symbol {
      std::list<Symbol> l;
      for (auto x : args) {
        if ((x.type == Type::List) || (x.type == Type::ListLiteral)) {
          auto r = std::get<std::list<Symbol>>(x.value);
          l.insert(l.end(), r.begin(), r.end());
        } else l.push_back(x);
      }
      return Symbol("", l, Type::List);
    }}},
    std::pair{"length", Functor{[](std::list<Symbol> args) -> Symbol {
      if (args.empty()) {
	return Symbol("", 0, Type::Number);
      }
      if ((!std::holds_alternative<std::list<Symbol>>(args.front().value)) ||
	  (args.size() > 1)) {
	throw std::logic_error{"'length' expects a list of which to return "
                               "the length!\n"};
      }
      auto lst = std::get<std::list<Symbol>>(args.front().value);
      if (lst.empty()) {
	return Symbol("", 0, Type::Number);
      }
      return Symbol("", static_cast<long long unsigned int>(lst.size()),
                    Type::Number);
    }}}
};
