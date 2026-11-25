// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the simulation worker frames (capability
// simulation-engine-embedding "Worker channel limits": "no frame content,
// however malformed, SHALL crash the core or make it read outside the
// frame"; openspec/changes/m9-lvs-simulation design D9, D20).
//
// Two parts, on the same input:
//   1. the codec: decode the input as one frame body; a decoded frame must
//      encode back to exactly the same bytes;
//   2. the core's WorkerEngine: the input is split into length-prefixed
//      frames (as a worker would send them) and fed, after a valid hello, to
//      a WorkerEngine over an in-memory channel; then the engine is queried
//      (vectors, and a run whose send fails).
//
// Written by SIMD; tests/fuzz/ is shared (INTEGRATION).

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "hefesto/platform/worker_host.hpp"
#include "hefesto/sim/engine/engine.hpp"
#include "hefesto/sim/engine/frames.hpp"
// The private header of sim/engine (worker::make with limits).
#include "../../src/core/src/sim/engine/worker_engine.hpp"

namespace {

namespace frames = hefesto::sim::frames;
namespace platform = hefesto::platform;

constexpr std::uint32_t kLimit = 1U << 16U;

class FuzzChannel final : public platform::WorkerChannel {
 public:
  hefesto::diag::Status send(std::span<const std::byte> /*frame*/) override {
    return hefesto::diag::make("PLATFORM-WORKER-002", "fuzz: no worker");
  }
  // A well-formed hello as soon as the engine listens, so the handshake passes.
  void on_frame(FrameHandler handler) override {
    frame_ = std::move(handler);
    frame_(frames::encode(frames::Hello{frames::kProtocolVersion, "fuzz"}));
  }
  void on_exit(ExitHandler handler) override { exit_ = std::move(handler); }
  void terminate() override {}
  [[nodiscard]] std::int64_t process_id() const noexcept override { return 1; }

  FrameHandler frame_;
  ExitHandler exit_;
};

class FuzzHost final : public platform::WorkerHost {
 public:
  [[nodiscard]] bool supported() const noexcept override { return true; }
  hefesto::diag::Outcome<std::unique_ptr<platform::WorkerChannel>> start(const platform::WorkerSpec& /*spec*/) override {
    auto channel = std::make_unique<FuzzChannel>();
    channel_ = channel.get();
    return std::unique_ptr<platform::WorkerChannel>{std::move(channel)};
  }
  FuzzChannel* channel_ = nullptr;
};

void codec(std::span<const std::byte> body) {
  const frames::Decoded decoded = frames::decode(body, kLimit);
  if (decoded.error != frames::Error::none) {
    return;
  }
  const std::vector<std::byte> again = frames::encode(*decoded.message);
  if (!std::equal(again.begin(), again.end(), body.begin(), body.end())) {
    __builtin_trap();  // the codec is not canonical
  }
}

void engine(std::span<const std::byte> input) {
  FuzzHost host;
  hefesto::sim::worker::Limits limits;
  limits.max_frame_bytes = kLimit;
  auto made = hefesto::sim::worker::make(&host, {}, limits);
  if (!made.ok()) {
    __builtin_trap();  // the channel always says hello
  }
  std::size_t at = 0;
  while (at + 4 <= input.size()) {
    const auto [size, error] = frames::parse_length(std::span<const std::byte, 4>{input.data() + at, 4}, kLimit);
    at += 4;
    if (error != frames::Error::none || size > input.size() - at) {
      break;
    }
    host.channel_->frame_(input.subspan(at, size));
    at += size;
  }
  (void)made.value()->vectors();
  (void)made.value()->run();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::span<const std::byte> input{reinterpret_cast<const std::byte*>(data), size};  // NOLINT
  codec(input);
  engine(input);
  return 0;
}
