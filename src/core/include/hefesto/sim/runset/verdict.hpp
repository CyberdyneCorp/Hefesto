// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pass and fail per corner and the corner matrix (capability
// simulation-corners "Pass and fail per corner", "Corner matrix"; the limit
// and margin table of simulation-measurements-and-statistics
// "Specifications"). The measurements themselves come from SIMMEAS (task
// 15.4); here they are given per point.
//
// Owner: SIMADV (M9 R3, task 15.2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hefesto::sim {

/// One limit of "Specifications".
struct Limit {
  enum class Kind : std::uint8_t { min, max, range, target };
  Kind kind = Kind::max;
  double a = 0.0;  // lo, hi, lo or t
  double b = 0.0;  // range: hi; target: tol
  std::optional<double> scale;  // the margin's divisor for a one-sided limit at 0
};

/// A specification with its corner filter and per-corner overrides.
struct CornerSpec {
  std::string measurement;
  Limit limit;
  std::vector<std::string> corners;                    // glob filter; empty = every corner
  std::vector<std::pair<std::string, Limit>> overrides;  // first matching glob wins
};

enum class SpecResult : std::uint8_t { pass, fail, no_value };

/// `*` and `?` glob match on the whole name.
[[nodiscard]] bool glob_match(std::string_view pattern, std::string_view name) noexcept;

[[nodiscard]] SpecResult check_limit(const Limit& limit, std::optional<double> value) noexcept;

/// The normalised margin; nullopt when undefined (no value, or a one-sided
/// limit at 0 without a scale).
[[nodiscard]] std::optional<double> margin(const Limit& limit, std::optional<double> value) noexcept;

/// The limit of `spec` at corner `corner` (nullopt: the spec does not apply).
[[nodiscard]] std::optional<Limit> limit_at(const CornerSpec& spec, std::string_view corner);

/// One point of a corner run: its corner level, whether it completed, and
/// its measurements by name.
struct CornerPoint {
  std::size_t corner = 0;  // the corner level index
  bool completed = false;  // false: failed, cancelled or not run
  bool final = true;       // false: not run yet (the matrix shows `pending`)
  std::vector<std::pair<std::string, std::optional<double>>> values;
};

enum class Verdict : std::uint8_t { pass, fail, error, pending };
enum class CellMark : std::uint8_t { pass, fail, marginal, no_value, pending };

[[nodiscard]] std::string_view verdict_name(Verdict v) noexcept;
/// The mark's symbol ("Marks SHALL NOT rely on colour alone").
[[nodiscard]] std::string_view mark_symbol(CellMark m) noexcept;

struct MatrixCell {
  std::optional<double> value;  // the worst value over the corner's points
  CellMark mark = CellMark::no_value;
};

struct MatrixRow {
  std::string corner;
  Verdict verdict = Verdict::pending;
  std::vector<MatrixCell> cells;  // one per column
};

struct MatrixSummary {
  std::optional<double> worst;
  std::string corner;  // where it occurs
};

struct CornerMatrix {
  std::vector<std::string> columns;  // specification columns first, then other measurements
  std::vector<MatrixRow> rows;       // corner level order
  std::vector<MatrixSummary> summary;
  Verdict verdict = Verdict::pending;  // the run set verdict: pass or fail
};

/// The matrix of `points` over `corners` (level names), with the testbench's
/// `marginal_threshold` (default 0.1). A corner with more than one point is
/// `pass` only when every point passes, and shows its worst values.
[[nodiscard]] CornerMatrix corner_matrix(const std::vector<std::string>& corners, const std::vector<CornerPoint>& points,
                                         const std::vector<CornerSpec>& specs,
                                         const std::vector<std::string>& measurements, double marginal_threshold = 0.1);

/// The matrix rows as `sim corners matrix` returns them: corner, verdict,
/// then each cell (`format_si`, or `—` for no value).
[[nodiscard]] std::vector<std::vector<std::string>> matrix_lists(const CornerMatrix& matrix);

/// Up to 4 significant digits and an SI prefix (`131p`, `1.8`, `2.5k`).
[[nodiscard]] std::string format_si(double value);

/// "Re-run failed": the corner levels whose verdict is fail or error.
[[nodiscard]] std::vector<std::size_t> failed_corners(const CornerMatrix& matrix);

}  // namespace hefesto::sim
