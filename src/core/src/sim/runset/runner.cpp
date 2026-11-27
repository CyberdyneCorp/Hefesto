// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// RunSetRunner (runset.hpp; simulation-sweeps "Run set execution",
// simulation-monte-carlo "Parallel and sequential execution", "Failed
// samples and early stopping"). Points are submitted in increasing index,
// at most the pool's worker count outstanding, so pausing stops new points
// from starting and every result is stored by point index whatever the
// order in which jobs finish.
//
// Owner: SIMADV.

#include <cstdio>

#include "hefesto/sim/engine/job_manager.hpp"
#include "hefesto/sim/runset/runset.hpp"

namespace hefesto::sim {

namespace {

bool is_final(std::optional<JobState> s) noexcept {
  return s && (*s == JobState::completed || *s == JobState::failed || *s == JobState::cancelled);
}

bool outstanding_state(std::optional<JobState> s) noexcept {
  return s && !is_final(s);
}

// The early-stop target when the Monte Carlo dimension is the only one
// ("Early stopping SHALL be disabled when the Monte Carlo dimension is
// inside other dimensions").
std::optional<double> stop_target(const RunSet& rs) {
  const bool alone = rs.dimensions.size() == 1 && rs.dimensions[0].kind == Dimension::Kind::monte_carlo;
  if (!alone || !rs.context.monte_carlo) {
    return std::nullopt;
  }
  return rs.context.monte_carlo->stop;
}

}  // namespace

std::string_view run_set_state_name(RunSetState state) noexcept {
  switch (state) {
    case RunSetState::queued: return "queued";
    case RunSetState::running: return "running";
    case RunSetState::paused: return "paused";
    case RunSetState::completed: return "completed";
    case RunSetState::failed: return "failed";
    case RunSetState::cancelled: return "cancelled";
  }
  return "queued";
}

RunSetRunner::RunSetRunner(RunSet& run_set, JobManager& jobs, std::function<bool(const JobRecord&)> passes)
    : run_set_(run_set), jobs_(jobs), passes_(std::move(passes)) {
  if (!passes_) {
    passes_ = [](const JobRecord& r) { return r.job.state == JobState::completed; };
  }
}

diag::Status RunSetRunner::start() {
  diag::Status status = run_set_.records.size() == run_set_.points.size() && !run_set_.points.empty()
                            ? diag::Status{diag::Done{}}
                            : run_set_.prepare();
  if (status.ok()) {
    // A deck error common to every point (a parameter defined twice, a
    // Monte Carlo mode the PDK lacks) refuses the run set before any job.
    std::vector<diag::Diagnostic> warnings;
    auto first = run_set_.point_deck(0, warnings);
    status = first.ok() ? diag::Status{diag::Done{}} : diag::Status{first.error()};
  }
  if (!status.ok()) {
    state_ = RunSetState::failed;
    return status;
  }
  state_ = RunSetState::running;
  next_ = 0;
  submit_more();
  return diag::Done{};
}

std::size_t RunSetRunner::outstanding() const {
  std::size_t n = 0;
  for (const PointRecord& r : run_set_.records) {
    n += outstanding_state(r.state) ? 1U : 0U;
  }
  return n;
}

void RunSetRunner::refresh() {
  for (PointRecord& r : run_set_.records) {
    if (outstanding_state(r.state)) {
      r.state = jobs_.state(r.job);
      r.completed = r.state == JobState::completed;
    }
  }
}

std::size_t RunSetRunner::submit_more() {
  std::size_t submitted = 0;
  const std::size_t window = std::max<std::size_t>(jobs_.workers(), 1);
  while (state_ == RunSetState::running && next_ < run_set_.points.size() && outstanding() < window) {
    const std::size_t i = next_++;
    PointRecord& rec = run_set_.records[i];
    if (rec.completed) {
      continue;  // resumed: a completed point never runs again
    }
    std::vector<diag::Diagnostic> warnings;
    auto deck = run_set_.point_deck(i, warnings);
    const RunPoint& p = run_set_.points[i];
    const std::string corner = p.corner.empty() ? run_set_.context.corner : p.corner;
    auto id = deck.ok() ? jobs_.submit(std::move(deck.value()), JobOptions{run_set_.context.testbench, corner, rec.job, p.seed})
                        : diag::Outcome<JobId>{deck.error()};
    rec.state = id.ok() ? std::optional{JobState::queued} : std::optional{JobState::failed};
    submitted += id.ok() ? 1U : 0U;
  }
  return submitted;
}

void RunSetRunner::check_early_stop() {
  const std::optional<double> h = stop_target(run_set_);
  if (!h || stopped_early_) {
    return;
  }
  std::vector<SampleOutcome> samples;
  for (std::size_t i = 0; i < run_set_.points.size(); ++i) {
    if (run_set_.points[i].nominal) {
      continue;
    }
    const PointRecord& r = run_set_.records[i];
    const JobRecord* job = is_final(r.state) ? jobs_.record(r.job) : nullptr;
    samples.push_back(SampleOutcome{job != nullptr, job != nullptr && passes_(*job),
                                    job != nullptr && job->job.state == JobState::failed});
  }
  const auto m = early_stop(samples, *h, run_set_.context.monte_carlo->exclude_failed);
  if (!m) {
    return;
  }
  stopped_early_ = m;
  // Samples m.. are cancelled: the running ones in their engine, the rest
  // are never submitted.
  next_ = run_set_.points.size();
  for (std::size_t i = 0; i < run_set_.points.size(); ++i) {
    const RunPoint& p = run_set_.points[i];
    if (!p.nominal && p.sample >= *m && outstanding_state(run_set_.records[i].state)) {
      (void)jobs_.cancel(run_set_.records[i].job);
    }
  }
}

std::size_t RunSetRunner::update() {
  if (state_ != RunSetState::running && state_ != RunSetState::paused) {
    refresh();
    return 0;
  }
  refresh();
  check_early_stop();
  const std::size_t submitted = submit_more();
  if (state_ == RunSetState::running && next_ >= run_set_.points.size() && outstanding() == 0) {
    state_ = RunSetState::completed;
  }
  return submitted;
}

diag::Status RunSetRunner::pause() {
  if (state_ != RunSetState::running) {
    return diag::make("SIM-RUNSET-018", "run set " + run_set_.spec.id + " is not running");
  }
  state_ = RunSetState::paused;
  for (const PointRecord& r : run_set_.records) {
    if (r.state == JobState::running) {
      (void)jobs_.pause(r.job);  // a refusal (the run just ended) leaves the point alone
    }
  }
  return diag::Done{};
}

diag::Status RunSetRunner::resume() {
  if (state_ == RunSetState::paused) {
    state_ = RunSetState::running;
    for (const PointRecord& r : run_set_.records) {
      if (jobs_.state(r.job) == JobState::paused) {
        (void)jobs_.resume(r.job);
      }
    }
    submit_more();
    return diag::Done{};
  }
  if (state_ != RunSetState::cancelled) {
    return diag::make("SIM-RUNSET-018", "run set " + run_set_.spec.id + " is not paused or cancelled");
  }
  // "Resumable": exactly the points without a completed result run again,
  // with their stored decks and seeds.
  refresh();
  for (PointRecord& r : run_set_.records) {
    if (!r.completed) {
      r.state.reset();
    }
  }
  state_ = RunSetState::running;
  next_ = 0;
  submit_more();
  return diag::Done{};
}

diag::Status RunSetRunner::cancel() {
  if (state_ != RunSetState::running && state_ != RunSetState::paused) {
    return diag::make("SIM-RUNSET-018", "run set " + run_set_.spec.id + " is not active");
  }
  for (const PointRecord& r : run_set_.records) {
    if (outstanding_state(r.state)) {
      (void)jobs_.cancel(r.job);
    }
  }
  refresh();
  state_ = RunSetState::cancelled;
  return diag::Done{};
}

std::size_t RunSetRunner::done() const {
  std::size_t n = 0;
  for (const PointRecord& r : run_set_.records) {
    n += is_final(r.state) ? 1U : 0U;
  }
  return n;
}

double RunSetRunner::progress() const {
  if (run_set_.points.empty()) {
    return 0.0;
  }
  double sum = static_cast<double>(done());
  for (const PointRecord& r : run_set_.records) {
    const JobRecord* job = outstanding_state(r.state) ? jobs_.record(r.job) : nullptr;
    sum += job != nullptr ? job->progress : 0.0;
  }
  return sum / static_cast<double>(run_set_.points.size());
}

std::string RunSetRunner::status_line() const {
  char buffer[32];
  std::snprintf(buffer, sizeof buffer, "%.12g", progress());
  std::string p{buffer};
  if (p.find('.') == std::string::npos && p.find('e') == std::string::npos) {
    p += ".0";  // `completed 9/9 1.0`
  }
  return std::string{run_set_state_name(state_)} + " " + std::to_string(done()) + "/" +
         std::to_string(run_set_.points.size()) + " " + p;
}

}  // namespace hefesto::sim
