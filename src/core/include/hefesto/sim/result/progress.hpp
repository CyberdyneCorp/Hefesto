// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Job progress (openspec/changes/m9-lvs-simulation design.md D12;
// capability simulation-analyses-and-results "Progress reporting"; Q-SIM-14:
// the status strings combined with the streamed time point). Added in M9 R2
// (task 13.3).
//
// Owner: SIMRES.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::sim {

/// One analysis card of a deck, in deck order: its name without the dot
/// (`op`, `tran`, `ac`, ...) and, for `tran`, its time range.
struct AnalysisSpan {
  std::string name;
  double start = 0.0;  // tran: tstart
  double stop = 0.0;   // tran: tstop; 0 otherwise
  friend bool operator==(const AnalysisSpan&, const AnalysisSpan&) = default;
};

/// The analysis cards of `deck` (outside `.control` sections), in order.
[[nodiscard]] std::vector<AnalysisSpan> analyses_of(const std::vector<std::string>& deck);

/// One status string of the table of "Progress reporting".
struct ProgressStatus {
  enum class Kind : std::uint8_t { percent, start, shooting, ready, other };
  Kind kind = Kind::other;
  std::string analysis;          // percent, start
  double fraction = 0.0;         // percent: p / 100
  std::uint64_t iteration = 0;   // shooting
};

/// Parses `status`; `point` is the percentage the engine split off already
/// (hefesto-simd sends `tran` and 34.5 for `tran: 34.5%`), or -1.
[[nodiscard]] ProgressStatus parse_progress(std::string_view status, double point = -1.0);

/// The progress of one job: never goes down within one analysis; overall
/// `(completed analyses + current progress) / total analyses`.
class ProgressTracker {
 public:
  ProgressTracker() = default;
  explicit ProgressTracker(std::vector<AnalysisSpan> analyses);

  void status(std::string_view status, double point = -1.0);
  /// A streamed `time` value: for `tran`, max(current, (t - tstart) / (tstop - tstart)).
  void scale(double t);

  [[nodiscard]] double progress() const noexcept;
  /// The analysis in progress (an index into the analyses).
  [[nodiscard]] std::size_t current() const noexcept { return current_; }
  /// `shooting: <n>`: progress is indeterminate, iteration n.
  [[nodiscard]] std::optional<std::uint64_t> iteration() const noexcept { return iteration_; }

 private:
  void enter(std::string_view analysis);
  void advance(double fraction);

  std::vector<AnalysisSpan> analyses_;
  std::size_t current_ = 0;
  double fraction_ = 0.0;
  std::optional<std::uint64_t> iteration_;
};

}  // namespace hefesto::sim
