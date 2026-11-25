// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: PLATFORM.

#include "worker_host_posix.hpp"

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "hefesto/diag/codes.hpp"
#include "hefesto/platform_posix/executable.hpp"

namespace hefesto::cli {

namespace {

using platform::WorkerChannel;
using platform::WorkerExit;
using platform::WorkerSpec;

constexpr std::size_t kLengthBytes = 4;

void close_fd(int& fd) noexcept {
  if (fd >= 0) {
    ::close(fd);
    fd = -1;
  }
}

// A pipe whose ends are close-on-exec; posix_spawn's dup2 onto 0 and 1
// clears the flag in the child.
bool make_pipe(std::array<int, 2>& fds) {
#if defined(__linux__)
  return ::pipe2(fds.data(), O_CLOEXEC) == 0;
#else
  if (::pipe(fds.data()) != 0) {
    return false;
  }
  for (const int fd : fds) {
    ::fcntl(fd, F_SETFD, FD_CLOEXEC);
  }
  return true;
#endif
}

// Reads exactly `size` bytes; false on end of file or error.
bool read_exact(int fd, std::byte* data, std::size_t size) {
  while (size > 0) {
    const ssize_t n = ::read(fd, data, size);
    if (n > 0) {
      data += n;
      size -= static_cast<std::size_t>(n);
    } else if (n == 0 || errno != EINTR) {
      return false;
    }
  }
  return true;
}

bool write_all(int fd, const std::byte* data, std::size_t size) {
  while (size > 0) {
    const ssize_t n = ::write(fd, data, size);
    if (n > 0) {
      data += n;
      size -= static_cast<std::size_t>(n);
    } else if (n < 0 && errno != EINTR) {
      return false;
    }
  }
  return true;
}

WorkerExit exit_of(int status) {
  WorkerExit exit;
  if (WIFEXITED(status)) {
    exit.status = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    exit.signal = WTERMSIG(status);
  }
  return exit;
}

class PosixWorkerChannel final : public WorkerChannel {
 public:
  PosixWorkerChannel(pid_t pid, int to_worker, int from_worker, std::uint32_t max_frame)
      : pid_(pid), to_worker_(to_worker), from_worker_(from_worker), max_frame_(max_frame) {
    reader_ = std::thread([this] { read_loop(); });
  }

  PosixWorkerChannel(const PosixWorkerChannel&) = delete;
  PosixWorkerChannel& operator=(const PosixWorkerChannel&) = delete;

  ~PosixWorkerChannel() override {
    terminate();
    {
      std::lock_guard lock{mutex_};
      stopping_ = true;
    }
    ready_.notify_all();
    if (reader_.get_id() == std::this_thread::get_id()) {
      reader_.detach();  // destroyed from a callback against the contract: do not deadlock
    } else if (reader_.joinable()) {
      reader_.join();
    }
    close_fd(to_worker_);
    close_fd(from_worker_);
  }

  diag::Status send(std::span<const std::byte> frame) override {
    if (frame.empty() || frame.size() > max_frame_) {
      return diag::make(diag::codes::kPlatformWorkerFrame,
                        "worker frame of " + std::to_string(frame.size()) + " bytes not sent (limit " +
                            std::to_string(max_frame_) + ")");
    }
    std::lock_guard lock{send_mutex_};
    if (closed_.load()) {
      return closed_error();
    }
    std::array<std::byte, kLengthBytes> prefix{};
    const auto size = static_cast<std::uint32_t>(frame.size());
    for (std::size_t i = 0; i < kLengthBytes; ++i) {
      prefix[i] = static_cast<std::byte>((size >> (8U * i)) & 0xFFU);
    }
    if (!write_all(to_worker_, prefix.data(), prefix.size()) || !write_all(to_worker_, frame.data(), frame.size())) {
      closed_.store(true);
      return closed_error();
    }
    return diag::Done{};
  }

  void on_frame(FrameHandler handler) override {
    {
      std::lock_guard lock{mutex_};
      frame_handler_ = std::move(handler);
    }
    ready_.notify_all();
  }

  void on_exit(ExitHandler handler) override {
    {
      std::lock_guard lock{mutex_};
      exit_handler_ = std::move(handler);
    }
    ready_.notify_all();
  }

  void terminate() override {
    closed_.store(true);
    std::lock_guard lock{mutex_};
    if (!reaped_) {
      ::kill(pid_, SIGKILL);
    }
  }

  [[nodiscard]] std::int64_t process_id() const noexcept override { return pid_; }

 private:
  static diag::Diagnostic closed_error() {
    return diag::make(diag::codes::kPlatformWorkerClosed, "the worker channel is closed; the frame was not sent");
  }

  // Waits until `handler` is set or the channel is being destroyed; returns
  // a copy (empty when stopping).
  template <typename Handler>
  Handler wait_for(const Handler& handler) {
    std::unique_lock lock{mutex_};
    ready_.wait(lock, [&] { return static_cast<bool>(handler) || stopping_; });
    return stopping_ ? Handler{} : handler;
  }

  // One frame from the worker: false at end of stream or on a frame error
  // (which is recorded in frame_error_).
  bool read_frame(std::vector<std::byte>& frame) {
    std::array<std::byte, kLengthBytes> prefix{};
    if (!read_exact(from_worker_, prefix.data(), prefix.size())) {
      return false;
    }
    std::uint32_t size = 0;
    for (std::size_t i = 0; i < kLengthBytes; ++i) {
      size |= static_cast<std::uint32_t>(prefix[i]) << (8U * i);
    }
    if (size == 0 || size > max_frame_) {
      frame_error_ = size == 0 ? std::string{"empty frame"}
                               : "oversize frame of " + std::to_string(size) + " bytes (limit " +
                                     std::to_string(max_frame_) + ")";
      return false;
    }
    frame.resize(size);
    return read_exact(from_worker_, frame.data(), frame.size());
  }

  void read_loop() {
    std::vector<std::byte> frame;
    while (read_frame(frame)) {
      if (const FrameHandler handler = wait_for(frame_handler_)) {
        handler(frame);
      }
    }
    closed_.store(true);
    if (!frame_error_.empty()) {
      terminate();
    }
    WorkerExit exit = reap();
    exit.frame_error = !frame_error_.empty();
    exit.detail = frame_error_;
    if (const ExitHandler handler = wait_for(exit_handler_)) {
      handler(exit);
    }
  }

  // Waits for the worker to end without reaping it, so terminate() never
  // signals a recycled process id, then reaps it under the lock.
  WorkerExit reap() {
    siginfo_t info{};
    while (::waitid(P_PID, static_cast<id_t>(pid_), &info, WEXITED | WNOWAIT) < 0 && errno == EINTR) {
    }
    std::lock_guard lock{mutex_};
    int status = 0;
    while (::waitpid(pid_, &status, 0) < 0 && errno == EINTR) {
    }
    reaped_ = true;
    return exit_of(status);
  }

  pid_t pid_;
  int to_worker_;
  int from_worker_;
  std::uint32_t max_frame_;
  std::atomic<bool> closed_{false};
  std::mutex send_mutex_;
  std::mutex mutex_;  // guards the handlers, stopping_ and reaped_
  std::condition_variable ready_;
  FrameHandler frame_handler_;
  ExitHandler exit_handler_;
  bool stopping_ = false;
  bool reaped_ = false;
  std::string frame_error_;  // reader thread only
  std::thread reader_;
};

std::string resolve_program(const std::string& program) {
  if (program.find('/') != std::string::npos) {
    return program;
  }
  const platform::Path dir = platform_posix::executable_dir();
  return dir.empty() ? program : (dir / program).string();
}

std::vector<std::string> worker_arguments(const WorkerSpec& spec, const std::string& program) {
  std::vector<std::string> argv{program};
  argv.insert(argv.end(), spec.args.begin(), spec.args.end());
  if (!spec.job_dir.empty()) {
    argv.push_back("--job-dir=" + spec.job_dir);
  }
  for (const std::string& root : spec.readonly_roots) {
    argv.push_back("--readonly-root=" + root);
  }
  return argv;
}

std::vector<char*> pointers(std::vector<std::string>& strings) {
  std::vector<char*> result;
  result.reserve(strings.size() + 1);
  for (std::string& s : strings) {
    result.push_back(s.data());
  }
  result.push_back(nullptr);
  return result;
}

class SpawnSetup {
 public:
  SpawnSetup() {
    ::posix_spawn_file_actions_init(&actions_);
    ::posix_spawnattr_init(&attr_);
  }
  SpawnSetup(const SpawnSetup&) = delete;
  SpawnSetup& operator=(const SpawnSetup&) = delete;
  ~SpawnSetup() {
    ::posix_spawn_file_actions_destroy(&actions_);
    ::posix_spawnattr_destroy(&attr_);
  }

  void configure(int child_stdin, int child_stdout, const std::string& job_dir) {
    ::posix_spawn_file_actions_adddup2(&actions_, child_stdin, STDIN_FILENO);
    ::posix_spawn_file_actions_adddup2(&actions_, child_stdout, STDOUT_FILENO);
    short flags = POSIX_SPAWN_SETSIGDEF;
#if defined(__APPLE__)
    // Every other descriptor is closed in the worker, whoever opened it.
    flags = static_cast<short>(flags | POSIX_SPAWN_CLOEXEC_DEFAULT);
    ::posix_spawn_file_actions_addinherit_np(&actions_, STDERR_FILENO);
#elif defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34))
    ::posix_spawn_file_actions_addclosefrom_np(&actions_, STDERR_FILENO + 1);
#endif
    ::posix_spawnattr_setflags(&attr_, flags);
    sigset_t defaults;
    sigemptyset(&defaults);
    sigaddset(&defaults, SIGPIPE);
    ::posix_spawnattr_setsigdefault(&attr_, &defaults);
    if (!job_dir.empty()) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      ::posix_spawn_file_actions_addchdir_np(&actions_, job_dir.c_str());
#pragma GCC diagnostic pop
    }
  }

  [[nodiscard]] const posix_spawn_file_actions_t* actions() const noexcept { return &actions_; }
  [[nodiscard]] const posix_spawnattr_t* attr() const noexcept { return &attr_; }

 private:
  posix_spawn_file_actions_t actions_{};
  posix_spawnattr_t attr_{};
};

std::mutex& spawn_mutex() {
  static std::mutex mutex;
  return mutex;
}

diag::Diagnostic start_error(const std::string& program, std::string_view what, int error) {
  return diag::make(diag::codes::kPlatformProcess,
                    "cannot " + std::string{what} + " worker '" + program + "': " + std::strerror(error));
}

}  // namespace

PosixWorkerHost::PosixWorkerHost() {
  std::signal(SIGPIPE, SIG_IGN);
}

diag::Outcome<std::unique_ptr<WorkerChannel>> PosixWorkerHost::start(const WorkerSpec& spec) {
  const std::string program = resolve_program(spec.program);
  std::vector<std::string> args = worker_arguments(spec, program);
  std::vector<std::string> env;
  for (const auto& [name, value] : spec.env) {
    env.push_back(name + "=" + value);
  }
  std::vector<char*> argv = pointers(args);
  std::vector<char*> envp = pointers(env);

  std::lock_guard lock{spawn_mutex()};
  std::array<int, 2> in{-1, -1};
  std::array<int, 2> out{-1, -1};
  if (!make_pipe(in) || !make_pipe(out)) {
    const int error = errno;
    for (int& fd : in) {
      close_fd(fd);
    }
    for (int& fd : out) {
      close_fd(fd);
    }
    return start_error(program, "create pipes for", error);
  }
  SpawnSetup setup;
  setup.configure(in[0], out[1], spec.job_dir);
  pid_t pid = 0;
  const int rc = ::posix_spawn(&pid, program.c_str(), setup.actions(), setup.attr(), argv.data(), envp.data());
  close_fd(in[0]);
  close_fd(out[1]);
  if (rc != 0) {
    close_fd(in[1]);
    close_fd(out[0]);
    return start_error(program, "start", rc);
  }
  return std::unique_ptr<WorkerChannel>{std::make_unique<PosixWorkerChannel>(pid, in[1], out[0], spec.max_frame_bytes)};
}

}  // namespace hefesto::cli
