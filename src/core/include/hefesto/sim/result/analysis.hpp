// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Analysis cards (openspec/changes/m9-lvs-simulation design.md D12;
// capability simulation-analyses-and-results "Supported analyses"). Added
// in M9 R2 (task 13.3): each analysis is validated and emitted as exactly
// its card, numbers in C `%.12g`, optional fields omitted from the end.
//
// Owner: SIMRES.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"

namespace hefesto::sim {

struct DcSweep {
  std::string source;
  double start = 0.0;
  double stop = 0.0;
  double step = 0.0;
};

struct Analysis {
  enum class Kind : std::uint8_t { op, dc, ac, tran, noise, tf };
  Kind kind = Kind::op;
  // dc
  DcSweep sweep1;
  std::optional<DcSweep> sweep2;
  // ac, noise
  std::string sweep = "dec";  // dec, oct or lin
  std::int64_t points = 0;
  double fstart = 0.0;
  double fstop = 0.0;
  // tran
  double tstep = 0.0;
  double tstop = 0.0;
  std::optional<double> tstart;
  std::optional<double> tmax;
  bool uic = false;
  // noise: v(<output>[,<reference>]) and the input source; tf: output and input
  std::string output;
  std::string reference;
  std::string input;
};

/// The card of `analysis`, or SIM-RESULT-010 naming the analysis and the
/// parameter (e.g. `ac: fstart must be > 0 for dec sweep`). `sources`: the
/// testbench and DUT source names, checked case-insensitively for dc, noise
/// and tf; empty skips the check.
[[nodiscard]] diag::Outcome<std::string> analysis_card(const Analysis& analysis,
                                                       const std::vector<std::string>& sources = {});

}  // namespace hefesto::sim
