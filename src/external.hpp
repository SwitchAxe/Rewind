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
    if (must_pipe) {
      if (pipe_fd_out) {
	if (dup2(pipe_fd_out, STDOUT_FILENO) != STDOUT_FILENO) {
	  throw std::logic_error {
	    "Error while piping " + prog + " (writing)!\n"
	  };
	}
	std::cerr << prog << " writes!\n";
      }
      if (pipe_fd_in) {
	if (dup2(pipe_fd_in, STDIN_FILENO) != STDIN_FILENO) {
	  throw std::logic_error {
	    "Error while piping " + prog + " (reading)!\n"
	  };
	}
	std::cerr << prog << " reads!\n";
      }
    }
    status = execv(prog.c_str(), argv);
    exit(1);
  } else if (pid > 0) {
    if (pipe_fd_out) close(pipe_fd_out);
    if (pipe_fd_in) close(pipe_fd_in);
    waitpid(pid, &status, 0);
    return status;
  } else {
    throw std::logic_error {
      "Error while executing child process " + prog + "!\n"
    };
  }
}

int rewind_pipe(Symbol node, const std::vector<std::string>& PATH) {
  int fd[2]; //fd[0] reads, fd[1] writes
  int status;
  std::list<Symbol> nodel =
    std::get<std::list<Symbol>>(node.value);
  rec_print_ast(node);
  std::cout << "\n";
  // Symbol prev_p = nodel.front(); // writes to cur
  // nodel.pop_front();
  int out = dup(1);
  int in = dup(0);
  auto last = nodel.back();
  nodel.pop_back();
  pipe(fd);
  auto first = nodel.front();
  nodel.pop_front();
  rewind_call_ext_program(first, PATH, true, fd[1], 0);
  for (auto cur: nodel) {
    status = rewind_call_ext_program(cur, PATH, true, fd[0], fd[1]);
  }
  wait(nullptr);
  close(fd[1]);
  rewind_call_ext_program(last, PATH, true, 0, fd[0]);
  close(fd[0]);
  return status;
}
