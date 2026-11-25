// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The comparison of one cell pair (openspec/changes/m9-lvs-simulation
// design.md D8, "Area contracts" LVSALG; capability lvs-comparison-algorithm).
// Single-threaded per pair (Q-LVS-34); the pseudo-random labels come from a
// generator seeded per pair, so no result depends on a global.
//
// Owner: LVSALG.

#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lvs/cancel.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/setup/setup.hpp"

namespace hefesto::lvs {

struct CompareOptions {
  std::uint64_t seed = 0;     // fixed per pair (design D8)
  bool netgen_compat = true;  // design D5
  CancelToken cancel;
};

/// The status of a compared pair (lvs-reports "Result classification").
enum class PairStatus : std::uint8_t {
  match,
  match_property_errors,
  match_port_errors,
  match_port_property_errors,
  symmetric,
  black_box,
  mismatch,
};

/// The matched devices and nets, as (index in cell 1, index in cell 2).
struct Matching {
  std::vector<std::pair<std::uint32_t, std::uint32_t>> devices;
  std::vector<std::pair<std::uint32_t, std::uint32_t>> nets;
};

struct PairResult {
  std::string cell1;
  std::string cell2;
  PairStatus status = PairStatus::mismatch;
  Matching matching;
  bool cancelled = false;
  std::vector<diag::Diagnostic> diagnostics;
};

[[nodiscard]] PairResult compare_pair(const Cell& first, const Cell& second, const SetupState& setup,
                                      CompareOptions options);

}  // namespace hefesto::lvs
