// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Frame input and output of hefesto-simd over its standard input and output
// (openspec/changes/m9-lvs-simulation design D9; the codec is
// hefesto/sim/engine/frames.hpp).
//
// Owner: SIMD.

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "hefesto/sim/engine/frames.hpp"

namespace hefesto::simd {

/// Writes whole frames to one descriptor; thread-safe, so the heartbeat
/// thread, the engine thread and the engine callbacks can share it.
class FrameWriter {
 public:
  explicit FrameWriter(int fd) : fd_(fd) {}

  /// False when the core has gone (the write failed).
  bool write(const sim::frames::Message& message);

 private:
  int fd_;
  std::mutex mutex_;
};

enum class ReadResult : std::uint8_t { frame, end, error };

/// Reads one frame body from `fd` into `body`. `error` (with `detail` set)
/// for a zero or oversize length; `end` at end of input or a short read.
ReadResult read_frame(int fd, std::vector<std::byte>& body, std::string& detail,
                      std::uint32_t limit = sim::frames::kMaxFrameBytes);

}  // namespace hefesto::simd
