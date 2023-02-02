#pragma once
// Copyright 2023 Sofia Cerasuoli (@SwitchAxe)
/*
  This file is part of Rewind.
  Rewind is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation, either version 3 of the license, or (at your
  option) any later version.
  Rewind is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along with
  Rewind. If not, see <https://www.gnu.org/licenses/>.
*/
#include "evaluator.hpp"
#include "src/procedures.hpp"
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

std::string to_str(Symbol sym) {
  const auto is_strlit = [](std::string s) -> bool {
    return (s.size() > 1) && (s[0] == '"') && (s[s.size() - 1] == '"');
  };
  using namespace matchit;
  Id<std::string> s;
  Id<long long int> in;
  Id<long long unsigned int> un;
  Id<bool> b;
#define p pattern
  std::string arg = match(sym.value)(
      p | as<std::string>(s) = [&] { return *s; },
      p | as<long long int>(in) = [&] { return std::to_string(*in); },
      p | as<long long unsigned int>(un) = [&] { return std::to_string(*un); },
      p | as<bool>(b) = [&] { return (*b == true) ? "true" : "false"; });
  if (is_strlit(arg)) {
    arg = arg.substr(1, arg.size() - 2);
  }

#undef p
  return arg;
}
Symbol rewind_call_ext_program(Symbol node,
                               const std::vector<std::string> &PATH,
                               bool must_pipe, int pipe_fd_out,
                               int pipe_fd_in) {
  const auto is_strlit = [](std::string s) -> bool {
    return (s.size() > 1) && (s[0] == '"') && (s[s.size() - 1] == '"');
  };
  std::list<Symbol> nodel = std::get<std::list<Symbol>>(node.value);
  int total_size = 1;
  for (auto &e : nodel) {
    e = eval(e, PATH);
    total_size += (e.type == Type::List)
                      ? std::get<std::list<Symbol>>(e.value).size()
                      : 1;
  }
  std::string prog = std::get<std::string>(nodel.front().value);
  char *argv[total_size];
  argv[0] = const_cast<char *>(prog.c_str());
  nodel.pop_front();
  Symbol cur_arg;
  int i = 1;
  for (auto cur : nodel) {
    if (cur.type == Type::List) {
      for (auto e : std::get<std::list<Symbol>>(cur.value)) {
        std::string arg = to_str(e);
        argv[i] = (char *)malloc(arg.length() + 1);
        std::strcpy(argv[i], arg.c_str());
        i++;
#undef p
      }
    } else {
      cur_arg = cur;
      std::string arg = to_str(cur_arg);
      argv[i] = (char *)malloc(arg.length() + 1);
      std::strcpy(argv[i], arg.c_str());
      i++;
    }
#undef p
  }
  argv[i] = nullptr;
  int status = 0;
  int pid = fork();
  active_pids.push_back(pid);
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
    if (pipe_fd_out) {
      close(pipe_fd_out);
    }
    if (pipe_fd_in)
      close(pipe_fd_in);
    for (int idx = 1; argv[idx] != nullptr; ++idx) {
      free(argv[idx]);
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
    auto status = rewind_call_ext_program(last, PATH, true, fd[1], 0);
    close(fd[1]);
    char buf[1024];
    int cnt = 0;
    std::string result;
    while ((cnt = read(fd[0], buf, 1023))) {
      if (cnt == -1)
        throw std::logic_error{"Read failed in a pipe!\n"};
      buf[cnt] = '\0';
      std::string tmp{buf};
      result += tmp;
    }
    close(fd[0]);
    while (wait(nullptr) != -1)
      ;
    return Symbol("", result, Type::String);
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
  old_read_end = dup(fd[0]);
  pipe(fd);
  status = rewind_call_ext_program(last, PATH, true, fd[1], old_read_end);
  char buf[1024];
  std::string result;
  if (std::get<long long int>(status.value) == -1) {
    return Symbol("", "Null", Type::String);
  }
  close(fd[1]);
  close(old_read_end);
  int cnt;
  while ((cnt = read(fd[0], buf, 1023))) {
    if (cnt == -1)
      throw std::logic_error{"Read failed in a pipe!\n"};
    buf[cnt] = '\0';
    std::string tmp{buf};
    result += tmp;
  }
  while (wait(nullptr) != -1)
    ;
  close(fd[0]);
  if (result.back() == '\n') {
    result.pop_back();
  }
  return Symbol("", result, Type::String);
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
  Id<long long int> i;
  Id<long long unsigned int> u;
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
      pattern | as<long long int>(i) = [&] { out << *i; },
      pattern | as<long long unsigned int>(u) = [&] { out << *u; },
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
  Id<long long int> i;
  Id<long long unsigned int> u;
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
      pattern | as<long long int>(i) = [&] { out << *i; },
      pattern | as<long long unsigned int>(u) = [&] { out << *u; },
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
