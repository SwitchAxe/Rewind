#pragma once
#include "evaluator.hpp"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
Symbol rewind_call_ext_program(Symbol node,
                               const std::vector<std::string> &PATH,
                               bool must_pipe, int pipe_fd_out,
                               int pipe_fd_in) {
  const auto is_strlit = [](std::string s) -> bool {
    return (s.size() > 1) && (s[0] == '"') && (s[s.size() - 1] == '"');
  };
  std::list<Symbol> nodel = std::get<std::list<Symbol>>(node.value);
  std::string prog = std::get<std::string>(nodel.front().value);
  char *argv[nodel.size() + 1];
  argv[0] = const_cast<char *>(prog.c_str());
  nodel.pop_front();
  Symbol cur_arg;
  int i = 1;
  for (auto cur : nodel) {
    if (cur.type == Type::List) {
      cur_arg = eval(cur, PATH);
      if (cur_arg.type == Type::List) {
        throw std::logic_error{"Can't have lists as arguments to programs!\n"};
      }
    } else
      cur_arg = cur;
    using namespace matchit;
    Id<std::string> s;
    Id<int> in;
    Id<bool> b;
#define p pattern
    std::string arg = match(cur_arg.value)(
        p | as<std::string>(s) = [&] { return *s; },
        p | as<int>(in) = [&] { return std::to_string(*in); },
        p | as<bool>(b) = [&] { return (*b == true) ? "true" : "false"; });
    argv[i] = (char *)malloc(arg.length() + 1);
    if (is_strlit(arg)) {
      arg = arg.substr(1, arg.size() - 2);
    }
    std::strcpy(argv[i], arg.c_str());
    i++;
#undef p
  }
  argv[i] = nullptr;
  if (procedures.contains(prog)) {
    auto l = std::list<Symbol>();
    for (int idx = 1; idx < i; ++idx) {
      l.push_back(eval(nodel.front(), PATH));
      nodel.pop_front();
      free(argv[idx]);
    }
    return procedures[prog](l);
  }
  int status = 0;
  std::string result_s = "";
  int pid = fork();
  if (pid == 0) {
    if (must_pipe) {
      if (pipe_fd_out) {
        if (dup2(pipe_fd_out, STDOUT_FILENO) != STDOUT_FILENO) {
          throw std::logic_error{"Error while piping " + prog +
                                 " (writing)!\n"};
        }
        close(pipe_fd_out);
      }
      if (pipe_fd_in) {
        if (dup2(pipe_fd_in, STDIN_FILENO) != STDIN_FILENO) {
          throw std::logic_error{"Error while piping " + prog +
                                 " (reading)!\n"};
        }
        close(pipe_fd_in);
      }
    }
    // check for additional environment variables
    if (environment_variables.empty()) {
      status = execv(prog.c_str(), argv);
    } else {
      char
          *envp[environment_variables[environment_variables.size() - 1].size() +
                1];
      int j = 0;
      for (auto e : environment_variables[environment_variables.size() - 1]) {
        std::string kv = e.first + "=" + e.second;
        envp[j] = (char *)malloc(kv.size());
        strcpy(envp[j], kv.c_str());
        j++;
      }
      envp[j] = nullptr;
      environment_variables.pop_back();
      status = execve(prog.c_str(), argv, envp);
    }
    if (status) {
      return Symbol("", status, Type::Number);
    }
    exit(1);
  } else if (pid > 0) {
    if ((node.depth > 1) && must_pipe) {
      char *buf = (char *)malloc(1024);
      int cnt;
      close(pipe_fd_out);
      while ((cnt = read(pipe_fd_in, buf, 1023))) {
        if (cnt == -1) {
          throw std::logic_error{"Read failed in rewind_call_ext_program.\n"};
        }
        buf[cnt] = '\0';
        std::string temp{buf};
        result_s += temp;
      }
      free(buf);
    }
    if (pipe_fd_out) {
      close(pipe_fd_out);
    }
    if (pipe_fd_in)
      close(pipe_fd_in);
    for (int idx = 1; argv[idx] != nullptr; ++idx) {
      free(argv[idx]);
    }
    if ((result_s != "") || (node.depth > 1)) {
      return Symbol("", result_s, Type::String, node.depth);
    }
    return Symbol("", status, Type::Number);
  } else {
    throw std::logic_error{"Error while executing child process " + prog +
                           "!\n"};
  }
}

Symbol rewind_pipe(Symbol node, const std::vector<std::string> &PATH) {
  int fd[2]; // fd[0] reads, fd[1] writes
  Symbol status;
  std::list<Symbol> nodel = std::get<std::list<Symbol>>(node.value);
  auto last = nodel.back();
  nodel.pop_back();
  pipe(fd);
  if (nodel.empty()) {
    close(fd[0]);
    close(fd[1]);
    auto status = rewind_call_ext_program(last, PATH, false, 0, 0);
    while (wait(nullptr) != -1)
      ;
    return status;
  }
  auto first = nodel.front();
  nodel.pop_front();
  int old_read_end;
  status = rewind_call_ext_program(first, PATH, true, fd[1], 0);
  for (auto cur : nodel) {
    old_read_end = dup(fd[0]);
    pipe(fd);
    status = rewind_call_ext_program(cur, PATH, true, fd[1], old_read_end);
  }
  status = rewind_call_ext_program(last, PATH, true, 0, fd[0]);
  close(fd[1]);
  close(old_read_end);
  while (wait(nullptr) != -1)
    ;
  close(fd[0]);
  return status;
}

Symbol rewind_redirect_append(Symbol node,
                              const std::vector<std::string> &PATH) {
  auto nodel = std::get<std::list<Symbol>>(node.value);
  Symbol content = nodel.front();
  nodel.pop_front();
  std::string filename = std::get<std::string>(nodel.front().value);
  std::ofstream out;
  out.open(filename, std::ios::app);
  out << std::boolalpha;
  using namespace matchit;
  Id<std::string> s;
  Id<int> i;
  Id<std::list<Symbol>> l;
  Id<bool> b;
  auto is_strlit = [](const std::string &s) -> bool {
    return !s.empty() && s[0] == '"' && s[s.length() - 1] == '"';
  };
  auto strlit_to_bare = [&](std::string s) -> std::string {
    if (is_strlit(s)) {
      s.erase(0, 1);
      s.erase(s.length() - 1, 1);
    }
    return s;
  };
  match(content.value)(
      pattern | as<int>(i) = [&] { out << *i; },
      pattern | as<std::string>(s) =
          [&] { out << strlit_to_bare(process_escapes(*s)); },
      pattern | as<bool>(b) = [&] { out << *b; },
      pattern | as<std::list<Symbol>>(l) =
          [&] {
            auto coutbuf = std::cout.rdbuf();
            std::cout.rdbuf(out.rdbuf());
            rec_print_ast(node);
            std::cout.rdbuf(coutbuf);
          });
  out.close();
  return Symbol("", true, Type::Boolean);
}

Symbol rewind_redirect_overwrite(Symbol node,
                                 const std::vector<std::string> &PATH) {
  auto nodel = std::get<std::list<Symbol>>(node.value);
  Symbol content = nodel.front();
  nodel.pop_front();
  std::string filename = std::get<std::string>(nodel.front().value);
  std::ofstream out;
  out.open(filename, std::ios::trunc);
  out << std::boolalpha;
  using namespace matchit;
  Id<std::string> s;
  Id<int> i;
  Id<std::list<Symbol>> l;
  Id<bool> b;
  auto is_strlit = [](const std::string &s) -> bool {
    return !s.empty() && s[0] == '"' && s[s.length() - 1] == '"';
  };
  auto strlit_to_bare = [&](std::string s) -> std::string {
    if (is_strlit(s)) {
      s.erase(0, 1);
      s.erase(s.length() - 1, 1);
    }
    return s;
  };
  match(content.value)(
      pattern | as<int>(i) = [&] { out << *i; },
      pattern | as<std::string>(s) =
          [&] { out << strlit_to_bare(process_escapes(*s)); },
      pattern | as<bool>(b) = [&] { out << *b; },
      pattern | as<std::list<Symbol>>(l) =
          [&] {
            auto coutbuf = std::cout.rdbuf();
            std::cout.rdbuf(out.rdbuf());
            rec_print_ast(node);
            std::cout.rdbuf(coutbuf);
          });
  out.close();
  return Symbol("", true, Type::Boolean);
}
