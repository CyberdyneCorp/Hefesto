// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// A simulation job (openspec/changes/m9-lvs-simulation design.md D12,
// "Area contracts" SIMENG; capability simulation-analyses-and-results "Job
// lifecycle").
//
// Owner: SIMENG.

#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "hefesto/sim/probe/net_name_map.hpp"

namespace hefesto::sim {

/// `<YYYYMMDD>-<HHMMSS>-<testbench>-<corner>[-<n>]`, or `<run set id>-p<index>`
/// for a point of a run set.
using JobId = std::string;

/// Added in the M9 R2 fix pass: a job id names a folder of the result store
/// (`<root>/<job id>/`), so it must be one safe path component: 1 to 255
/// bytes, not `.` or `..`, with no `/`, backslash, `:` or control character.
[[nodiscard]] inline bool is_safe_job_id(std::string_view id) noexcept {
  if (id.empty() || id.size() > 255 || id == "." || id == "..") {
    return false;
  }
  return std::none_of(id.begin(), id.end(), [](char c) {
    const auto u = static_cast<unsigned char>(c);
    return u < 0x20 || u == 0x7F || c == '/' || c == '\\' || c == ':';
  });
}

/// queued -> preparing -> loading -> running (<-> paused) -> one final state.
enum class JobState : std::uint8_t { queued, preparing, loading, running, paused, completed, failed, cancelled };

struct Job {
  JobId id;
  JobState state = JobState::queued;
  std::string dut;  // the DUT cell of a layout source; empty otherwise
  /// The DUT's net-name map; nullptr for a raw or imported deck.
  std::shared_ptr<const NetNameMap> names;
};

}  // namespace hefesto::sim
