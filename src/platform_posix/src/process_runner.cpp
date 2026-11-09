// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/platform_posix/process_runner.hpp"

#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include "hefesto/diag/codes.hpp"

extern char** environ;  // NOLINT(readability-redundant-declaration)

namespace hefesto::platform_posix {

namespace {

using platform::ProcessOutput;
using platform::ProcessRequest;

diag::Diagnostic process_error(const ProcessRequest& request, std::string_view what, int error) {
  return diag::make(diag::codes::kPlatformProcess, "cannot " + std::string{what} + " '" + request.program +
                                                       "': " + std::strerror(error));
}

// Creates a pipe whose ends are close-on-exec, so a child spawned by another
// thread never inherits them (it would hold our write end open and our reader
// would never see EOF). posix_spawn's dup2 onto 0-2 clears the flag there.
bool make_cloexec_pipe(std::array<int, 2>& fds) {
#if defined(__linux__)
  return ::pipe2(fds.data(), O_CLOEXEC) == 0;
#else
  // No pipe2 on macOS: run() holds spawn_mutex() from pipe creation to spawn,
  // so no other run() can spawn between these calls.
  if (::pipe(fds.data()) != 0) {
    return false;
  }
  for (const int fd : fds) {
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
  }
  return true;
#endif
}

std::mutex& spawn_mutex() {
  static std::mutex mutex;
  return mutex;
}

// A close-on-exec pipe whose ends close themselves.
class Pipe {
 public:
  Pipe() {
    if (!make_cloexec_pipe(fds_)) {
      fds_ = {-1, -1};
    }
  }
  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;
  ~Pipe() {
    close_read();
    close_write();
  }

  [[nodiscard]] bool valid() const noexcept { return fds_[0] >= 0; }
  [[nodiscard]] int read_end() const noexcept { return fds_[0]; }
  [[nodiscard]] int write_end() const noexcept { return fds_[1]; }
  void close_read() noexcept { close_fd(fds_[0]); }
  void close_write() noexcept { close_fd(fds_[1]); }

 private:
  static void close_fd(int& fd) noexcept {
    if (fd >= 0) {
      ::close(fd);
      fd = -1;
    }
  }
  std::array<int, 2> fds_{-1, -1};
};

class FileActions {
 public:
  FileActions() { ::posix_spawn_file_actions_init(&actions_); }
  FileActions(const FileActions&) = delete;
  FileActions& operator=(const FileActions&) = delete;
  ~FileActions() { ::posix_spawn_file_actions_destroy(&actions_); }

  void dup_to(int fd, int target) { ::posix_spawn_file_actions_adddup2(&actions_, fd, target); }
  // The _np spelling exists on every supported host (glibc >= 2.29, macOS
  // >= 10.15); macOS 26 deprecates it in favour of a name older hosts lack.
  void chdir(const std::string& dir) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    ::posix_spawn_file_actions_addchdir_np(&actions_, dir.c_str());
#pragma GCC diagnostic pop
  }
  [[nodiscard]] const posix_spawn_file_actions_t* get() const noexcept { return &actions_; }

 private:
  posix_spawn_file_actions_t actions_{};
};

struct ChildPipes {
  Pipe in;
  Pipe out;
  Pipe err;
  [[nodiscard]] bool valid() const noexcept { return in.valid() && out.valid() && err.valid(); }
};

void configure_actions(FileActions& actions, const ChildPipes& pipes, const ProcessRequest& request) {
  actions.dup_to(pipes.in.read_end(), STDIN_FILENO);
  actions.dup_to(pipes.out.write_end(), STDOUT_FILENO);
  actions.dup_to(pipes.err.write_end(), STDERR_FILENO);
  // Every pipe fd is close-on-exec, so the child keeps only 0, 1 and 2.
  if (request.working_directory) {
    actions.chdir(request.working_directory->string());
  }
}

std::vector<char*> make_argv(const ProcessRequest& request, std::vector<std::string>& storage) {
  storage.clear();
  storage.push_back(request.program);
  storage.insert(storage.end(), request.args.begin(), request.args.end());
  std::vector<char*> argv;
  for (auto& s : storage) {
    argv.push_back(s.data());
  }
  argv.push_back(nullptr);
  return argv;
}

// Moves data between the parent ends of the pipes until both outputs close.
class Pump {
 public:
  Pump(ChildPipes& pipes, std::string_view input, ProcessOutput& output)
      : pipes_(pipes), input_(input), output_(output) {}

  void run() {
    if (input_.empty()) {
      pipes_.in.close_write();
    }
    while (pipes_.out.read_end() >= 0 || pipes_.err.read_end() >= 0) {
      std::array<pollfd, 3> fds = {pollfd{pipes_.out.read_end(), POLLIN, 0}, pollfd{pipes_.err.read_end(), POLLIN, 0},
                                   pollfd{pipes_.in.write_end(), POLLOUT, 0}};
      if (::poll(fds.data(), fds.size(), -1) < 0) {
        if (errno == EINTR) {
          continue;
        }
        return;
      }
      drain(fds[0], pipes_.out, output_.stdout_data);
      drain(fds[1], pipes_.err, output_.stderr_data);
      feed(fds[2]);
    }
  }

 private:
  static void drain(const pollfd& fd, Pipe& pipe, std::string& sink) {
    if (fd.fd < 0 || (fd.revents & (POLLIN | POLLHUP | POLLERR)) == 0) {
      return;
    }
    std::array<char, 4096> buffer{};
    const ssize_t n = ::read(fd.fd, buffer.data(), buffer.size());
    if (n > 0) {
      sink.append(buffer.data(), static_cast<std::size_t>(n));
    } else if (n == 0 || errno != EINTR) {
      pipe.close_read();
    }
  }

  void feed(const pollfd& fd) {
    if (fd.fd < 0 || fd.revents == 0) {
      return;
    }
    const ssize_t n = (fd.revents & POLLOUT) != 0 ? ::write(fd.fd, input_.data(), input_.size()) : -1;
    if (n > 0) {
      input_.remove_prefix(static_cast<std::size_t>(n));
    }
    if (n <= 0 || input_.empty()) {
      pipes_.in.close_write();
    }
  }

  ChildPipes& pipes_;
  std::string_view input_;
  ProcessOutput& output_;
};

int wait_for(pid_t pid) {
  int status = 0;
  while (::waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) {
      return -1;
    }
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;
}

}  // namespace

PosixProcessRunner::PosixProcessRunner() {
  std::signal(SIGPIPE, SIG_IGN);
}

diag::Outcome<ProcessOutput> PosixProcessRunner::run(const ProcessRequest& request) {
  std::vector<std::string> storage;
  std::vector<char*> argv = make_argv(request, storage);
  std::unique_lock spawn_lock{spawn_mutex()};
  ChildPipes pipes;
  if (!pipes.valid()) {
    return process_error(request, "create pipes for", errno);
  }
  FileActions actions;
  configure_actions(actions, pipes, request);

  pid_t pid = 0;
  const int rc = ::posix_spawnp(&pid, request.program.c_str(), actions.get(), nullptr, argv.data(), environ);
  spawn_lock.unlock();
  if (rc != 0) {
    return process_error(request, "start", rc);
  }
  pipes.in.close_read();
  pipes.out.close_write();
  pipes.err.close_write();

  ProcessOutput output;
  Pump{pipes, request.stdin_data, output}.run();
  output.exit_code = wait_for(pid);
  return output;
}

}  // namespace hefesto::platform_posix
