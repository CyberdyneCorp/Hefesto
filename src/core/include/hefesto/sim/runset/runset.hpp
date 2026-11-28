// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Run sets: sweeps, corners and Monte Carlo (openspec/changes/
// m9-lvs-simulation design.md D14, D18, "Area contracts" SIMADV;
// capabilities simulation-sweeps, simulation-corners,
// simulation-monte-carlo). Points are generated corner-outer, sweep
// lexicographic in declaration order, sample last; each point is one job on
// the worker pool (sim/engine/pool.hpp) and results are stored by point
// index.
//
// M9 R3 (tasks 15.1-15.3), additive to the scaffold contract:
//   - Dimension: the ordered dimensions of "Run sets" (sweep, DOE block,
//     corner, Monte Carlo innermost); RunSetSpec::dimensions takes
//     precedence over the scaffold's corners/sweeps/samples fields, which
//     still build the D14 order (corner, sweeps, samples);
//   - RunSet::generate: the mixed-radix points, refused above the point
//     limit before anything is allocated;
//   - RunPointContext and execute(): every point's deck (point_deck.hpp)
//     submitted to the JobManager in index order with id
//     `<run set id>-p<6 digits>` and its seed (1, or the sample's s_i);
//   - RunSetRunner: windowed submission (at most the pool's workers
//     outstanding), state, progress, pause, resume, cancel, resumption of
//     the points without a completed result, and Monte Carlo early stop;
//   - storage (runset.json, base.spice, points.csv, p<index>/), retention
//     and the result views (family overlay legend, measurement versus
//     variable), all in point-index order.
//
// Owner: SIMADV.

#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/sim/deck/deck.hpp"
#include "hefesto/sim/engine/engine.hpp"
#include "hefesto/sim/engine/job.hpp"
#include "hefesto/sim/runset/corners.hpp"
#include "hefesto/sim/runset/montecarlo.hpp"
#include "hefesto/sim/runset/sweep.hpp"

namespace hefesto::sim {

class JobManager;
struct JobRecord;

/// Caps of design D18 (Q-SIMADV, Q-SIM-25).
inline constexpr std::size_t kMaxRunSetPoints = 100'000;
inline constexpr std::size_t kMaxMonteCarloSamples = 100'000;

/// Added in M9 R3: the caps as settings; each can be lowered, and zero or a
/// value above twice its default is refused (SIM-RUNSET-015).
struct RunSetLimits {
  std::size_t max_points = kMaxRunSetPoints;
  std::size_t max_samples = kMaxMonteCarloSamples;
};
[[nodiscard]] diag::Status check_run_set_limits(const RunSetLimits& limits);

struct SweepVariable {
  std::string name;
  std::vector<double> values;
};

/// Added in M9 R3: one dimension of "Run sets".
struct Dimension {
  enum class Kind : std::uint8_t { sweep, doe, corner, monte_carlo };
  Kind kind = Kind::sweep;
  std::string name;                           // `param:wn`, `temp`, `doe`, `corner`, `mc`
  std::vector<SweepVariableRef> variables;    // sweep: one; doe: one per factor
  std::vector<std::vector<double>> values;    // sweep, doe: per level, one value per variable
  std::vector<CornerLevel> corners;           // corner
  bool corners_set_conditions = false;        // the corner levels set temperature and supply
  std::vector<McLevel> samples;               // monte_carlo
  std::uint32_t base_seed = 1;                // monte_carlo

  [[nodiscard]] std::size_t size() const noexcept;
  /// The level's value in points.csv and job.json: `%.12g`, the corner
  /// name, `nom` or the sample index; a DOE row is its factors joined by `;`.
  [[nodiscard]] std::string label(std::size_t level) const;
};

/// Added in M9 R3: the dimensions from a sweep definition, a DOE block, a
/// corner set's levels and a Monte Carlo spec (each checked).
[[nodiscard]] diag::Outcome<Dimension> sweep_dimension(const SweepDefinition& sweep, const RunSetLimits& limits = {});
[[nodiscard]] diag::Outcome<Dimension> doe_dimension(const DoeBlock& block, const RunSetLimits& limits = {});
[[nodiscard]] Dimension corner_dimension(std::vector<CornerLevel> levels);
[[nodiscard]] diag::Outcome<Dimension> mc_dimension(const MonteCarloSpec& spec, const RunSetLimits& limits = {});

struct RunSetSpec {
  std::string id;
  std::vector<std::string> corners;  // empty = the testbench's corner
  std::vector<SweepVariable> sweeps;
  std::uint32_t samples = 0;         // Monte Carlo samples; 0 = none
  std::uint32_t base_seed = 1;       // B of "Sample seeds"
  // Added in M9 R3:
  std::vector<Dimension> dimensions;  // outermost first; non-empty: replaces the four fields above
  RunSetLimits limits;
};

/// One point of a run set (the contract's `Point`).
struct RunPoint {
  std::size_t index = 0;
  std::string corner;
  std::vector<double> values;  // one per sweep variable (DOE factors included), in dimension order
  std::uint32_t sample = 0;    // the Monte Carlo sample index i (0 also for the nominal sample)
  // Added in M9 R3:
  std::vector<std::size_t> levels;  // the level index per dimension
  bool monte_carlo = false;         // the run set has a Monte Carlo dimension
  bool nominal = false;             // the Monte Carlo nominal sample
  std::uint32_t seed = 1;           // the ngspice seed (1, or s_i)
};

/// Added in M9 R3: how the points' decks are made (simulation-sweeps "Point
/// decks").
struct RunPointContext {
  std::string testbench;
  PreparedDeck base;                       // prepared once per run set
  std::optional<CornerTable> corners;      // for corner and Monte Carlo dimensions
  std::string corner;                      // the testbench's corner (no corner dimension)
  std::vector<std::string> supply_sources;  // the stimuli marked `supply`
  std::optional<MonteCarloSpec> monte_carlo;
  std::vector<StatParameter> stat_parameters;
  std::string dut_subckt;
};

/// Added in M9 R3: a point's job and its last known state.
struct PointRecord {
  JobId job;
  std::optional<JobState> state;  // nullopt: not submitted
  bool completed = false;         // has a completed result
};

struct RunSet {
  RunSetSpec spec;
  std::vector<RunPoint> points;
  std::vector<JobId> jobs;  // by point index
  // Added in M9 R3:
  RunPointContext context;
  std::vector<Dimension> dimensions;  // as generated
  std::vector<PointRecord> records;   // by point index

  /// The points of `spec` in the fixed order; an error above the caps.
  [[nodiscard]] static diag::Outcome<std::vector<RunPoint>> generate(const RunSetSpec& spec);

  /// Added in M9 R3: generates the points and fills `dimensions`, `points`,
  /// `jobs` (ids) and `records`.
  [[nodiscard]] diag::Status prepare();

  /// Added in M9 R3: the deck of point `index` and its warnings.
  [[nodiscard]] diag::Outcome<PreparedDeck> point_deck(std::size_t index, std::vector<diag::Diagnostic>& warnings) const;
};

/// Added in M9 R3: `<run set id>-p<index>` with 6 digits.
[[nodiscard]] JobId point_job_id(std::string_view run_set_id, std::size_t index);

/// Added in M9 R3: `<YYYYMMDD>-<HHMMSS>-<testbench>-rs` in local time, with
/// `-2`, `-3`, ... when `taken` returns true.
[[nodiscard]] std::string make_run_set_id(std::chrono::system_clock::time_point when, std::string_view testbench,
                                          const std::function<bool(std::string_view)>& taken);

/// Submits every point of `run_set` without a completed result to `jobs`,
/// in increasing index (M9 R3; `prepare()` first).
[[nodiscard]] diag::Status execute(RunSet& run_set, JobManager& jobs);

/// The ngspice seed of sample `k` with base seed `base` ("Sample seeds").
/// M9 R3: s_k for the sample index k; the nominal sample uses kNominalSeed.
/// A base seed outside 1..2147483647 is refused (SIM-RUNSET-010).
[[nodiscard]] diag::Outcome<std::uint32_t> sample_seed(std::uint32_t base, std::uint32_t k);

/// Added in M9 R3: "Run set execution".
enum class RunSetState : std::uint8_t { queued, running, paused, completed, failed, cancelled };
[[nodiscard]] std::string_view run_set_state_name(RunSetState state) noexcept;

class RunSetRunner {
 public:
  /// `passes` decides a completed sample's verdict for early stopping
  /// (default: completed passes).
  RunSetRunner(RunSet& run_set, JobManager& jobs, std::function<bool(const JobRecord&)> passes = {});

  /// Submits the first points (at most the pool's workers at once).
  [[nodiscard]] diag::Status start();
  /// After JobManager::poll(): records final points, submits the next ones,
  /// applies early stopping. Returns the number of points submitted.
  std::size_t update();
  [[nodiscard]] diag::Status pause();
  [[nodiscard]] diag::Status resume();
  [[nodiscard]] diag::Status cancel();

  [[nodiscard]] RunSetState state() const noexcept { return state_; }
  /// (points in a final state + sum of running points' progress) / total.
  [[nodiscard]] double progress() const;
  [[nodiscard]] std::size_t done() const;
  [[nodiscard]] std::optional<std::size_t> stopped_early() const noexcept { return stopped_early_; }
  /// `<state> <done>/<total> <progress>` (`sim runset status`).
  [[nodiscard]] std::string status_line() const;

 private:
  void refresh();
  std::size_t submit_more();
  void check_early_stop();
  [[nodiscard]] std::size_t outstanding() const;

  RunSet& run_set_;
  JobManager& jobs_;
  std::function<bool(const JobRecord&)> passes_;
  RunSetState state_ = RunSetState::queued;
  std::size_t next_ = 0;
  std::optional<std::size_t> stopped_early_;
};

// --- storage ("Run set storage", "Measured waveform retention") ---

enum class Retention : std::uint8_t { all, failing, none, measured };
[[nodiscard]] std::string_view retention_name(Retention r) noexcept;
/// `all` up to 100 points, `failing` above (the default stays until the
/// user approves another after PERF, D-24).
[[nodiscard]] Retention default_retention(std::size_t points) noexcept;
/// Whether point `p` keeps `result.raw`: point 0 and the Monte Carlo nominal
/// point always do; `failing` keeps failed points and points failing a spec.
[[nodiscard]] bool keeps_raw(Retention keep, const RunPoint& p, bool failed, bool failed_spec) noexcept;
/// `measured`: the scale vector (the first) and the vectors the run set's
/// measurements reference, at full resolution; every other mode keeps all.
[[nodiscard]] std::vector<Vector> retained_vectors(Retention keep, const std::vector<Vector>& vectors,
                                                   const std::vector<std::string>& referenced);

/// One row of points.csv: the point's job state and its measurements.
struct PointResult {
  std::size_t index = 0;
  JobId job;
  std::string state;  // the job state name, or `pending`
  std::vector<std::optional<double>> measurements;
};

/// The results of every point in point-index order ("Results in point
/// order"), measured by `measure` (nullopt values when not completed).
[[nodiscard]] std::vector<PointResult> point_results(
    const RunSet& run_set, const JobManager& jobs,
    const std::function<std::vector<std::optional<double>>(const JobRecord&)>& measure = {});

[[nodiscard]] std::string points_csv(const RunSet& run_set, const std::vector<std::string>& measurements,
                                     const std::vector<PointResult>& rows);

struct RunSetInfo {
  std::string state;
  Retention keep = Retention::all;
  std::string pdk;
  std::string descriptor_version;
  std::optional<std::size_t> stopped_early;
  std::string rerun_of;
};

[[nodiscard]] std::string runset_json(const RunSet& run_set, const RunSetInfo& info);

/// Writes `<folder>/runset.json`, `base.spice` and `points.csv`.
[[nodiscard]] diag::Status store_run_set(platform::FileSystem& files, const platform::Path& folder,
                                         const RunSet& run_set, const RunSetInfo& info,
                                         const std::vector<std::string>& measurements,
                                         const std::vector<PointResult>& rows);

// --- result views ("Sweep result views") ---

/// `<display name> [<dim>=<value>, ...]`.
[[nodiscard]] std::string family_legend(std::string_view display, const RunSet& run_set, std::size_t point);
/// The legend of an overlay of `count` traces: one entry `<display> (<n>
/// points)` above 200 traces, else one per point (empty: use family_legend).
[[nodiscard]] std::optional<std::string> overlay_legend(std::string_view display, std::size_t count);

struct Curve {
  std::string label;  // `supply=0.9`, ...
  std::vector<double> x;
  std::vector<std::optional<double>> y;
};

/// "Measurement versus variable": one curve per combination of the other
/// sweep dimensions' levels, x the levels of dimension `dimension`.
[[nodiscard]] std::vector<Curve> measurement_curves(const RunSet& run_set, std::size_t dimension,
                                                    const std::vector<std::optional<double>>& values);

}  // namespace hefesto::sim
