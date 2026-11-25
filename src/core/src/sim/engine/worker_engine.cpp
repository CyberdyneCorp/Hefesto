// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SIMD (task 5.4). See worker_engine.hpp.

#include "sim/engine/worker_engine.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "hefesto/platform/worker_host.hpp"

namespace hefesto::sim::worker {

namespace {

using Clock = std::chrono::steady_clock;

// SIM-ENGINE codes of this file (sim/engine/codes.inc).
constexpr std::string_view kNoHost = "SIM-ENGINE-001";
constexpr std::string_view kHandshake = "SIM-ENGINE-002";
constexpr std::string_view kProtocol = "SIM-ENGINE-003";
constexpr std::string_view kMalformed = "SIM-ENGINE-004";
constexpr std::string_view kOversize = "SIM-ENGINE-005";
constexpr std::string_view kUnknownKind = "SIM-ENGINE-006";
constexpr std::string_view kHeartbeat = "SIM-ENGINE-007";
constexpr std::string_view kTerminated = "SIM-ENGINE-008";
constexpr std::string_view kDeckRejected = "SIM-ENGINE-009";
constexpr std::string_view kCommandRejected = "SIM-ENGINE-010";
constexpr std::string_view kBusy = "SIM-ENGINE-011";
constexpr std::string_view kNoVectors = "SIM-ENGINE-012";

constexpr std::string_view kTerminatedText = "simulation engine terminated";
constexpr std::size_t kKeptErrorLines = 50;
constexpr std::size_t kBacklogLines = 1000;

std::string_view code_of(frames::Error error) {
  switch (error) {
    case frames::Error::oversize: return kOversize;
    case frames::Error::unknown_kind: return kUnknownKind;
    default: return kMalformed;
  }
}

std::string exit_text(const platform::WorkerExit& exit) {
  if (exit.frame_error) {
    return "the worker sent a bad frame (" + exit.detail + ")";
  }
  return exit.exited() ? "the worker exited with status " + std::to_string(exit.status)
                       : "the worker was killed by signal " + std::to_string(exit.signal);
}

// One vector being assembled from its chunks.
struct PartialVector {
  Vector vector;
  std::uint64_t length = 0;
};

class WorkerEngine final : public Engine {
 public:
  WorkerEngine(std::unique_ptr<platform::WorkerChannel> channel, const Limits& limits)
      : channel_(std::move(channel)), worker_(channel_.get()), limits_(limits) {
    channel_->on_frame([this](std::span<const std::byte> frame) { handle_frame(frame); });
    channel_->on_exit([this](const platform::WorkerExit& exit) { handle_exit(exit); });
  }

  WorkerEngine(const WorkerEngine&) = delete;
  WorkerEngine& operator=(const WorkerEngine&) = delete;

  ~WorkerEngine() override {
    {
      std::lock_guard lock{mutex_};
      closing_ = true;
    }
    changed_.notify_all();
    if (watchdog_.joinable()) {
      watchdog_.join();
    }
    channel_.reset();  // ends the worker and joins the reader thread while the members live
  }

  // Waits for the hello frame and checks the protocol version.
  diag::Status handshake() {
    std::unique_lock lock{mutex_};
    const bool done = changed_.wait_for(lock, limits_.handshake, [this] { return hello_ || fault_; });
    if (fault_) {
      return *fault_;
    }
    if (!done) {
      lock.unlock();
      return fail(kHandshake, "no version handshake within " + std::to_string(limits_.handshake.count()) + " ms");
    }
    if (hello_->protocol != frames::kProtocolVersion) {
      const std::uint32_t protocol = hello_->protocol;
      lock.unlock();
      return fail(kProtocol, "the worker speaks protocol " + std::to_string(protocol) + ", expected " +
                                 std::to_string(frames::kProtocolVersion));
    }
    last_beat_ = Clock::now();
    lock.unlock();
    watchdog_ = std::thread([this] { watch(); });
    return diag::Done{};
  }

  // The job-log lines that came before any on_output (the hello's
  // confinement notes) are delivered to the first one.
  void set_callbacks(EngineCallbacks callbacks) override {
    std::vector<std::string> backlog;
    std::function<void(std::string_view)> sink;
    {
      std::lock_guard lock{mutex_};
      callbacks_ = std::move(callbacks);
      if (callbacks_.on_output) {
        backlog.swap(backlog_);
        sink = callbacks_.on_output;
      }
    }
    for (const std::string& line : backlog) {
      sink(line);
    }
  }

  diag::Status load(const std::vector<std::string>& deck) override {
    std::lock_guard request_lock{request_mutex_};
    if (job_loaded_) {
      if (diag::Status reset = command(frames::Command::reset); !reset) {
        return reset;
      }
    }
    {
      std::lock_guard lock{mutex_};
      results_.clear();
      results_ready_ = false;
    }
    job_loaded_ = true;
    for (const frames::Load& chunk : frames::load_chunks(deck, 1, {})) {
      if (diag::Status sent = send(chunk); !sent) {
        return sent;
      }
    }
    return await_reply(frames::kRequestLoad);
  }

  diag::Status run() override {
    std::lock_guard request_lock{request_mutex_};
    {
      std::lock_guard lock{mutex_};
      results_ready_ = false;
    }
    return command(frames::Command::run);
  }
  diag::Status pause() override { return locked_command(frames::Command::pause); }
  diag::Status resume() override { return locked_command(frames::Command::resume); }
  diag::Status cancel() override { return locked_command(frames::Command::cancel); }

  diag::Outcome<std::vector<Vector>> vectors() const override {
    std::lock_guard lock{mutex_};
    if (fault_) {
      return *fault_;
    }
    if (!results_ready_) {
      return diag::make(kNoVectors, "no result vectors: no run has ended");
    }
    return results_;
  }

 private:
  // --- requests (caller threads, one at a time) --------------------------------------

  diag::Status locked_command(frames::Command which) {
    std::lock_guard request_lock{request_mutex_};
    return command(which);
  }

  diag::Status command(frames::Command which) {
    if (diag::Status sent = send(frames::Cmd{which}); !sent) {
      return sent;
    }
    return await_reply(static_cast<std::uint8_t>(which));
  }

  // Sends without holding mutex_: the worker may be blocked writing to us.
  diag::Status send(const frames::Message& message) {
    {
      std::lock_guard lock{mutex_};
      if (fault_) {
        return *fault_;
      }
    }
    const std::vector<std::byte> body = frames::encode(message);
    if (diag::Status sent = worker_->send(body); !sent) {
      return fail(kTerminated, std::string{kTerminatedText} + ": " + sent.error().text);
    }
    return diag::Done{};
  }

  diag::Status await_reply(std::uint8_t request) {
    std::unique_lock lock{mutex_};
    changed_.wait(lock, [this] { return !replies_.empty() || fault_ || closing_; });
    if (fault_) {
      return *fault_;
    }
    if (replies_.empty()) {
      return diag::make(kTerminated, std::string{kTerminatedText} + ": the engine is closing");
    }
    frames::Reply reply = std::move(replies_.front());
    replies_.pop_front();
    lock.unlock();
    if (reply.request != request) {
      return fail(kMalformed, "reply to request " + std::to_string(reply.request) + ", expected " +
                                  std::to_string(request));
    }
    if (reply.ok) {
      return diag::Done{};
    }
    if (reply.message == "engine busy") {
      return diag::make(kBusy, "engine busy");
    }
    return diag::make(request == frames::kRequestLoad ? kDeckRejected : kCommandRejected, reply.message);
  }

  // --- faults ---------------------------------------------------------------------------

  // Records the first fault, terminates the worker and reports `failed`.
  // Returns the fault's diagnostic (the first one when several race).
  diag::Diagnostic fail(std::string_view code, std::string text) {
    EngineCallbacks report;
    diag::Diagnostic fault;
    bool closing = false;
    {
      std::lock_guard lock{mutex_};
      closing = closing_;
      if (!fault_) {
        if (!text.starts_with(kTerminatedText)) {
          text = std::string{kTerminatedText} + ": " + text;
        }
        for (const std::string& line : error_lines_) {
          text += "\n" + line;
        }
        fault_ = diag::make(code, std::move(text));
        report = closing_ ? EngineCallbacks{} : callbacks_;
      }
      fault = *fault_;
    }
    changed_.notify_all();
    if (!closing) {
      worker_->terminate();  // while closing, the destructor ends the worker
    }
    if (report.on_state) {
      report.on_state(EngineState::failed);
    }
    return fault;
  }

  void watch() {
    std::unique_lock lock{mutex_};
    while (!closing_ && !fault_) {
      changed_.wait_for(lock, limits_.watchdog_period);
      if (!closing_ && !fault_ && Clock::now() - last_beat_ > limits_.heartbeat_timeout) {
        lock.unlock();
        fail(kHeartbeat, "no heartbeat for " + std::to_string(limits_.heartbeat_timeout.count()) + " ms");
        lock.lock();
      }
    }
  }

  // --- the reader thread -------------------------------------------------------------------

  void handle_exit(const platform::WorkerExit& exit) {
    if (exit.frame_error) {
      fail(exit.detail.starts_with("oversize") ? kOversize : kMalformed, exit_text(exit));
      return;
    }
    fail(kTerminated, exit_text(exit));
  }

  void handle_frame(std::span<const std::byte> body) {
    frames::Decoded decoded = frames::decode(body, limits_.max_frame_bytes);
    if (decoded.error != frames::Error::none) {
      fail(code_of(decoded.error), "the worker sent a bad frame (" + std::string{frames::error_name(decoded.error)} +
                                       ": " + decoded.detail + ")");
      return;
    }
    std::visit([this](auto& message) { handle(message); }, *decoded.message);
  }

  EngineCallbacks callbacks() {
    std::lock_guard lock{mutex_};
    return closing_ ? EngineCallbacks{} : callbacks_;
  }

  void handle(frames::Hello& hello) {
    {
      std::lock_guard lock{mutex_};
      if (!hello_) {
        hello_ = std::move(hello);
        changed_.notify_all();
        return;
      }
    }
    fail(kMalformed, "the worker sent a second hello");
  }

  void handle(frames::Output& output) {
    {
      std::lock_guard lock{mutex_};
      if (output.stream == frames::Stream::error) {
        error_lines_.push_back(output.text);
        if (error_lines_.size() > kKeptErrorLines) {
          error_lines_.pop_front();
        }
      }
      if (!callbacks_.on_output) {
        if (backlog_.size() < kBacklogLines) {
          backlog_.push_back(output.text);
        }
        return;
      }
    }
    if (const EngineCallbacks c = callbacks(); c.on_output) {
      c.on_output(output.text);
    }
  }

  void handle(frames::Progress& progress) {
    if (const EngineCallbacks c = callbacks(); c.on_progress) {
      c.on_progress(progress.status, progress.point);
    }
  }

  void handle(frames::StateChange& change) {
    if (change.state == frames::State::failed) {
      fail(kTerminated, "the engine faulted");
      return;
    }
    if (const EngineCallbacks c = callbacks(); c.on_state) {
      c.on_state(static_cast<EngineState>(change.state));
    }
  }

  void handle(frames::Reply& reply) {
    {
      std::lock_guard lock{mutex_};
      replies_.push_back(std::move(reply));
    }
    changed_.notify_all();
  }

  void handle(frames::VecChunk& chunk) {
    std::lock_guard lock{mutex_};
    if (chunk.offset == 0) {
      partial_.emplace_back();
      partial_.back().vector.name = std::move(chunk.name);
      partial_.back().vector.type = std::move(chunk.type);
      partial_.back().vector.complex = chunk.complex;
      partial_.back().length = chunk.length;
    } else if (partial_.empty() || partial_.back().vector.name != chunk.name ||
               partial_.back().vector.real.size() != chunk.offset || partial_.back().length != chunk.length) {
      bad_chunk_ = true;
      return;
    }
    Vector& vector = partial_.back().vector;
    vector.real.insert(vector.real.end(), chunk.real.begin(), chunk.real.end());
    vector.imag.insert(vector.imag.end(), chunk.imag.begin(), chunk.imag.end());
  }

  void handle(frames::VectorsDone& done) {
    bool consistent = false;
    {
      std::lock_guard lock{mutex_};
      consistent = !bad_chunk_ && partial_.size() == done.count &&
                   std::all_of(partial_.begin(), partial_.end(),
                               [](const PartialVector& p) { return p.vector.real.size() == p.length; });
      if (consistent) {
        results_.clear();
        for (PartialVector& p : partial_) {
          results_.push_back(std::move(p.vector));
        }
        results_ready_ = true;
      }
      partial_.clear();
      bad_chunk_ = false;
    }
    if (!consistent) {
      fail(kMalformed, "the worker sent an inconsistent set of vectors");
    }
  }

  void handle(frames::Beat& /*beat*/) {
    std::lock_guard lock{mutex_};
    last_beat_ = Clock::now();
  }

  void handle(frames::Load& /*load*/) { fail(kMalformed, "the worker sent a load frame"); }
  void handle(frames::Cmd& /*cmd*/) { fail(kMalformed, "the worker sent a cmd frame"); }

  std::unique_ptr<platform::WorkerChannel> channel_;
  platform::WorkerChannel* worker_;  // channel_.get(), stable while channel_ is reset
  Limits limits_;
  std::thread watchdog_;
  std::mutex request_mutex_;  // one request at a time
  bool job_loaded_ = false;   // under request_mutex_

  mutable std::mutex mutex_;  // everything below
  std::condition_variable changed_;
  EngineCallbacks callbacks_;
  std::optional<frames::Hello> hello_;
  std::optional<diag::Diagnostic> fault_;
  bool closing_ = false;
  Clock::time_point last_beat_ = Clock::now();
  std::deque<frames::Reply> replies_;
  std::deque<std::string> error_lines_;
  std::vector<std::string> backlog_;  // output before the first on_output
  std::vector<PartialVector> partial_;
  bool bad_chunk_ = false;
  std::vector<Vector> results_;
  bool results_ready_ = false;
};

}  // namespace

Limits clamped(const Limits& limits) {
  const Limits defaults;
  Limits out;
  out.max_frame_bytes = std::clamp(limits.max_frame_bytes, 1U, defaults.max_frame_bytes);
  const std::chrono::milliseconds one{1};
  out.handshake = std::clamp(limits.handshake, one, defaults.handshake);
  out.heartbeat_timeout = std::clamp(limits.heartbeat_timeout, one, defaults.heartbeat_timeout);
  out.watchdog_period = std::clamp(limits.watchdog_period, one, defaults.watchdog_period);
  return out;
}

diag::Outcome<std::unique_ptr<Engine>> make(platform::WorkerHost* host, const WorkerEngineOptions& options,
                                            const Limits& limits, const std::vector<std::string>& args) {
  if (host == nullptr) {
    return diag::make(kNoHost, "no worker host: simulation is unavailable");
  }
  const Limits bounded = clamped(limits);
  platform::WorkerSpec spec;
  spec.program = options.program;
  spec.args = args;
  spec.job_dir = options.job_dir;
  spec.readonly_roots = options.readonly_roots;
  spec.max_frame_bytes = bounded.max_frame_bytes;
  auto channel = host->start(spec);
  if (!channel) {
    return channel.error();
  }
  auto engine = std::make_unique<WorkerEngine>(std::move(channel).value(), bounded);
  if (diag::Status ready = engine->handshake(); !ready) {
    return ready.error();
  }
  return std::unique_ptr<Engine>{std::move(engine)};
}

}  // namespace hefesto::sim::worker
