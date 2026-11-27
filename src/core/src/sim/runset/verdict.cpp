// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Corner verdicts and the corner matrix (verdict.hpp).
//
// Owner: SIMADV.

#include "hefesto/sim/runset/verdict.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace hefesto::sim {

namespace {

// A column: a specification (spec != nullptr) or a plain measurement.
struct Column {
  std::string measurement;
  const CornerSpec* spec = nullptr;
};

std::optional<double> value_of(const CornerPoint& p, const std::string& measurement) {
  const auto it = std::find_if(p.values.begin(), p.values.end(), [&](const auto& v) { return v.first == measurement; });
  return it == p.values.end() ? std::nullopt : it->second;
}

// Larger is worse: the negated margin under a limit, else the value.
std::optional<double> badness(const std::optional<Limit>& limit, std::optional<double> value) {
  if (!value) {
    return std::nullopt;
  }
  if (!limit) {
    return value;
  }
  const auto m = margin(*limit, value);
  if (m) {
    return -*m;
  }
  return check_limit(*limit, value) == SpecResult::pass ? -0.0 : 1.0;
}

std::vector<Column> columns_of(const std::vector<CornerSpec>& specs, const std::vector<std::string>& measurements) {
  std::vector<Column> out;
  for (const CornerSpec& s : specs) {
    out.push_back(Column{s.measurement, &s});
  }
  for (const std::string& m : measurements) {
    const bool spec = std::any_of(specs.begin(), specs.end(), [&m](const CornerSpec& s) { return s.measurement == m; });
    if (!spec) {
      out.push_back(Column{m, nullptr});
    }
  }
  return out;
}

Verdict point_verdict(const CornerPoint& p, const std::vector<CornerSpec>& specs, const std::string& corner) {
  if (!p.final) {
    return Verdict::pending;
  }
  if (!p.completed) {
    return Verdict::error;
  }
  for (const CornerSpec& s : specs) {
    const auto limit = limit_at(s, corner);
    if (limit && check_limit(*limit, value_of(p, s.measurement)) != SpecResult::pass) {
      return Verdict::fail;
    }
  }
  return Verdict::pass;
}

// The worse of two verdicts: pending, then error, then fail, then pass.
Verdict combine(Verdict a, Verdict b) {
  const auto rank = [](Verdict v) {
    switch (v) {
      case Verdict::pending: return 3;
      case Verdict::error: return 2;
      case Verdict::fail: return 1;
      case Verdict::pass: return 0;
    }
    return 0;
  };
  return rank(a) >= rank(b) ? a : b;
}

CellMark mark_of(const Column& column, const std::optional<Limit>& limit, std::optional<double> value,
                 double threshold) {
  if (!value) {
    return CellMark::no_value;
  }
  if (column.spec == nullptr || !limit) {
    return CellMark::pass;
  }
  if (check_limit(*limit, value) != SpecResult::pass) {
    return CellMark::fail;
  }
  const auto m = margin(*limit, value);
  return m && *m < threshold ? CellMark::marginal : CellMark::pass;
}

MatrixCell worst_cell(const Column& column, const std::string& corner, const std::vector<const CornerPoint*>& points,
                      double threshold) {
  const std::optional<Limit> limit = column.spec != nullptr ? limit_at(*column.spec, corner) : std::nullopt;
  MatrixCell cell;
  std::optional<double> worst_badness;
  bool missing = false;
  for (const CornerPoint* p : points) {
    const std::optional<double> v = value_of(*p, column.measurement);
    const auto b = badness(limit, v);
    missing = missing || !v;
    if (b && (!worst_badness || *b > *worst_badness)) {
      worst_badness = b;
      cell.value = v;
    }
  }
  cell.mark = missing ? CellMark::no_value : mark_of(column, limit, cell.value, threshold);
  return cell;
}

MatrixRow corner_row(const std::string& corner, std::size_t level, const std::vector<CornerPoint>& points,
                     const std::vector<CornerSpec>& specs, const std::vector<Column>& columns, double threshold) {
  MatrixRow row{corner, Verdict::pass, {}};
  std::vector<const CornerPoint*> mine;
  for (const CornerPoint& p : points) {
    if (p.corner == level) {
      mine.push_back(&p);
      row.verdict = combine(row.verdict, point_verdict(p, specs, corner));
    }
  }
  if (mine.empty()) {
    row.verdict = Verdict::pending;
  }
  for (const Column& c : columns) {
    MatrixCell cell = worst_cell(c, corner, mine, threshold);
    if (row.verdict == Verdict::pending) {
      cell.mark = CellMark::pending;
    }
    row.cells.push_back(cell);
  }
  return row;
}

void summarise(CornerMatrix& m, const std::vector<Column>& columns) {
  for (std::size_t c = 0; c < columns.size(); ++c) {
    MatrixSummary s;
    std::optional<double> worst;
    for (const MatrixRow& row : m.rows) {
      const std::optional<Limit> limit = columns[c].spec != nullptr ? limit_at(*columns[c].spec, row.corner) : std::nullopt;
      const auto b = badness(limit, row.cells[c].value);
      if (b && (!worst || *b > *worst)) {
        worst = b;
        s.worst = row.cells[c].value;
        s.corner = row.corner;
      }
    }
    m.summary.push_back(std::move(s));
  }
}

}  // namespace

bool glob_match(std::string_view pattern, std::string_view name) noexcept {
  // Iterative matcher with one backtrack point for the last `*`.
  std::size_t p = 0;
  std::size_t n = 0;
  std::size_t star = std::string_view::npos;
  std::size_t resume = 0;
  while (n < name.size()) {
    if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == name[n])) {
      ++p;
      ++n;
    } else if (p < pattern.size() && pattern[p] == '*') {
      star = p++;
      resume = n;
    } else if (star != std::string_view::npos) {
      p = star + 1;
      n = ++resume;
    } else {
      return false;
    }
  }
  while (p < pattern.size() && pattern[p] == '*') {
    ++p;
  }
  return p == pattern.size();
}

SpecResult check_limit(const Limit& limit, std::optional<double> value) noexcept {
  if (!value || std::isnan(*value)) {
    return SpecResult::no_value;
  }
  const double v = *value;
  bool pass = false;
  switch (limit.kind) {
    case Limit::Kind::min: pass = v >= limit.a; break;
    case Limit::Kind::max: pass = v <= limit.a; break;
    case Limit::Kind::range: pass = limit.a <= v && v <= limit.b; break;
    case Limit::Kind::target: pass = limit.a - limit.b <= v && v <= limit.a + limit.b; break;
  }
  return pass ? SpecResult::pass : SpecResult::fail;
}

std::optional<double> margin(const Limit& limit, std::optional<double> value) noexcept {
  if (!value || std::isnan(*value)) {
    return std::nullopt;
  }
  const double v = *value;
  const auto one_sided = [&limit](double distance) -> std::optional<double> {
    const double divisor = limit.a != 0.0 ? std::fabs(limit.a) : limit.scale.value_or(0.0);
    return divisor != 0.0 ? std::optional<double>{distance / divisor} : std::nullopt;
  };
  switch (limit.kind) {
    case Limit::Kind::min: return one_sided(v - limit.a);
    case Limit::Kind::max: return one_sided(limit.a - v);
    case Limit::Kind::range: return std::min(v - limit.a, limit.b - v) / ((limit.b - limit.a) / 2.0);
    case Limit::Kind::target: return std::min(v - (limit.a - limit.b), limit.a + limit.b - v) / limit.b;
  }
  return std::nullopt;
}

std::optional<Limit> limit_at(const CornerSpec& spec, std::string_view corner) {
  const bool applies = spec.corners.empty() || std::any_of(spec.corners.begin(), spec.corners.end(),
                                                           [corner](const std::string& g) { return glob_match(g, corner); });
  if (!applies) {
    return std::nullopt;
  }
  for (const auto& [pattern, limit] : spec.overrides) {
    if (glob_match(pattern, corner)) {
      return limit;
    }
  }
  return spec.limit;
}

std::string_view verdict_name(Verdict v) noexcept {
  switch (v) {
    case Verdict::pass: return "pass";
    case Verdict::fail: return "fail";
    case Verdict::error: return "error";
    case Verdict::pending: return "pending";
  }
  return "pending";
}

std::string_view mark_symbol(CellMark m) noexcept {
  switch (m) {
    case CellMark::pass: return "✓";
    case CellMark::fail: return "✗";
    case CellMark::marginal: return "!";
    case CellMark::no_value: return "—";
    case CellMark::pending: return "…";
  }
  return "—";
}

CornerMatrix corner_matrix(const std::vector<std::string>& corners, const std::vector<CornerPoint>& points,
                           const std::vector<CornerSpec>& specs, const std::vector<std::string>& measurements,
                           double marginal_threshold) {
  CornerMatrix m;
  const std::vector<Column> columns = columns_of(specs, measurements);
  for (const Column& c : columns) {
    m.columns.push_back(c.measurement);
  }
  m.verdict = Verdict::pass;
  for (std::size_t level = 0; level < corners.size(); ++level) {
    m.rows.push_back(corner_row(corners[level], level, points, specs, columns, marginal_threshold));
    if (m.rows.back().verdict != Verdict::pass) {
      m.verdict = Verdict::fail;
    }
  }
  summarise(m, columns);
  return m;
}

std::vector<std::vector<std::string>> matrix_lists(const CornerMatrix& matrix) {
  std::vector<std::vector<std::string>> out;
  for (const MatrixRow& row : matrix.rows) {
    std::vector<std::string> list{row.corner, std::string{verdict_name(row.verdict)}};
    for (const MatrixCell& cell : row.cells) {
      list.push_back(cell.value ? format_si(*cell.value) : std::string{"—"});
    }
    out.push_back(std::move(list));
  }
  return out;
}

std::string format_si(double value) {
  if (value == 0.0 || !std::isfinite(value)) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%g", value);
    return buffer;
  }
  static constexpr const char* kPrefixes[] = {"f", "p", "n", "u", "m", "", "k", "M", "G", "T"};
  int exponent = static_cast<int>(std::floor(std::log10(std::fabs(value)) / 3.0)) * 3;
  exponent = std::clamp(exponent, -15, 12);
  char buffer[48];
  std::snprintf(buffer, sizeof buffer, "%.4g%s", value / std::pow(10.0, exponent), kPrefixes[(exponent + 15) / 3]);
  return buffer;
}

std::vector<std::size_t> failed_corners(const CornerMatrix& matrix) {
  std::vector<std::size_t> out;
  for (std::size_t i = 0; i < matrix.rows.size(); ++i) {
    if (matrix.rows[i].verdict == Verdict::fail || matrix.rows[i].verdict == Verdict::error) {
      out.push_back(i);
    }
  }
  return out;
}

}  // namespace hefesto::sim
