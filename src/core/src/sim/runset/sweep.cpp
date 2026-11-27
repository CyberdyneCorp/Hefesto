// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Sweep variables, sweep levels and DOE rows (sweep.hpp).
//
// Owner: SIMADV.

#include "hefesto/sim/runset/sweep.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <utility>

#include "hefesto/sim/runset/random.hpp"
#include "hefesto/sim/deck/spice_text.hpp"

namespace hefesto::sim {

namespace {

bool valid_identifier(std::string_view name) {
  if (name.empty() || !(std::isalpha(static_cast<unsigned char>(name[0])) != 0 || name[0] == '_')) {
    return false;
  }
  return std::all_of(name.begin(), name.end(),
                     [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; });
}

diag::Diagnostic sweep_error(const std::string& variable, const std::string& problem) {
  return diag::make("SIM-RUNSET-003", "Sweep " + variable + ": " + problem);
}

diag::Diagnostic too_many(std::size_t n, std::size_t limit) {
  return diag::make("SIM-RUNSET-001", "Run set has " + std::to_string(n) + " points; the limit is " +
                                          std::to_string(limit));
}

bool finite(std::initializer_list<double> values) {
  return std::all_of(values.begin(), values.end(), [](double v) { return std::isfinite(v); });
}

// floor(x + 1e-9) + 1 as a count, or nullopt when it is not a finite count
// within `limit` (checked before any allocation).
diag::Outcome<std::size_t> level_count(double x, std::size_t limit) {
  const double n = std::floor(x + 1e-9) + 1.0;
  if (!std::isfinite(n) || n < 1.0) {
    return diag::make("SIM-RUNSET-003", "the level count is not finite");
  }
  if (n > static_cast<double>(limit)) {
    const double shown = std::min(n, 1e18);
    return too_many(static_cast<std::size_t>(shown), limit);
  }
  return static_cast<std::size_t>(n);
}

template <typename F>
std::vector<double> generate(std::size_t n, F&& level) {
  std::vector<double> out(n);
  for (std::size_t k = 0; k < n; ++k) {
    out[k] = level(static_cast<double>(k));
  }
  return out;
}

diag::Outcome<std::vector<double>> lin_points(const SweepDefinition& s, std::size_t limit) {
  if (s.count < 2) {
    return sweep_error(s.variable, "lin points must be >= 2");
  }
  if (s.count > limit) {
    return too_many(s.count, limit);
  }
  const double a = s.start;
  const double b = s.stop;
  const double d = static_cast<double>(s.count - 1);
  return generate(s.count, [a, b, d](double k) { return a + k * (b - a) / d; });
}

diag::Outcome<std::vector<double>> lin_step(const SweepDefinition& s, std::size_t limit) {
  if (s.step == 0.0 || !std::isfinite(s.step)) {
    return sweep_error(s.variable, "lin step must not be 0");
  }
  const double span = (s.stop - s.start) / s.step;
  if (span < 0.0) {
    return sweep_error(s.variable, "lin step does not lead from start to stop");
  }
  auto n = level_count(span, limit);
  if (!n.ok()) {
    return n.error().code == "SIM-RUNSET-001" ? n.error() : sweep_error(s.variable, n.error().text);
  }
  const double a = s.start;
  const double step = s.step;
  return generate(n.value(), [a, step](double k) { return a + k * step; });
}

diag::Outcome<std::vector<double>> logarithmic(const SweepDefinition& s, std::size_t limit) {
  const bool dec = s.mode == SweepDefinition::Mode::dec;
  const std::string mode = dec ? "dec" : "oct";
  if (s.start <= 0.0) {
    return sweep_error(s.variable, mode + " start must be > 0");
  }
  if (s.stop <= 0.0) {
    return sweep_error(s.variable, mode + " stop must be > 0");
  }
  if (s.count < 1) {
    return sweep_error(s.variable, mode + " points must be >= 1");
  }
  const double ratio = dec ? std::log10(s.stop / s.start) : std::log2(s.stop / s.start);
  auto n = level_count(static_cast<double>(s.count) * std::fabs(ratio), limit);
  if (!n.ok()) {
    return n.error().code == "SIM-RUNSET-001" ? n.error() : sweep_error(s.variable, n.error().text);
  }
  const double sign = ratio < 0.0 ? -1.0 : 1.0;
  const double base = dec ? 10.0 : 2.0;
  const double a = s.start;
  const double m = static_cast<double>(s.count);
  return generate(n.value(), [a, base, sign, m](double k) { return a * std::pow(base, sign * k / m); });
}

diag::Outcome<std::vector<double>> list(const SweepDefinition& s, std::size_t limit) {
  if (s.values.empty()) {
    return sweep_error(s.variable, "list needs at least one value");
  }
  if (s.values.size() > limit) {
    return too_many(s.values.size(), limit);
  }
  if (!std::all_of(s.values.begin(), s.values.end(), [](double v) { return std::isfinite(v); })) {
    return sweep_error(s.variable, "list values must be finite");
  }
  return s.values;
}

// --- DOE ---

diag::Diagnostic doe_error(const std::string& problem) {
  return diag::make("SIM-RUNSET-006", "DOE: " + problem);
}

diag::Status check_factor(const DoeFactor& f, DoeBlock::Method method) {
  if (!finite({f.lo, f.hi}) || !(f.hi > f.lo)) {
    return doe_error("factor " + f.variable + ": hi must be greater than lo");
  }
  if (f.log && f.lo <= 0.0) {
    return doe_error("factor " + f.variable + ": a log factor needs lo > 0");
  }
  if (method == DoeBlock::Method::full && f.levels < 2) {
    return doe_error("factor " + f.variable + ": full needs at least 2 levels");
  }
  return diag::Done{};
}

double factor_value(const DoeFactor& f, double u) {
  return f.log ? f.lo * std::pow(f.hi / f.lo, u) : f.lo + u * (f.hi - f.lo);
}

diag::Outcome<std::size_t> row_count(const DoeBlock& b, std::size_t limit) {
  switch (b.method) {
    case DoeBlock::Method::full: {
      double n = 1.0;  // in binary64: a product past the limit is reported, never allocated
      for (const DoeFactor& f : b.factors) {
        n = std::min(n * f.levels, 1e18);
      }
      if (n > static_cast<double>(limit)) {
        return too_many(static_cast<std::size_t>(n), limit);
      }
      return static_cast<std::size_t>(n);
    }
    case DoeBlock::Method::oat: return 1 + 2 * b.factors.size();
    case DoeBlock::Method::lhs:
      if (b.points < 2) {
        return doe_error("lhs needs at least 2 points");
      }
      return std::size_t{b.points};
    case DoeBlock::Method::random:
      if (b.points < 1) {
        return doe_error("random needs at least 1 point");
      }
      return std::size_t{b.points};
  }
  return doe_error("unknown method");
}

// Unit values of each method, row by row.
std::vector<std::vector<double>> full_units(const DoeBlock& b, std::size_t rows) {
  std::vector<std::vector<double>> u(rows, std::vector<double>(b.factors.size()));
  for (std::size_t r = 0; r < rows; ++r) {
    std::size_t rest = r;
    for (std::size_t j = b.factors.size(); j-- > 0;) {  // factor 0 outermost: the last one moves fastest
      const std::size_t levels = b.factors[j].levels;
      u[r][j] = static_cast<double>(rest % levels) / static_cast<double>(levels - 1);
      rest /= levels;
    }
  }
  return u;
}

std::vector<std::vector<double>> oat_units(std::size_t d) {
  std::vector<std::vector<double>> u(1 + 2 * d, std::vector<double>(d, 0.5));
  for (std::size_t j = 0; j < d; ++j) {
    u[1 + 2 * j][j] = 0.0;
    u[2 + 2 * j][j] = 1.0;
  }
  return u;
}

std::vector<std::vector<double>> lhs_units(std::size_t d, std::size_t n, std::uint64_t seed) {
  RandomStream stream(seed);
  std::vector<std::vector<double>> u(n, std::vector<double>(d));
  std::vector<std::size_t> perm(n);
  for (std::size_t j = 0; j < d; ++j) {
    std::iota(perm.begin(), perm.end(), std::size_t{0});
    for (std::size_t i = n - 1; i >= 1; --i) {
      const auto pick = static_cast<std::size_t>(std::floor(stream.next_unit() * static_cast<double>(i + 1)));
      std::swap(perm[i], perm[std::min(pick, i)]);
    }
    for (std::size_t i = 0; i < n; ++i) {
      u[i][j] = (static_cast<double>(perm[i]) + 0.5) / static_cast<double>(n);
    }
  }
  return u;
}

std::vector<std::vector<double>> random_units(std::size_t d, std::size_t n, std::uint64_t seed) {
  RandomStream stream(seed);
  std::vector<std::vector<double>> u(n, std::vector<double>(d));
  for (std::vector<double>& row : u) {
    for (double& x : row) {
      x = stream.next_unit();
    }
  }
  return u;
}

std::vector<std::vector<double>> units(const DoeBlock& b, std::size_t rows) {
  switch (b.method) {
    case DoeBlock::Method::full: return full_units(b, rows);
    case DoeBlock::Method::oat: return oat_units(b.factors.size());
    case DoeBlock::Method::lhs: return lhs_units(b.factors.size(), rows, b.seed);
    case DoeBlock::Method::random: return random_units(b.factors.size(), rows, b.seed);
  }
  return {};
}

}  // namespace

std::string SweepVariableRef::text() const {
  switch (kind) {
    case Kind::parameter: return "param:" + name;
    case Kind::temperature: return "temp";
    case Kind::supply: return "supply";
    case Kind::source: return "source:" + name;
  }
  return {};
}

bool SweepVariableRef::same_as(const SweepVariableRef& other) const {
  if (kind != other.kind) {
    return false;
  }
  return kind == Kind::parameter || kind == Kind::source ? ascii_lower(name) == ascii_lower(other.name) : true;
}

diag::Outcome<SweepVariableRef> parse_sweep_variable(std::string_view text) {
  if (text == "temp") {
    return SweepVariableRef{SweepVariableRef::Kind::temperature, {}};
  }
  if (text == "supply") {
    return SweepVariableRef{SweepVariableRef::Kind::supply, {}};
  }
  if (text == "corner") {
    return diag::make("SIM-RUNSET-004", "Sweep corner: not a sweep dimension; use the corner dimension");
  }
  for (const auto& [prefix, kind] : {std::pair{std::string_view{"param:"}, SweepVariableRef::Kind::parameter},
                                     std::pair{std::string_view{"source:"}, SweepVariableRef::Kind::source}}) {
    if (text.starts_with(prefix) && valid_identifier(text.substr(prefix.size()))) {
      return SweepVariableRef{kind, std::string{text.substr(prefix.size())}};
    }
  }
  return diag::make("SIM-RUNSET-004", "Sweep " + std::string{text} +
                                          ": unknown variable; use param:<name>, temp, supply or source:<name>");
}

diag::Status check_source_variable(const SweepVariableRef& variable, const std::vector<Stimulus>& stimuli) {
  if (variable.kind != SweepVariableRef::Kind::source) {
    return diag::Done{};
  }
  const auto it = std::find_if(stimuli.begin(), stimuli.end(), [&variable](const Stimulus& s) {
    return ascii_lower(s.name) == ascii_lower(variable.name);
  });
  if (it == stimuli.end()) {
    return diag::make("SIM-RUNSET-004", "Sweep " + variable.text() + ": not a stimulus of the testbench");
  }
  if (it->kind != Stimulus::Kind::dc) {
    return diag::make("SIM-RUNSET-004", "Sweep " + variable.text() + ": only dc stimuli can be swept; use a parameter");
  }
  return diag::Done{};
}

namespace {

// `param:wn:1:3` as {param:wn, 1, 3}: the colon-separated parts, with the
// variable's own colon joined back.
std::vector<std::string> factor_parts(std::string_view text) {
  std::vector<std::string> parts;
  for (std::size_t pos = 0;;) {
    const std::size_t colon = text.find(':', pos);
    parts.emplace_back(text.substr(pos, colon == std::string_view::npos ? std::string_view::npos : colon - pos));
    if (colon == std::string_view::npos) {
      break;
    }
    pos = colon + 1;
  }
  if (parts.size() > 1 && (parts[0] == "param" || parts[0] == "source")) {
    parts[1] = parts[0] + ":" + parts[1];
    parts.erase(parts.begin());
  }
  return parts;
}

// The optional `log` and `<levels>` parts; false when one is malformed.
bool factor_options(const std::vector<std::string>& parts, DoeFactor& f) {
  for (std::size_t i = 3; i < parts.size(); ++i) {
    const auto levels = parse_spice_number(parts[i]);
    if (parts[i] == "log" && !f.log) {
      f.log = true;
    } else if (levels && *levels >= 2 && *levels <= 1e9 && std::floor(*levels) == *levels) {
      f.levels = static_cast<std::uint32_t>(*levels);
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace

diag::Outcome<DoeFactor> parse_doe_factor(std::string_view text) {
  const std::string usage = "Usage: sim doe <full|oat|lhs|random> [-points <n>] [-seed <s>] <factor…>";
  const std::vector<std::string> parts = factor_parts(text);
  const auto lo = parts.size() >= 3 ? parse_spice_number(parts[1]) : std::nullopt;
  const auto hi = parts.size() >= 3 ? parse_spice_number(parts[2]) : std::nullopt;
  if (!lo || !hi || parts.size() > 5 || !parse_sweep_variable(parts[0]).ok()) {
    return diag::make("SIM-RUNSET-006", usage);
  }
  DoeFactor f{parts[0], *lo, *hi, false, 2};
  if (!factor_options(parts, f)) {
    return diag::make("SIM-RUNSET-006", usage);
  }
  return f;
}

diag::Outcome<std::vector<double>> sweep_levels(const SweepDefinition& sweep, std::size_t max_levels) {
  if (!finite({sweep.start, sweep.stop})) {
    return sweep_error(sweep.variable, "start and stop must be finite");
  }
  switch (sweep.mode) {
    case SweepDefinition::Mode::lin_points: return lin_points(sweep, max_levels);
    case SweepDefinition::Mode::lin_step: return lin_step(sweep, max_levels);
    case SweepDefinition::Mode::dec:
    case SweepDefinition::Mode::oct: return logarithmic(sweep, max_levels);
    case SweepDefinition::Mode::list: return list(sweep, max_levels);
  }
  return sweep_error(sweep.variable, "unknown mode");
}

std::string format_level(double value) {
  return format_g12(value);
}

diag::Outcome<std::vector<std::vector<double>>> doe_rows(const DoeBlock& block, std::size_t max_rows) {
  if (block.factors.empty()) {
    return doe_error("at least one factor is needed");
  }
  for (const DoeFactor& f : block.factors) {
    if (auto status = check_factor(f, block.method); !status.ok()) {
      return status.error();
    }
  }
  auto rows = row_count(block, max_rows);
  if (!rows.ok()) {
    return rows.error();
  }
  if (rows.value() > max_rows) {
    return too_many(rows.value(), max_rows);
  }
  std::vector<std::vector<double>> out = units(block, rows.value());
  for (std::vector<double>& row : out) {
    for (std::size_t j = 0; j < row.size(); ++j) {
      row[j] = factor_value(block.factors[j], row[j]);
    }
  }
  return out;
}

}  // namespace hefesto::sim
