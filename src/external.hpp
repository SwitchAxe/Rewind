#pragma once
#include <unistd.h>
#include <sys/wait.h>
#include "evaluator.hpp"

Symbol eval(Symbol root, const std::vector<std::string>& PATH);

int rewind_call_ext_program(Symbol node, const std::vector<std::string>& PATH,
			    bool must_pipe = false,
			    int pipe_fd_out = 0,
			    int pipe_fd_in = 0) {
  std::cout << std::boolalpha << must_pipe << "\n";
  std::cout << pipe_fd_out << "\n";
  std::cout << pipe_fd_in << "\n";

  std::list<Symbol> nodel =
    std::get<std::list<Symbol>>(node.value);
  std::string prog = std::get<std::string>(nodel.front().value);
  rec_print_ast(node);
  std::cout << "\n";
  char* argv[nodel.size() + 1];
  argv[0] = const_cast<char*>(prog.c_str());
  nodel.pop_front();
  Symbol cur_arg;
  int i = 1;
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
    argv[i] = const_cast<char*>(arg.c_str());
    i++;
    std::cout << "arg: " << arg << "\n";
#undef p
  }
  argv[i] = nullptr;
  if (builtin_commands.contains(prog)) {
    auto l = std::list<std::string>();
    for (int idx = 1; idx < i; ++idx) {
      l.push_back(std::string{argv[idx]});
    }
    return builtin_commands[prog](l);
  }
  int status;
  int pid = fork();
  if (pid == 0) {
    int out = dup(1);
    int in = dup(0);
    if (must_pipe) {
      if (pipe_fd_out) {
	if (dup2(pipe_fd_out, STDOUT_FILENO) != STDOUT_FILENO) {
	  std::cerr << "Error while piping " + prog + " (writing)!\n";
	  return -1;
	}
	std::cerr << prog << " writes!\n";
      }
      else if (pipe_fd_in) {
	if (dup2(pipe_fd_in, STDIN_FILENO) != STDIN_FILENO) {
	  std::cerr << "Error while piping " + prog + " (reading)!\n";
	  return -1;
	}
	std::cerr << prog << " reads!\n";
	wait(nullptr);
      }
    }
    if (pipe_fd_out)
      if (dup2(out, 1) < 0) {
	std::cerr << "Error while restoring stdout for " + prog + "!\n";
	exit(1);
      } else close(out);
    else if (pipe_fd_in)
      if (dup2(in, 0) < 0) {
	std::cerr << "Error while restoring stdin for " + prog + "!\n";
	exit(1);
      } else close(in);
    status = execv(prog.c_str(), argv);
    exit(0);
  } else if (pid > 0) {
    wait(&status);
    return status;
  } else {
    std::cerr << "Error while executing child process " + prog + "!\n";
    return -1;
  }
}

int rewind_pipe(Symbol node, const std::vector<std::string>& PATH) {
  int fd[2]; //fd[0] reads, fd[1] writes
  int status;
  std::list<Symbol> nodel =
    std::get<std::list<Symbol>>(node.value);
  rec_print_ast(node);
  std::cout << "\n";
  nodel.pop_front();
  Symbol prev_p = nodel.front(); // writes to cur
  // if ((prev_p.type == Type::Operator) || prev_p.type == Type::Identifier) {
  //   prev_p = Symbol("", std::list<Symbol>{prev_p}, Type::List);
  // }
  nodel.pop_front();
  int out = dup(1);
  int in = dup(0);
  for (auto cur: nodel) {
    // if (cur.type != Type::List) {
    //   cur = Symbol("", std::list<Symbol>{cur}, Type::List);
    // }
    pipe(fd);
    status = rewind_call_ext_program(prev_p, PATH, true, fd[1], 0);
    wait(nullptr);
    status = rewind_call_ext_program(cur, PATH, true, 0, fd[0]);
    wait(nullptr);
    prev_p = cur;
    close(fd[0]);
    close(fd[1]);
  }
  return status;
}
