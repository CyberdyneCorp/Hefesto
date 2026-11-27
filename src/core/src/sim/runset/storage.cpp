// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Run-set storage, retention, results in point order and result views
// (runset.hpp; simulation-sweeps "Run set storage", "Measured waveform
// retention", "Results in point order", "Sweep result views").
//
// Owner: SIMADV.

#include <algorithm>
#include <cstdio>
#include <map>

#include <nlohmann/json.hpp>

#include "hefesto/sim/engine/job_manager.hpp"
#include "hefesto/sim/runset/runset.hpp"

namespace hefesto::sim {

namespace {

std::string_view job_state_name(JobState s) noexcept {
  switch (s) {
    case JobState::queued: return "queued";
    case JobState::preparing: return "preparing";
    case JobState::loading: return "loading";
    case JobState::running: return "running";
    case JobState::paused: return "paused";
    case JobState::completed: return "completed";
    case JobState::failed: return "failed";
    case JobState::cancelled: return "cancelled";
  }
  return "queued";
}

std::string number(double v) {
  char buffer[40];
  std::snprintf(buffer, sizeof buffer, "%.17g", v);
  return buffer;
}

std::string csv_field(const std::string& text) {
  if (text.find_first_of(",\"\n\r") == std::string::npos) {
    return text;
  }
  std::string out = "\"";
  for (const char c : text) {
    out += c == '"' ? std::string{"\"\""} : std::string{c};
  }
  return out + "\"";
}

nlohmann::json dimension_json(const Dimension& d) {
  static constexpr const char* kKinds[] = {"sweep", "doe", "corner", "monte_carlo"};
  nlohmann::json j{{"name", d.name}, {"kind", kKinds[static_cast<int>(d.kind)]}};
  nlohmann::json levels = nlohmann::json::array();
  for (std::size_t i = 0; i < d.size(); ++i) {
    levels.push_back(d.label(i));
  }
  j["levels"] = std::move(levels);
  if (d.kind == Dimension::Kind::doe || d.kind == Dimension::Kind::sweep) {
    nlohmann::json vars = nlohmann::json::array();
    for (const SweepVariableRef& v : d.variables) {
      vars.push_back(v.text());
    }
    j["variables"] = std::move(vars);
  }
  if (d.kind == Dimension::Kind::monte_carlo) {
    j["baseSeed"] = d.base_seed;
  }
  return j;
}

// A level's numeric x for "Measurement versus variable": the swept value
// (first variable), else the level index.
double level_x(const Dimension& d, std::size_t level) {
  const bool numeric = (d.kind == Dimension::Kind::sweep || d.kind == Dimension::Kind::doe) && !d.values[level].empty();
  return numeric ? d.values[level][0] : static_cast<double>(level);
}

std::string other_levels(const RunSet& rs, const RunPoint& p, std::size_t skip) {
  std::string out;
  for (std::size_t d = 0; d < rs.dimensions.size(); ++d) {
    if (d != skip) {
      out += (out.empty() ? "" : ", ") + rs.dimensions[d].name + "=" + rs.dimensions[d].label(p.levels[d]);
    }
  }
  return out;
}

}  // namespace

std::string_view retention_name(Retention r) noexcept {
  switch (r) {
    case Retention::all: return "all";
    case Retention::failing: return "failing";
    case Retention::none: return "none";
    case Retention::measured: return "measured";
  }
  return "all";
}

Retention default_retention(std::size_t points) noexcept {
  return points <= 100 ? Retention::all : Retention::failing;
}

bool keeps_raw(Retention keep, const RunPoint& p, bool failed, bool failed_spec) noexcept {
  if (p.index == 0 || p.nominal) {
    return true;
  }
  switch (keep) {
    case Retention::all:
    case Retention::measured: return true;
    case Retention::failing: return failed || failed_spec;
    case Retention::none: return false;
  }
  return true;
}

std::vector<Vector> retained_vectors(Retention keep, const std::vector<Vector>& vectors,
                                     const std::vector<std::string>& referenced) {
  if (keep != Retention::measured || vectors.empty()) {
    return vectors;
  }
  std::vector<Vector> out{vectors.front()};  // the scale vector
  for (std::size_t i = 1; i < vectors.size(); ++i) {
    if (std::find(referenced.begin(), referenced.end(), vectors[i].name) != referenced.end()) {
      out.push_back(vectors[i]);
    }
  }
  return out;
}

std::vector<PointResult> point_results(
    const RunSet& run_set, const JobManager& jobs,
    const std::function<std::vector<std::optional<double>>(const JobRecord&)>& measure) {
  std::vector<PointResult> out;
  out.reserve(run_set.points.size());
  for (std::size_t i = 0; i < run_set.points.size(); ++i) {
    PointResult row{i, run_set.records.size() > i ? run_set.records[i].job : point_job_id(run_set.spec.id, i),
                    "pending", {}};
    const JobRecord* rec = jobs.record(row.job);
    if (rec != nullptr) {
      row.state = std::string{job_state_name(rec->job.state)};
      if (measure && rec->job.state == JobState::completed) {
        row.measurements = measure(*rec);
      }
    }
    out.push_back(std::move(row));
  }
  return out;
}

std::string points_csv(const RunSet& run_set, const std::vector<std::string>& measurements,
                       const std::vector<PointResult>& rows) {
  std::string out = "index,job,state";
  for (const Dimension& d : run_set.dimensions) {
    out += "," + csv_field(d.name);
  }
  for (const std::string& m : measurements) {
    out += "," + csv_field(m);
  }
  out += "\n";
  for (const PointResult& row : rows) {
    out += std::to_string(row.index) + "," + csv_field(row.job) + "," + row.state;
    const RunPoint& p = run_set.points[row.index];
    for (std::size_t d = 0; d < run_set.dimensions.size(); ++d) {
      out += "," + csv_field(run_set.dimensions[d].label(p.levels[d]));
    }
    for (std::size_t m = 0; m < measurements.size(); ++m) {
      const bool has = m < row.measurements.size() && row.measurements[m];
      out += "," + (has ? number(*row.measurements[m]) : std::string{});
    }
    out += "\n";
  }
  return out;
}

std::string runset_json(const RunSet& run_set, const RunSetInfo& info) {
  nlohmann::json dims = nlohmann::json::array();
  for (const Dimension& d : run_set.dimensions) {
    dims.push_back(dimension_json(d));
  }
  nlohmann::json j{{"id", run_set.spec.id},
                   {"testbench", run_set.context.testbench},
                   {"state", info.state},
                   {"dimensions", std::move(dims)},
                   {"total", run_set.points.size()},
                   {"keep", std::string{retention_name(info.keep)}},
                   {"deckSha256", run_set.context.base.sha256},
                   {"pdk", {{"name", info.pdk}, {"descriptorVersion", info.descriptor_version}}}};
  if (info.stopped_early) {
    j["stoppedEarly"] = *info.stopped_early;
  }
  if (!info.rerun_of.empty()) {
    j["rerunOf"] = info.rerun_of;
  }
  return j.dump(2) + "\n";
}

diag::Status store_run_set(platform::FileSystem& files, const platform::Path& folder, const RunSet& run_set,
                           const RunSetInfo& info, const std::vector<std::string>& measurements,
                           const std::vector<PointResult>& rows) {
  if (auto status = files.create_directories(folder); !status.ok()) {
    return diag::make("SIM-RUNSET-016", "cannot create " + folder.string() + ": " + status.error().text);
  }
  std::string base;
  for (const std::string& line : run_set.context.base.lines) {
    base += line + "\n";
  }
  const std::pair<const char*, std::string> outputs[] = {
      {"runset.json", runset_json(run_set, info)}, {"base.spice", base}, {"points.csv", points_csv(run_set, measurements, rows)}};
  for (const auto& [name, text] : outputs) {
    if (auto status = files.write_atomic(folder / name, text); !status.ok()) {
      return diag::make("SIM-RUNSET-016", "cannot write " + (folder / name).string() + ": " + status.error().text);
    }
  }
  return diag::Done{};
}

std::string family_legend(std::string_view display, const RunSet& run_set, std::size_t point) {
  const RunPoint& p = run_set.points.at(point);
  return std::string{display} + " [" + other_levels(run_set, p, run_set.dimensions.size()) + "]";
}

std::optional<std::string> overlay_legend(std::string_view display, std::size_t count) {
  if (count > 200) {
    return std::string{display} + " (" + std::to_string(count) + " points)";
  }
  return std::nullopt;
}

std::vector<Curve> measurement_curves(const RunSet& run_set, std::size_t dimension,
                                      const std::vector<std::optional<double>>& values) {
  std::vector<Curve> curves;
  std::map<std::string, std::size_t> by_label;
  if (dimension >= run_set.dimensions.size()) {
    return curves;
  }
  for (const RunPoint& p : run_set.points) {
    const std::string label = other_levels(run_set, p, dimension);
    const auto [it, added] = by_label.emplace(label, curves.size());
    if (added) {
      curves.push_back(Curve{label, {}, {}});
    }
    Curve& c = curves[it->second];
    c.x.push_back(level_x(run_set.dimensions[dimension], p.levels[dimension]));
    c.y.push_back(p.index < values.size() ? values[p.index] : std::nullopt);
  }
  return curves;
}

}  // namespace hefesto::sim
