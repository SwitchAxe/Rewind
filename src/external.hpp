#pragma once
#include <unistd.h>
#include <sys/wait.h>
#include "evaluator.hpp"

Symbol eval(Symbol root, const std::vector<std::string>& PATH);

int rewind_call_ext_program(Symbol node,
			    const std::vector<std::string>& PATH) {
  std::list<Symbol> nodel =
    std::get<std::list<Symbol>>(node.value);
  std::string prog = std::get<std::string>(nodel.front().value);
  char* argv[std::get<std::list<Symbol>>(node.value).size() - 1];
  nodel.pop_front();
  Symbol cur_arg;
  int i = 0;
  for (auto cur: nodel) {
    if (cur.type == Type::List) {
      cur_arg = eval(cur, PATH);
      if (cur_arg.type == Type::List) {
	throw std::logic_error {
	  "Can't have lists as arguments to programs!\n"
	};
      }
    } else cur_arg = cur;
    using namespace matchit;
    Id<std::string> s;
    Id<int> in;
    Id<bool> b;
#define p pattern
    std::string arg = match (cur_arg.value)
      (p | as<std::string>(s) = [&] {return *s;},
       p | as<int>(in) = [&] {
	 return std::to_string(*in);
       },
       p | as<bool>(b) = [&] {
	 return (*b == true) ? "true" : "false";
       });
    argv[i] = (char*) malloc(arg.length() + 1);
    argv[i] = const_cast<char*>(arg.c_str());
    i++;
#undef p
  }
  argv[i] = nullptr;
  int status;
  int pid = fork();
  if (pid == 0) {
    return execv(prog.c_str(), argv);
  } else if (pid > 0) {
    wait(&status);
    return status;
  } else {
    std::cerr << "Error while executing child process " + prog + "!\n";
    return -1;
  }
}

