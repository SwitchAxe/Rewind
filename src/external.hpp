#pragma once
#include <unistd.h>
#include <sys/wait.h>
#include "evaluator.hpp"
#include <cstring>

Symbol eval(Symbol root, const std::vector<std::string>& PATH);

int rewind_call_ext_program(Symbol node, const std::vector<std::string>& PATH,
			    bool must_pipe = false,
			    int pipe_fd_out = 0,
			    int pipe_fd_in = 0) {
  std::list<Symbol> nodel =
    std::get<std::list<Symbol>>(node.value);
  std::string prog = std::get<std::string>(nodel.front().value);
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
    argv[i] = (char*) malloc(arg.length() + 1);
    std::strcpy(argv[i], arg.c_str());
    i++;
#undef p
  }
  argv[i] = nullptr;
  if (builtin_commands.contains(prog)) {
    auto l = std::list<std::string>();
    for (int idx = 1; idx < i; ++idx) {
      l.push_back(std::string{argv[idx]});
      free(argv[idx]);
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
	close(pipe_fd_out);
      }
      if (pipe_fd_in) {
	if (dup2(pipe_fd_in, STDIN_FILENO) != STDIN_FILENO) {
	  throw std::logic_error {
	    "Error while piping " + prog + " (reading)!\n"
	  };
	}
	close(pipe_fd_in);
      }
    }
    status = execv(prog.c_str(), argv);
    exit(1);
  } else if (pid > 0) {
    if (pipe_fd_out) close(pipe_fd_out);
    if (pipe_fd_in) close(pipe_fd_in);
    for (int idx = 0; argv[idx] != nullptr; ++idx) {
      free(argv[i]);
    }
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
  auto last = nodel.back();
  nodel.pop_back();
  pipe(fd);
  auto first = nodel.front();
  nodel.pop_front();
  int old_read_end;
  rewind_call_ext_program(first, PATH, true, fd[1], 0);
  for (auto cur: nodel) {
    old_read_end = dup(fd[0]);
    pipe(fd);
    status = rewind_call_ext_program(cur, PATH, true, fd[1], old_read_end);
  }
  close(fd[1]);
  close(old_read_end);
  rewind_call_ext_program(last, PATH, true, 0, fd[0]);
  while (wait(nullptr) != -1);
  close(fd[0]);
  return status;
}
