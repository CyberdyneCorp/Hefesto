// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Run-set dimensions, point generation, point decks and submission
// (runset.hpp; M9 R3, tasks 15.1-15.3).
//
// Owner: SIMADV.

#include "hefesto/sim/runset/runset.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>

#include "hefesto/sim/engine/job_manager.hpp"
#include "hefesto/sim/runset/point_deck.hpp"
#include "hefesto/sim/runset/random.hpp"

namespace hefesto::sim {

namespace {

diag::Diagnostic too_many(double n, std::size_t limit) {
  char buffer[32];
  std::snprintf(buffer, sizeof buffer, "%.0f", n);
  return diag::make("SIM-RUNSET-001", "Run set has " + std::string{buffer} + " points; the limit is " +
                                          std::to_string(limit));
}

// The scaffold fields in the order of design D14: corner, sweeps, samples.
diag::Outcome<std::vector<Dimension>> legacy_dimensions(const RunSetSpec& spec) {
  std::vector<Dimension> out;
  if (!spec.corners.empty()) {
    std::vector<CornerLevel> levels;
    for (const std::string& c : spec.corners) {
      levels.push_back(CornerLevel{c, c, 27.0, 1.0});
    }
    out.push_back(corner_dimension(std::move(levels)));
    out.back().corners_set_conditions = false;  // names only: no temperature or supply
  }
  for (const SweepVariable& s : spec.sweeps) {
    SweepDefinition d{s.name, SweepDefinition::Mode::list, 0.0, 0.0, 0.0, 0, s.values};
    auto dim = sweep_dimension(d, spec.limits);
    if (!dim.ok()) {
      return dim.error();
    }
    out.push_back(std::move(dim.value()));
  }
  if (spec.samples > 0) {
    auto dim = mc_dimension(MonteCarloSpec{McMode::process, spec.samples, spec.base_seed, false, {}, false},
                            spec.limits);
    if (!dim.ok()) {
      return dim.error();
    }
    out.push_back(std::move(dim.value()));
  }
  return out;
}

diag::Status check_dimension_kinds(const std::vector<Dimension>& dims) {
  std::size_t corners = 0;
  std::size_t mcs = 0;
  for (std::size_t i = 0; i < dims.size(); ++i) {
    corners += dims[i].kind == Dimension::Kind::corner ? 1U : 0U;
    if (dims[i].kind == Dimension::Kind::monte_carlo) {
      ++mcs;
      if (i + 1 != dims.size()) {
        return diag::make("SIM-RUNSET-004", "the Monte Carlo dimension must be the innermost");
      }
    }
  }
  if (corners > 1 || mcs > 1) {
    return diag::make("SIM-RUNSET-004", "a run set has at most one corner and one Monte Carlo dimension");
  }
  return diag::Done{};
}

diag::Status check_variables(const std::vector<Dimension>& dims) {
  std::vector<SweepVariableRef> seen;
  const bool conditions = std::any_of(dims.begin(), dims.end(), [](const Dimension& d) {
    return d.kind == Dimension::Kind::corner && d.corners_set_conditions;
  });
  for (const Dimension& d : dims) {
    for (const SweepVariableRef& v : d.variables) {
      const bool dup = std::any_of(seen.begin(), seen.end(), [&v](const SweepVariableRef& s) { return s.same_as(v); });
      if (dup) {
        return diag::make("SIM-RUNSET-004", "Sweep " + v.text() + ": two dimensions name the same variable");
      }
      const bool corner_owned = v.kind == SweepVariableRef::Kind::supply || v.kind == SweepVariableRef::Kind::temperature;
      if (conditions && corner_owned) {
        return diag::make("SIM-RUNSET-004", "Sweep " + v.text() + " conflicts with the corner " + v.text());
      }
      seen.push_back(v);
    }
  }
  return diag::Done{};
}

// The point count, refused above the limit (or at 0) before allocation.
diag::Outcome<std::size_t> point_count(const std::vector<Dimension>& dims, std::size_t limit) {
  double n = 1.0;
  for (const Dimension& d : dims) {
    n = std::min(n * static_cast<double>(d.size()), 1e18);
  }
  if (n > static_cast<double>(limit)) {
    return too_many(n, limit);
  }
  if (n < 1.0) {
    return diag::make("SIM-RUNSET-002", "Run set has no points");
  }
  return static_cast<std::size_t>(n);
}

void apply_level(RunPoint& p, const Dimension& d, std::size_t level) {
  switch (d.kind) {
    case Dimension::Kind::sweep:
    case Dimension::Kind::doe: p.values.insert(p.values.end(), d.values[level].begin(), d.values[level].end()); break;
    case Dimension::Kind::corner: p.corner = d.corners[level].name; break;
    case Dimension::Kind::monte_carlo:
      p.monte_carlo = true;
      p.nominal = d.samples[level].nominal;
      p.sample = d.samples[level].index;
      p.seed = d.samples[level].seed(d.base_seed);
      break;
  }
}

std::vector<RunPoint> points_of(const std::vector<Dimension>& dims, std::size_t count) {
  std::vector<RunPoint> out(count);
  for (std::size_t p = 0; p < count; ++p) {
    RunPoint& point = out[p];
    point.index = p;
    point.levels.assign(dims.size(), 0);
    std::size_t rest = p;
    for (std::size_t d = dims.size(); d-- > 0;) {  // innermost digit fastest
      point.levels[d] = rest % dims[d].size();
      rest /= dims[d].size();
    }
    for (std::size_t d = 0; d < dims.size(); ++d) {
      apply_level(point, dims[d], point.levels[d]);
    }
  }
  return out;
}

diag::Outcome<std::vector<Dimension>> dimensions_of(const RunSetSpec& spec) {
  if (auto status = check_run_set_limits(spec.limits); !status.ok()) {
    return status.error();
  }
  auto dims = spec.dimensions.empty() ? legacy_dimensions(spec) : diag::Outcome<std::vector<Dimension>>{spec.dimensions};
  if (!dims.ok()) {
    return dims.error();
  }
  for (auto check : {check_dimension_kinds, check_variables}) {
    if (auto status = check(dims.value()); !status.ok()) {
      return status.error();
    }
  }
  return dims;
}

// The sweep and DOE levels of point `p` as deck changes.
void add_variable_levels(PointChanges& c, const RunSet& rs, const RunPoint& p) {
  for (std::size_t d = 0; d < rs.dimensions.size(); ++d) {
    const Dimension& dim = rs.dimensions[d];
    if (dim.kind != Dimension::Kind::sweep && dim.kind != Dimension::Kind::doe) {
      continue;
    }
    const std::vector<double>& values = dim.values[p.levels[d]];
    for (std::size_t v = 0; v < dim.variables.size(); ++v) {
      const SweepVariableRef& var = dim.variables[v];
      switch (var.kind) {
        case SweepVariableRef::Kind::parameter: c.params.emplace_back(var.name, values[v]); break;
        case SweepVariableRef::Kind::temperature: c.temperature = values[v]; break;
        case SweepVariableRef::Kind::supply: c.supply = values[v]; break;
        case SweepVariableRef::Kind::source: c.sources.emplace_back(var.name, values[v]); break;
      }
    }
  }
}

const CornerLevel* corner_level_of(const RunSet& rs, const RunPoint& p) {
  for (std::size_t d = 0; d < rs.dimensions.size(); ++d) {
    if (rs.dimensions[d].kind == Dimension::Kind::corner) {
      return &rs.dimensions[d].corners[p.levels[d]];
    }
  }
  return nullptr;
}

diag::Status add_corner(PointChanges& c, const RunSet& rs, const RunPoint& p) {
  const CornerLevel* level = corner_level_of(rs, p);
  if (level == nullptr || !rs.context.corners) {
    return diag::Done{};
  }
  auto applied = corner_application(*rs.context.corners, *level);
  if (!applied.ok()) {
    return applied.error();
  }
  c.corner = std::move(applied.value());
  return diag::Done{};
}

diag::Status add_monte_carlo(PointChanges& c, const RunSet& rs, const RunPoint& p) {
  if (!p.monte_carlo) {
    return diag::Done{};
  }
  const RunPointContext& ctx = rs.context;
  const McMode mode = ctx.monte_carlo ? ctx.monte_carlo->mode : McMode::process;
  c.mode = mode;
  c.nominal = p.nominal;
  c.stat_parameters = ctx.stat_parameters;
  c.dut_subckt = ctx.dut_subckt;
  if (!ctx.corners) {
    c.monte_carlo = McApplication{};
    return diag::Done{};
  }
  const CornerLevel* level = corner_level_of(rs, p);
  auto applied = mc_application(*ctx.corners, mode, level != nullptr ? level->corner : ctx.corner);
  if (!applied.ok()) {
    return applied.error();
  }
  c.monte_carlo = std::move(applied.value());
  return diag::Done{};
}

}  // namespace

diag::Status check_run_set_limits(const RunSetLimits& limits) {
  const auto bad = [](std::size_t value, std::size_t fallback) { return value == 0 || value > 2 * fallback; };
  if (bad(limits.max_points, kMaxRunSetPoints) || bad(limits.max_samples, kMaxMonteCarloSamples)) {
    return diag::make("SIM-RUNSET-015", "run-set limit refused: each limit must be between 1 and twice its default " +
                                            std::to_string(kMaxRunSetPoints));
  }
  return diag::Done{};
}

std::size_t Dimension::size() const noexcept {
  switch (kind) {
    case Kind::sweep:
    case Kind::doe: return values.size();
    case Kind::corner: return corners.size();
    case Kind::monte_carlo: return samples.size();
  }
  return 0;
}

std::string Dimension::label(std::size_t level) const {
  switch (kind) {
    case Kind::corner: return corners[level].name;
    case Kind::monte_carlo: return samples[level].label();
    case Kind::sweep:
    case Kind::doe: break;
  }
  std::string out;
  for (const double v : values[level]) {
    out += (out.empty() ? "" : ";") + format_level(v);
  }
  return out;
}

diag::Outcome<Dimension> sweep_dimension(const SweepDefinition& sweep, const RunSetLimits& limits) {
  auto variable = parse_sweep_variable(sweep.variable);
  if (!variable.ok()) {
    return variable.error();
  }
  auto levels = sweep_levels(sweep, limits.max_points);
  if (!levels.ok()) {
    return levels.error();
  }
  Dimension d;
  d.kind = Dimension::Kind::sweep;
  d.name = variable.value().text();
  d.variables = {variable.value()};
  for (const double v : levels.value()) {
    d.values.push_back({v});
  }
  return d;
}

diag::Outcome<Dimension> doe_dimension(const DoeBlock& block, const RunSetLimits& limits) {
  Dimension d;
  d.kind = Dimension::Kind::doe;
  d.name = "doe";
  for (const DoeFactor& f : block.factors) {
    auto variable = parse_sweep_variable(f.variable);
    if (!variable.ok()) {
      return variable.error();
    }
    d.variables.push_back(variable.value());
  }
  auto rows = doe_rows(block, limits.max_points);
  if (!rows.ok()) {
    return rows.error();
  }
  d.values = std::move(rows.value());
  return d;
}

Dimension corner_dimension(std::vector<CornerLevel> levels) {
  Dimension d;
  d.kind = Dimension::Kind::corner;
  d.name = "corner";
  d.corners = std::move(levels);
  d.corners_set_conditions = true;
  return d;
}

diag::Outcome<Dimension> mc_dimension(const MonteCarloSpec& spec, const RunSetLimits& limits) {
  const auto max = static_cast<std::uint32_t>(std::min<std::size_t>(limits.max_samples, UINT32_MAX));
  if (auto status = check_monte_carlo(spec, max); !status.ok()) {
    return status.error();
  }
  Dimension d;
  d.kind = Dimension::Kind::monte_carlo;
  d.name = "mc";
  d.samples = mc_levels(spec);
  d.base_seed = spec.base_seed;
  return d;
}

diag::Outcome<std::vector<RunPoint>> RunSet::generate(const RunSetSpec& spec) {
  auto dims = dimensions_of(spec);
  if (!dims.ok()) {
    return dims.error();
  }
  auto count = point_count(dims.value(), spec.limits.max_points);
  if (!count.ok()) {
    return count.error();
  }
  return points_of(dims.value(), count.value());
}

diag::Status RunSet::prepare() {
  auto dims = dimensions_of(spec);
  if (!dims.ok()) {
    return dims.error();
  }
  auto count = point_count(dims.value(), spec.limits.max_points);
  if (!count.ok()) {
    return count.error();
  }
  dimensions = std::move(dims.value());
  points = points_of(dimensions, count.value());
  jobs.clear();
  records.clear();
  for (const RunPoint& p : points) {
    jobs.push_back(point_job_id(spec.id, p.index));
    records.push_back(PointRecord{jobs.back(), std::nullopt, false});
  }
  return diag::Done{};
}

diag::Outcome<PreparedDeck> RunSet::point_deck(std::size_t index, std::vector<diag::Diagnostic>& warnings) const {
  if (index >= points.size()) {
    return diag::make("SIM-RUNSET-018", "run set " + spec.id + " has no point " + std::to_string(index));
  }
  const RunPoint& p = points[index];
  PointChanges changes;
  changes.job_id = point_job_id(spec.id, index);
  changes.supply_sources = context.supply_sources;
  if (context.corners) {
    changes.variables = context.corners->variables;
  }
  add_variable_levels(changes, *this, p);
  for (auto step : {add_corner, add_monte_carlo}) {
    if (auto status = step(changes, *this, p); !status.ok()) {
      return status.error();
    }
  }
  auto lines = sim::point_deck(context.base.lines, changes, warnings);
  if (!lines.ok()) {
    return lines.error();
  }
  PreparedDeck deck = context.base;
  deck.lines = std::move(lines.value());
  deck.sha256.clear();  // the base deck's hash is runset.json's deckSha256
  return deck;
}

JobId point_job_id(std::string_view run_set_id, std::size_t index) {
  char buffer[16];
  std::snprintf(buffer, sizeof buffer, "%06zu", index);
  return std::string{run_set_id} + "-p" + buffer;
}

std::string make_run_set_id(std::chrono::system_clock::time_point when, std::string_view testbench,
                            const std::function<bool(std::string_view)>& taken) {
  const std::time_t t = std::chrono::system_clock::to_time_t(when);
  std::tm local{};
  localtime_r(&t, &local);
  char buffer[32];
  const std::size_t n = std::strftime(buffer, sizeof buffer, "%Y%m%d-%H%M%S", &local);
  const std::string base = std::string(buffer, n) + "-" + std::string{testbench} + "-rs";
  std::string id = base;
  for (std::size_t k = 2; taken && taken(id); ++k) {
    id = base + "-" + std::to_string(k);
  }
  return id;
}

diag::Status execute(RunSet& run_set, JobManager& jobs) {
  if (run_set.records.size() != run_set.points.size()) {
    if (auto status = run_set.prepare(); !status.ok()) {
      return status;
    }
  }
  for (std::size_t i = 0; i < run_set.points.size(); ++i) {
    PointRecord& rec = run_set.records[i];
    if (rec.completed) {
      continue;
    }
    std::vector<diag::Diagnostic> warnings;
    auto deck = run_set.point_deck(i, warnings);
    if (!deck.ok()) {
      return deck.error();
    }
    const RunPoint& p = run_set.points[i];
    auto id = jobs.submit(std::move(deck.value()),
                          JobOptions{run_set.context.testbench, p.corner.empty() ? run_set.context.corner : p.corner,
                                     rec.job, p.seed});
    if (!id.ok()) {
      return id.error();
    }
    rec.state = JobState::queued;
  }
  return diag::Done{};
}

diag::Outcome<std::uint32_t> sample_seed(std::uint32_t base, std::uint32_t k) {
  if (base < 1 || base > kMaxBaseSeed) {
    return diag::make("SIM-RUNSET-010", "sim mc: seed must be between 1 and " + std::to_string(kMaxBaseSeed));
  }
  return sample_seed_value(base, k);
}

}  // namespace hefesto::sim
