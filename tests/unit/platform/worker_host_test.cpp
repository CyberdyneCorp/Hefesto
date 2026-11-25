// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The POSIX worker host (src/cli/worker_host_posix.hpp) and the core default
// (capability hefesto-architecture "Long-lived worker processes"), with
// hefesto_test_worker. Owner: PLATFORM.

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "hefesto/platform/worker_host.hpp"
#include "worker_host_posix.hpp"

namespace platform = hefesto::platform;
using namespace std::chrono_literals;

namespace {

std::vector<std::byte> text_frame(const std::string& text) {
  std::vector<std::byte> out;
  for (const char c : text) {
    out.push_back(static_cast<std::byte>(c));
  }
  return out;
}

std::string as_text(const std::vector<std::byte>& frame) {
  std::string out;
  for (const std::byte b : frame) {
    out.push_back(static_cast<char>(b));
  }
  return out;
}

// Collects the frames and the exit of one channel.
class Collector {
 public:
  void attach(platform::WorkerChannel& channel) {
    channel.on_frame([this](std::span<const std::byte> frame) {
      std::lock_guard lock{mutex_};
      frames_.emplace_back(frame.begin(), frame.end());
      changed_.notify_all();
    });
    channel.on_exit([this](const platform::WorkerExit& exit) {
      std::lock_guard lock{mutex_};
      exit_ = exit;
      changed_.notify_all();
    });
  }
  bool wait_frames(std::size_t count, std::chrono::milliseconds timeout = 10s) {
    std::unique_lock lock{mutex_};
    return changed_.wait_for(lock, timeout, [&] { return frames_.size() >= count; });
  }
  std::optional<platform::WorkerExit> wait_exit(std::chrono::milliseconds timeout = 10s) {
    std::unique_lock lock{mutex_};
    changed_.wait_for(lock, timeout, [&] { return exit_.has_value(); });
    return exit_;
  }
  std::vector<std::vector<std::byte>> frames() {
    std::lock_guard lock{mutex_};
    return frames_;
  }

 private:
  std::mutex mutex_;
  std::condition_variable changed_;
  std::vector<std::vector<std::byte>> frames_;
  std::optional<platform::WorkerExit> exit_;
};

platform::WorkerSpec spec(const std::string& mode) {
  platform::WorkerSpec s;
  s.program = HEFESTO_TEST_WORKER;
  s.args = {mode};
  return s;
}

std::unique_ptr<platform::WorkerChannel> start(hefesto::cli::PosixWorkerHost& host, const platform::WorkerSpec& s) {
  auto channel = host.start(s);
  REQUIRE(channel.ok());
  return std::move(channel).value();
}

}  // namespace

TEST_CASE("No process creation: the core default worker host is unsupported",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  platform::UnsupportedWorkerHost host;
  CHECK_FALSE(host.supported());
  const auto channel = host.start(spec("echo"));
  REQUIRE_FALSE(channel.ok());
  CHECK(channel.error().code == "PLATFORM-UNSUPPORTED-001");
}

TEST_CASE("frames go to the worker and come back in order",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  hefesto::cli::PosixWorkerHost host;
  CHECK(host.supported());
  const auto channel = start(host, spec("echo"));
  CHECK(channel->process_id() > 0);
  Collector collector;
  collector.attach(*channel);
  REQUIRE(channel->send(text_frame("one")).ok());
  REQUIRE(channel->send(text_frame("two")).ok());
  const std::vector<std::byte> large(3U * 1024U * 1024U, std::byte{0x5A});
  REQUIRE(channel->send(large).ok());
  REQUIRE(collector.wait_frames(3));
  const auto frames = collector.frames();
  CHECK(as_text(frames[0]) == "one");
  CHECK(as_text(frames[1]) == "two");
  CHECK(frames[2] == large);
}

TEST_CASE("frames sent before the handler is set wait for it",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  hefesto::cli::PosixWorkerHost host;
  const auto channel = start(host, spec("echo"));
  REQUIRE(channel->send(text_frame("early")).ok());
  std::this_thread::sleep_for(100ms);
  Collector collector;
  collector.attach(*channel);
  REQUIRE(collector.wait_frames(1));
  CHECK(as_text(collector.frames()[0]) == "early");
}

TEST_CASE("Worker exits: the exit status arrives and later sends fail without blocking",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  hefesto::cli::PosixWorkerHost host;
  const auto channel = start(host, spec("exit=3"));
  Collector collector;
  collector.attach(*channel);
  const auto exit = collector.wait_exit();
  REQUIRE(exit.has_value());
  CHECK(exit->exited());
  CHECK(exit->status == 3);
  CHECK_FALSE(exit->frame_error);
  const auto started = std::chrono::steady_clock::now();
  const auto sent = channel->send(text_frame("after exit"));
  REQUIRE_FALSE(sent.ok());
  CHECK(sent.error().code == "PLATFORM-WORKER-002");
  CHECK(std::chrono::steady_clock::now() - started < 2s);
}

TEST_CASE("a worker that exits while frames are in flight reports its status",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  hefesto::cli::PosixWorkerHost host;
  const auto channel = start(host, spec("exit-after"));
  Collector collector;
  collector.attach(*channel);
  REQUIRE(channel->send(text_frame("last")).ok());
  const auto exit = collector.wait_exit();
  REQUIRE(exit.has_value());
  CHECK(exit->status == 3);
  REQUIRE(collector.frames().size() == 1);
  // The pipe is closed now: a send fails (EPIPE, no SIGPIPE) instead of blocking.
  for (int i = 0; i < 3; ++i) {
    CHECK_FALSE(channel->send(text_frame("x")).ok());
  }
}

TEST_CASE("a crashing worker reports its signal",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  hefesto::cli::PosixWorkerHost host;
  const auto channel = start(host, spec("crash"));
  Collector collector;
  collector.attach(*channel);
  REQUIRE(channel->send(text_frame("boom")).ok());
  const auto exit = collector.wait_exit();
  REQUIRE(exit.has_value());
  CHECK_FALSE(exit->exited());
  CHECK(exit->signal == SIGABRT);
}

TEST_CASE("terminate ends a hung worker",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  hefesto::cli::PosixWorkerHost host;
  const auto channel = start(host, spec("hang"));
  Collector collector;
  collector.attach(*channel);
  CHECK_FALSE(collector.wait_exit(200ms).has_value());
  channel->terminate();
  channel->terminate();  // idempotent
  const auto exit = collector.wait_exit();
  REQUIRE(exit.has_value());
  CHECK(exit->signal == SIGKILL);
  CHECK(channel->send(text_frame("x")).error().code == "PLATFORM-WORKER-002");
}

TEST_CASE("destroying a channel ends its worker even with no handlers set",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  hefesto::cli::PosixWorkerHost host;
  auto channel = start(host, spec("hang"));
  const pid_t pid = static_cast<pid_t>(channel->process_id());
  const auto started = std::chrono::steady_clock::now();
  channel.reset();
  CHECK(std::chrono::steady_clock::now() - started < 5s);
  CHECK(::kill(pid, 0) != 0);  // reaped: no such process
}

TEST_CASE("an oversize or empty frame from the worker ends it as a frame error",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  for (const char* mode : {"oversize", "empty"}) {
    hefesto::cli::PosixWorkerHost host;
    const auto channel = start(host, spec(mode));
    Collector collector;
    collector.attach(*channel);
    const auto exit = collector.wait_exit();
    REQUIRE(exit.has_value());
    CHECK(exit->frame_error);
    CHECK(exit->signal == SIGKILL);
    CHECK(exit->detail.starts_with(std::string{mode} == "oversize" ? "oversize" : "empty"));
  }
}

TEST_CASE("frames larger than the limit, or empty, are not sent",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  hefesto::cli::PosixWorkerHost host;
  platform::WorkerSpec s = spec("echo");
  s.max_frame_bytes = 16;
  const auto channel = start(host, s);
  CHECK(channel->send(std::vector<std::byte>(17)).error().code == "PLATFORM-WORKER-001");
  CHECK(channel->send({}).error().code == "PLATFORM-WORKER-001");
  CHECK(channel->send(std::vector<std::byte>(16)).ok());
}

TEST_CASE("the worker runs in the job directory with the confinement arguments and an empty environment",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  const std::filesystem::path job =
      std::filesystem::temp_directory_path() / ("hefesto-worker-job-" + std::to_string(::getpid()));
  std::filesystem::create_directories(job);
  hefesto::cli::PosixWorkerHost host;
  platform::WorkerSpec s = spec("describe");
  s.job_dir = job.string();
  s.readonly_roots = {"/pdk/a", "/pdk/b"};
  const auto channel = start(host, s);
  Collector collector;
  collector.attach(*channel);
  REQUIRE(collector.wait_frames(1));
  const std::string text = as_text(collector.frames()[0]);
  CHECK(text == std::filesystem::canonical(job).string() + "\n5\n" + HEFESTO_TEST_WORKER + "\ndescribe\n--job-dir=" +
                    job.string() + "\n--readonly-root=/pdk/a\n--readonly-root=/pdk/b\n0");
  std::filesystem::remove_all(job);
}

TEST_CASE("a program that does not exist cannot be started",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  hefesto::cli::PosixWorkerHost host;
  platform::WorkerSpec s;
  s.program = "/nonexistent/hefesto-simd";
  const auto channel = host.start(s);
  REQUIRE_FALSE(channel.ok());
  CHECK(channel.error().code == "PLATFORM-PROCESS-001");
}
