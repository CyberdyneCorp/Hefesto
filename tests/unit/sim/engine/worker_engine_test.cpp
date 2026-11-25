// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// WorkerEngine over an in-memory WorkerHost (openspec/changes/
// m9-lvs-simulation design D9; capability simulation-engine-embedding
// "Crash containment on desktop", "Worker channel limits", "Single engine
// instance per address space", "Deterministic engine initialisation").
// The fake channel plays the worker: it answers each request from a script
// and can send any frame, so every fault path is reachable without a
// process. tests/unit/platform/worker_host_simd_test.cpp runs the real
// hefesto-simd.
//
// Owner: SIMD.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "hefesto/platform/worker_host.hpp"
#include "hefesto/sim/engine/engine.hpp"
#include "hefesto/sim/engine/frames.hpp"
// The private header of sim/engine (worker::make with lowered limits).
#include "../../../../src/core/src/sim/engine/worker_engine.hpp"

namespace frames = hefesto::sim::frames;
namespace platform = hefesto::platform;
namespace sim = hefesto::sim;
using namespace std::chrono_literals;

namespace {

class FakeChannel;
using Script = std::function<void(const frames::Message& received, FakeChannel& worker)>;

class FakeChannel final : public platform::WorkerChannel {
 public:
  FakeChannel(Script script, std::shared_ptr<std::atomic<bool>> terminated)
      : script_(std::move(script)), terminated_flag_(std::move(terminated)) {}

  hefesto::diag::Status send(std::span<const std::byte> frame) override {
    if (closed_) {
      return hefesto::diag::make("PLATFORM-WORKER-002", "closed");
    }
    const frames::Decoded decoded = frames::decode(frame);
    REQUIRE(decoded.error == frames::Error::none);
    {
      std::lock_guard lock{mutex_};
      received_.push_back(*decoded.message);
    }
    if (script_) {
      script_(*decoded.message, *this);
    }
    return hefesto::diag::Done{};
  }
  void on_frame(FrameHandler handler) override {
    std::deque<std::vector<std::byte>> pending;
    {
      std::lock_guard lock{mutex_};
      frame_handler_ = std::move(handler);
      pending.swap(pending_);
    }
    for (const auto& frame : pending) {
      frame_handler_(frame);
    }
  }
  void on_exit(ExitHandler handler) override { exit_handler_ = std::move(handler); }
  void terminate() override {
    terminated_ = true;
    *terminated_flag_ = true;
    closed_ = true;
  }
  [[nodiscard]] std::int64_t process_id() const noexcept override { return 1; }

  // The worker's side.
  void emit(const frames::Message& message) { emit_raw(frames::encode(message)); }
  void emit_raw(std::vector<std::byte> body) {
    {
      std::lock_guard lock{mutex_};
      if (!frame_handler_) {
        pending_.push_back(std::move(body));
        return;
      }
    }
    frame_handler_(body);
  }
  void exit(platform::WorkerExit status) {
    closed_ = true;
    if (exit_handler_) {
      exit_handler_(status);
    }
  }

  std::vector<frames::Message> received() {
    std::lock_guard lock{mutex_};
    return received_;
  }
  std::atomic<bool> terminated_{false};

 private:
  Script script_;
  std::shared_ptr<std::atomic<bool>> terminated_flag_;  // outlives the channel
  std::mutex mutex_;
  FrameHandler frame_handler_;
  ExitHandler exit_handler_;
  std::deque<std::vector<std::byte>> pending_;
  std::vector<frames::Message> received_;
  std::atomic<bool> closed_{false};
};

class FakeHost final : public platform::WorkerHost {
 public:
  FakeHost(Script script, bool hello = true, std::uint32_t protocol = frames::kProtocolVersion)
      : script_(std::move(script)), hello_(hello), protocol_(protocol) {}

  [[nodiscard]] bool supported() const noexcept override { return true; }
  hefesto::diag::Outcome<std::unique_ptr<platform::WorkerChannel>> start(const platform::WorkerSpec& spec) override {
    spec_ = spec;
    auto channel = std::make_unique<FakeChannel>(script_, terminated);
    channel_ = channel.get();
    if (hello_) {
      channel->emit(frames::Hello{protocol_, "fake 1"});
    }
    return std::unique_ptr<platform::WorkerChannel>{std::move(channel)};
  }

  FakeChannel* channel_ = nullptr;  // dangles once the engine is gone
  // Set by terminate(); readable after a failed make() destroyed the channel.
  std::shared_ptr<std::atomic<bool>> terminated = std::make_shared<std::atomic<bool>>(false);
  platform::WorkerSpec spec_;

 private:
  Script script_;
  bool hello_;
  std::uint32_t protocol_;
};

// Replies ok to every request; a run sends two vectors and `loaded`.
void healthy(const frames::Message& message, FakeChannel& worker) {
  if (const auto* load = std::get_if<frames::Load>(&message)) {
    if ((load->flags & frames::kLoadLast) != 0) {
      worker.emit(frames::Reply{frames::kRequestLoad, true, ""});
    }
    return;
  }
  const auto& cmd = std::get<frames::Cmd>(message);
  worker.emit(frames::Reply{static_cast<std::uint8_t>(cmd.command), true, ""});
  if (cmd.command == frames::Command::run) {
    worker.emit(frames::StateChange{frames::State::running});
    worker.emit(frames::VecChunk{"time", "time", false, 3, 0, {0.0, 1.0}, {}});
    worker.emit(frames::VecChunk{"time", "time", false, 3, 2, {2.0}, {}});
    worker.emit(frames::VecChunk{"v(out)", "voltage", true, 1, 0, {0.5}, {0.25}});
    worker.emit(frames::VectorsDone{2});
    worker.emit(frames::StateChange{frames::State::loaded});
  }
}

struct Recorder {
  std::mutex mutex;
  std::vector<sim::EngineState> states;
  std::vector<std::string> output;

  sim::EngineCallbacks callbacks() {
    sim::EngineCallbacks c;
    c.on_state = [this](sim::EngineState s) {
      std::lock_guard lock{mutex};
      states.push_back(s);
    };
    c.on_output = [this](std::string_view line) {
      std::lock_guard lock{mutex};
      output.emplace_back(line);
    };
    return c;
  }
  bool saw(sim::EngineState s) {
    std::lock_guard lock{mutex};
    return std::find(states.begin(), states.end(), s) != states.end();
  }
};

sim::worker::Limits fast_limits() {
  sim::worker::Limits limits;
  limits.handshake = 300ms;
  limits.heartbeat_timeout = 300ms;
  limits.watchdog_period = 10ms;
  return limits;
}

std::unique_ptr<sim::Engine> start(FakeHost& host, const sim::worker::Limits& limits = {}) {
  sim::WorkerEngineOptions options;
  options.job_dir = "/jobs/1";
  options.readonly_roots = {"/pdks"};
  auto engine = sim::worker::make(&host, options, limits);
  REQUIRE(engine.ok());
  return std::move(engine).value();
}

}  // namespace

TEST_CASE("the worker spec carries the program, job directory and read-only roots",
          "[cap:simulation-engine-embedding][req:Worker confinement]") {
  FakeHost host{healthy};
  const auto engine = start(host);
  CHECK(host.spec_.program == "hefesto-simd");
  CHECK(host.spec_.job_dir == "/jobs/1");
  CHECK(host.spec_.readonly_roots == std::vector<std::string>{"/pdks"});
  CHECK(host.spec_.max_frame_bytes == frames::kMaxFrameBytes);
  CHECK(host.spec_.env.empty());
}

TEST_CASE("an unsupported host makes the engine unavailable with the platform diagnostic",
          "[cap:hefesto-architecture][req:Long-lived worker processes]") {
  platform::UnsupportedWorkerHost host;
  CHECK_FALSE(host.supported());
  const auto engine = sim::make_worker_engine(&host, {});
  REQUIRE_FALSE(engine.ok());
  CHECK(engine.error().code == "PLATFORM-UNSUPPORTED-001");
}

TEST_CASE("load, run and vectors go through the worker",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  FakeHost host{healthy};
  const auto engine = start(host);
  Recorder recorder;
  engine->set_callbacks(recorder.callbacks());
  CHECK_FALSE(engine->vectors().ok());
  CHECK(engine->vectors().error().code == "SIM-ENGINE-012");
  REQUIRE(engine->load({"* rc", "R1 a 0 1k"}).ok());
  REQUIRE(engine->run().ok());
  const auto vectors = engine->vectors();
  REQUIRE(vectors.ok());
  REQUIRE(vectors.value().size() == 2);
  CHECK(vectors.value()[0].name == "time");
  CHECK(vectors.value()[0].real == std::vector<double>{0.0, 1.0, 2.0});
  CHECK(vectors.value()[1].complex);
  CHECK(vectors.value()[1].imag == std::vector<double>{0.25});
  CHECK(recorder.saw(sim::EngineState::running));
  CHECK(recorder.saw(sim::EngineState::loaded));
}

TEST_CASE("a reused worker clears the last job and reseeds before the next load",
          "[cap:simulation-engine-embedding][req:Single engine instance per address space]") {
  FakeHost host{healthy};
  const auto engine = start(host);
  REQUIRE(engine->load({"* job 1"}).ok());
  REQUIRE(engine->run().ok());
  REQUIRE(engine->load({"* job 2"}).ok());
  const std::vector<frames::Message> received = host.channel_->received();
  REQUIRE(received.size() == 4);
  CHECK(std::get<frames::Load>(received[0]).seed == 1);
  CHECK(std::get<frames::Cmd>(received[1]).command == frames::Command::run);
  CHECK(std::get<frames::Cmd>(received[2]).command == frames::Command::reset);
  CHECK(std::get<frames::Load>(received[3]).seed == 1);
  CHECK(std::get<frames::Load>(received[3]).lines == std::vector<std::string>{"* job 2"});
}

TEST_CASE("rejections and engine busy are reported with their codes",
          "[cap:simulation-engine-embedding][req:Engine threading model]") {
  FakeHost host{[](const frames::Message& message, FakeChannel& worker) {
    if (std::holds_alternative<frames::Load>(message)) {
      worker.emit(frames::Reply{frames::kRequestLoad, false, "Error: unknown subckt: xinv"});
    } else {
      const auto command = static_cast<std::uint8_t>(std::get<frames::Cmd>(message).command);
      worker.emit(frames::Reply{command, false, command == 1 ? "engine busy" : "no analysis is running"});
    }
  }};
  const auto engine = start(host);
  const auto load = engine->load({"x1 a b xinv"});
  REQUIRE_FALSE(load.ok());
  CHECK(load.error().code == "SIM-ENGINE-009");
  CHECK(load.error().text == "Error: unknown subckt: xinv");
  CHECK(engine->run().error().code == "SIM-ENGINE-011");
  CHECK(engine->pause().error().code == "SIM-ENGINE-010");
}

TEST_CASE("the handshake must arrive in time and speak this protocol",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  SECTION("no hello") {
    FakeHost host{healthy, false};
    const auto engine = sim::worker::make(&host, {}, fast_limits());
    REQUIRE_FALSE(engine.ok());
    CHECK(engine.error().code == "SIM-ENGINE-002");
    CHECK(engine.error().text.starts_with("simulation engine terminated"));
    CHECK(*host.terminated);
  }
  SECTION("another protocol") {
    FakeHost host{healthy, true, frames::kProtocolVersion + 1};
    const auto engine = sim::worker::make(&host, {}, fast_limits());
    REQUIRE_FALSE(engine.ok());
    CHECK(engine.error().code == "SIM-ENGINE-003");
    CHECK(*host.terminated);
  }
}

TEST_CASE("a malformed, unknown or oversize frame terminates the worker",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  struct Case {
    std::vector<std::byte> body;
    const char* code;
  };
  const std::vector<Case> cases = {
      {{std::byte{6}, std::byte{1}}, "SIM-ENGINE-004"},
      {{std::byte{77}}, "SIM-ENGINE-006"},
      {frames::encode(frames::Output{frames::Stream::output, std::string(200, 'x')}), "SIM-ENGINE-005"},
      {frames::encode(frames::Hello{}), "SIM-ENGINE-004"},  // a second hello
      {frames::encode(frames::Cmd{frames::Command::run}), "SIM-ENGINE-004"},
  };
  for (const Case& c : cases) {
    FakeHost host{healthy};
    sim::worker::Limits limits;
    limits.max_frame_bytes = 128;
    const auto engine = start(host, limits);
    Recorder recorder;
    engine->set_callbacks(recorder.callbacks());
    host.channel_->emit_raw(c.body);
    CHECK(host.channel_->terminated_);
    CHECK(recorder.saw(sim::EngineState::failed));
    const auto run = engine->run();
    REQUIRE_FALSE(run.ok());
    CHECK(run.error().code == c.code);
    CHECK(run.error().text.starts_with("simulation engine terminated"));
    CHECK(engine->vectors().error().code == c.code);
  }
}

TEST_CASE("inconsistent vector chunks are a fault",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  FakeHost host{healthy};
  const auto engine = start(host);
  host.channel_->emit(frames::VecChunk{"a", "t", false, 4, 0, {1.0}, {}});
  host.channel_->emit(frames::VecChunk{"a", "t", false, 4, 3, {1.0}, {}});  // a gap
  host.channel_->emit(frames::VectorsDone{1});
  CHECK(host.channel_->terminated_);
  CHECK(engine->vectors().error().code == "SIM-ENGINE-004");
}

TEST_CASE("Stuck worker: a missed heartbeat terminates the worker",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  FakeHost host{healthy};
  const auto engine = start(host, fast_limits());
  Recorder recorder;
  engine->set_callbacks(recorder.callbacks());
  // Beats keep it alive...
  for (int i = 0; i < 10; ++i) {
    host.channel_->emit(frames::Beat{static_cast<std::uint64_t>(i)});
    std::this_thread::sleep_for(50ms);
  }
  CHECK_FALSE(host.channel_->terminated_);
  // ...and silence ends it.
  for (int i = 0; i < 100 && !host.channel_->terminated_; ++i) {
    std::this_thread::sleep_for(20ms);
  }
  CHECK(host.channel_->terminated_);
  CHECK(recorder.saw(sim::EngineState::failed));
  CHECK(engine->run().error().code == "SIM-ENGINE-007");
}

TEST_CASE("the worker's exit fails the job with simulation engine terminated",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  FakeHost host{healthy};
  const auto engine = start(host);
  Recorder recorder;
  engine->set_callbacks(recorder.callbacks());
  host.channel_->emit(frames::Output{frames::Stream::error, "Error: something broke"});
  platform::WorkerExit exit;
  exit.signal = 11;
  host.channel_->exit(exit);
  CHECK(recorder.saw(sim::EngineState::failed));
  const auto load = engine->load({"* next"});
  REQUIRE_FALSE(load.ok());
  CHECK(load.error().code == "SIM-ENGINE-008");
  CHECK(load.error().text.find("signal 11") != std::string::npos);
  CHECK(load.error().text.find("Error: something broke") != std::string::npos);
}

TEST_CASE("a failed state from the worker is a fault",
          "[cap:simulation-engine-embedding][req:Cancellation and fault states]") {
  FakeHost host{healthy};
  const auto engine = start(host);
  host.channel_->emit(frames::StateChange{frames::State::failed});
  CHECK(host.channel_->terminated_);
  CHECK(engine->run().error().code == "SIM-ENGINE-008");
}

TEST_CASE("the channel limits can be lowered, never raised",
          "[cap:simulation-engine-embedding][req:Worker channel limits]") {
  sim::worker::Limits high;
  high.max_frame_bytes = frames::kMaxFrameBytes * 2;
  high.handshake = 60s;
  high.heartbeat_timeout = 60s;
  const sim::worker::Limits out = sim::worker::clamped(high);
  CHECK(out.max_frame_bytes == frames::kMaxFrameBytes);
  CHECK(out.handshake == 10s);
  CHECK(out.heartbeat_timeout == 10s);
  sim::worker::Limits low;
  low.heartbeat_timeout = 1s;
  CHECK(sim::worker::clamped(low).heartbeat_timeout == 1s);
}

// Regression: the worker's confinement notes arrive right after hello, before
// the owner can set callbacks; they were dropped and never reached the job log.
TEST_CASE("job-log lines sent before set_callbacks reach the first on_output",
          "[cap:simulation-engine-embedding][req:Worker confinement]") {
  FakeHost host{healthy};
  const auto engine = start(host);
  host.channel_->emit(frames::Output{frames::Stream::output, "confinement: applied"});
  Recorder recorder;
  engine->set_callbacks(recorder.callbacks());
  REQUIRE(recorder.output.size() == 1);
  CHECK(recorder.output[0] == "confinement: applied");
  host.channel_->emit(frames::Output{frames::Stream::output, "later"});
  CHECK(recorder.output.size() == 2);
}

// Regression (M9 R1 integration): error-stream lines lost their stream on the
// way to EngineCallbacks, so JobManager never saw an `Error` line of a worker.
TEST_CASE("Error stream classification: error-stream frames reach on_error, also from the backlog",
          "[cap:simulation-engine-embedding][req:Callback contract]") {
  FakeHost host{healthy};
  std::vector<std::string> errors;
  Recorder recorder;
  const auto engine = start(host);
  host.channel_->emit(frames::Output{frames::Stream::error, "Error: early"});
  sim::EngineCallbacks callbacks = recorder.callbacks();
  callbacks.on_error = [&errors](std::string_view line) { errors.emplace_back(line); };
  engine->set_callbacks(std::move(callbacks));
  host.channel_->emit(frames::Output{frames::Stream::error, "Error: unknown subckt: xinv"});
  host.channel_->emit(frames::Output{frames::Stream::output, "Note: plain"});
  CHECK(errors == std::vector<std::string>{"Error: early", "Error: unknown subckt: xinv"});
  CHECK(recorder.output == std::vector<std::string>{"Note: plain"});
}

// Regression: vector chunks before VectorsDone were kept without limit, so a
// faulty worker could grow the core's memory without bound.
TEST_CASE("Result vectors beyond the result budget are a fault",
          "[cap:simulation-engine-embedding][req:Crash containment on desktop]") {
  FakeHost host{healthy};
  sim::worker::Limits limits;
  limits.max_result_bytes = 1024;
  const auto engine = start(host, limits);
  Recorder recorder;
  engine->set_callbacks(recorder.callbacks());
  host.channel_->emit(frames::VecChunk{"time", "time", false, 64, 0, {0.0}, {}});  // 64 + 8 + 512 bytes
  CHECK_FALSE(host.channel_->terminated_);
  host.channel_->emit(frames::VecChunk{"v(out)", "voltage", false, 64, 0, {0.0}, {}});
  CHECK(host.channel_->terminated_);
  CHECK(recorder.saw(sim::EngineState::failed));
  CHECK(engine->vectors().error().code == "SIM-ENGINE-013");
  sim::worker::Limits high;
  high.max_result_bytes = std::uint64_t{1} << 40;
  CHECK(sim::worker::clamped(high).max_result_bytes == std::uint64_t{4} << 30);
}

// Regression: set_callbacks published the callbacks before it delivered the
// backlog, so a line the reader delivered meanwhile came before older ones.
TEST_CASE("Backlog lines reach on_output before any later line",
          "[cap:simulation-engine-embedding][req:Callback contract]") {
  FakeHost host{healthy};
  const auto engine = start(host);
  host.channel_->emit(frames::Output{frames::Stream::output, "note 1"});
  host.channel_->emit(frames::Output{frames::Stream::output, "note 2"});
  std::mutex mutex;
  std::vector<std::string> lines;
  std::thread reader;
  std::atomic<bool> sent{false};
  sim::EngineCallbacks callbacks;
  callbacks.on_output = [&](std::string_view line) {
    {
      std::lock_guard lock{mutex};
      lines.emplace_back(line);
    }
    if (line == "note 1") {
      // The reader thread sends a new line while the backlog is delivered.
      reader = std::thread([&host, &sent] {
        host.channel_->emit(frames::Output{frames::Stream::output, "live"});
        sent = true;
      });
      for (int i = 0; i < 20 && !sent; ++i) {
        std::this_thread::sleep_for(10ms);
      }
    }
  };
  engine->set_callbacks(std::move(callbacks));
  reader.join();
  std::lock_guard lock{mutex};
  CHECK(lines == std::vector<std::string>{"note 1", "note 2", "live"});
}
