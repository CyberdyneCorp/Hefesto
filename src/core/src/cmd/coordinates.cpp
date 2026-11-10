// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Coordinate parsing, reported value formatting and pointer snapping
// (capability cmd-coordinates-and-units). Ported from Magic VLSI 8.3.694
// (fc5daa9e) commands/CmdSubrs.c (cmdScaleCoord), dbwind/DBWprocs.c
// (dbwPrintValue0, dbwValueFormat) and dbwind/DBWtools.c (ToolSnapToGrid),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md). Hefesto's decisions Q-CMDEDIT-1..5 are noted
// where they differ from Magic. Owner: EDIT.

#include "hefesto/cmd/coordinates.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "hefesto/diag/codes.hpp"

namespace hefesto::cmd {

namespace {

bool is_alnum(char c) noexcept {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool is_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool is_operator(char c) noexcept {
  return c == '+' || c == '-' || c == '*' || c == '/';
}

bool is_closer(char c) noexcept {
  return c == '}' || c == '"';
}

void warn(diag::DiagnosticSink* sink, std::string text) {
  if (sink != nullptr) {
    diag::Diagnostic d = diag::make(diag::codes::kCmdUsage, std::move(text));
    d.severity = diag::Severity::warning;
    sink->emit(d);
  }
}

diag::Diagnostic invalid(std::string text) {
  return diag::make(diag::codes::kCmdUsage, std::move(text));
}

// The metric multiplier k of `suffix` (nm per unit): 0 = not metric, -1 = an
// unknown `Xm` prefix.
double metric_k(std::string_view suffix) noexcept {
  if (suffix.size() >= 2 && suffix[1] == 'm') {
    switch (suffix[0]) {
      case 'n':
        return 1.0;
      case 'u':
        return 1000.0;
      case 'm':
        return 1.0e6;
      case 'c':
        return 1.0e7;
      default:
        return -1.0;
    }
  }
  if (suffix.front() == 'u' && (suffix.size() == 1 || !is_alnum(suffix[1]))) {
    return 1000.0;
  }
  if (suffix.starts_with("micron")) {
    return 1000.0;
  }
  if (suffix.starts_with("centimicron") || suffix == "cu") {
    return 10.0;
  }
  return 0.0;
}

// CIFGetOutputScale(k) in single precision, then the double division.
diag::Outcome<double> metric_to_internal(double v, double k, const CoordinateUnits& u) {
  if (!u.cif_scale) {
    return invalid("Metric units need a CIF/GDS output style");  // Q-CMDEDIT-3
  }
  const auto [s, e] = *u.cif_scale;
  const auto denominator = static_cast<float>(static_cast<double>(e) * k);
  const float divisor = static_cast<float>(10 * s) / denominator;
  return v / static_cast<double>(divisor);
}

diag::Outcome<double> grid_to_internal(double v, const CoordinateUnits& u, const DistanceRequest& r) {
  if (!u.grid) {
    return invalid("Grid units need a layout window");  // Q-CMDEDIT-4/5
  }
  const auto& g = *u.grid;
  const std::size_t lo = r.axis == Axis::x ? 0 : 1;
  const double pitch = static_cast<double>(g.at(lo + 2) - g.at(lo));
  return v * pitch + (r.relative ? 0.0 : static_cast<double>(g.at(lo)));
}

double lambda_to_internal(double v, const CoordinateUnits& u) {
  return v * static_cast<double>(u.internal_per) / static_cast<double>(u.lambda_per);
}

diag::Outcome<double> default_to_internal(double v, const CoordinateUnits& u, const DistanceRequest& r) {
  switch (u.default_unit) {
    case DefaultUnit::internal:
      return v;
    case DefaultUnit::user:
      return grid_to_internal(v, u, r);
    case DefaultUnit::microns:
      return metric_to_internal(v, 1000.0, u);
    case DefaultUnit::lambda:
      break;
  }
  return lambda_to_internal(v, u);
}

// An operand's suffix is the text up to the next blank, operator or closer.
std::size_t suffix_length(std::string_view rest) noexcept {
  std::size_t n = 0;
  while (n < rest.size() && !is_space(rest[n]) && !is_operator(rest[n]) && !is_closer(rest[n])) {
    ++n;
  }
  return n;
}

diag::Outcome<double> suffixed_to_internal(double v, std::string_view suffix, const CoordinateUnits& u,
                                           const DistanceRequest& r, diag::DiagnosticSink* sink) {
  switch (suffix.front()) {
    case 'l':
      return lambda_to_internal(v, u);
    case 'i':
      return v;
    case 'g':
      return grid_to_internal(v, u, r);
    default:
      break;
  }
  const double k = metric_k(suffix);
  if (k > 0.0) {
    return metric_to_internal(v, k, u);
  }
  if (k < 0.0) {
    warn(sink, "Unknown metric prefix \"" + std::string{suffix.substr(0, 2)} + "\"; assuming internal units");
  } else {
    warn(sink, "Unknown coordinate type at \"" + std::string{suffix} + "\"; assuming internal units");
  }
  return v;
}

// The unit conversion of one operand: `next` is the text right after the
// number.
diag::Outcome<double> to_internal(double v, std::string_view next, const CoordinateUnits& u,
                                  const DistanceRequest& r, diag::DiagnosticSink* sink) {
  const std::size_t n = suffix_length(next);
  if (n > 0) {
    return suffixed_to_internal(v, next.substr(0, n), u, r, sink);
  }
  // Q-CMDEDIT-1: directly before an operator, magic-compat takes internal units.
  if (!next.empty() && is_operator(next.front()) && u.magic_compat) {
    return v;
  }
  return default_to_internal(v, u, r);
}

// C's (int)round(v), saturated.
std::int64_t round_operand(double v) noexcept {
  constexpr double kLimit = 9.0e15;
  return static_cast<std::int64_t>(std::round(std::clamp(v, -kLimit, kLimit)));
}

std::int32_t wrap32(std::int64_t v) noexcept {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(v) & 0xFFFFFFFFULL));
}

diag::Outcome<std::int32_t> apply_op(char op, std::int32_t acc, std::int32_t operand) {
  const std::int64_t a = acc;
  const std::int64_t b = operand;
  switch (op) {
    case '+':
      return wrap32(a + b);
    case '-':
      return wrap32(a - b);
    case '*':
      return wrap32(a * b);
    case '/':
      if (b == 0) {
        return invalid("Division by zero in coordinate expression");  // Q-CMDEDIT-5
      }
      return wrap32(a / b);
    default:
      return operand;
  }
}

// What follows an operand: the next operator, or the end of the expression.
struct Continuation {
  char op = 0;  // 0: the expression ends
  std::size_t next = 0;
};

Continuation scan_operator(std::string_view text, std::size_t pos) noexcept {
  while (pos < text.size()) {
    const char c = text[pos];
    if (is_operator(c)) {
      ++pos;
      while (pos < text.size() && is_space(text[pos])) {
        ++pos;
      }
      return Continuation{c, pos};
    }
    if (c != ' ' && c != '\t') {
      break;  // a closer or any other character ends the expression
    }
    ++pos;
  }
  return Continuation{};
}

std::size_t skip_lead(std::string_view text) noexcept {
  std::size_t pos = 0;
  if (!text.empty() && (text.front() == '{' || text.front() == '"')) {
    pos = 1;
  }
  while (pos < text.size() && is_space(text[pos])) {
    ++pos;
  }
  return pos;
}

// The number at `pos` (strtod); nullopt when there is none.
struct Number {
  double value = 0.0;
  std::size_t end = 0;
};

std::optional<Number> read_number(std::string_view text, std::size_t pos) {
  const std::string copy{text.substr(pos)};
  char* end = nullptr;
  const double v = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str() || !std::isfinite(v)) {
    return std::nullopt;
  }
  return Number{v, pos + static_cast<std::size_t>(end - copy.c_str())};
}

// After a suffixed operand with magic-compat ON the expression ends
// (Q-CMDEDIT-2); a remainder other than a closer or blanks is reported.
void report_ignored(std::string_view rest, diag::DiagnosticSink* sink) {
  const std::size_t first = rest.find_first_not_of(" \t");
  if (first == std::string_view::npos || is_closer(rest[first])) {
    return;
  }
  warn(sink, "Ignored text after unit suffix: \"" + std::string{rest} + "\"");
}

}  // namespace

diag::Outcome<std::int32_t> parse_distance(std::string_view text, const CoordinateUnits& units,
                                           const DistanceRequest& request, diag::DiagnosticSink* sink) {
  std::size_t pos = skip_lead(text);
  std::int32_t acc = 0;
  char op = 0;
  while (pos < text.size()) {
    const auto number = read_number(text, pos);
    if (!number) {
      warn(sink, "Coordinate value cannot be parsed:  assuming 0");
      return acc;
    }
    const std::string_view next = text.substr(number->end);
    const auto value = to_internal(number->value * static_cast<double>(request.scale), next, units, request, sink);
    if (!value.ok()) {
      return value.error();
    }
    const auto applied = apply_op(op, acc, wrap32(round_operand(value.value())));
    if (!applied.ok()) {
      return applied.error();
    }
    acc = applied.value();
    const std::size_t suffix = suffix_length(next);
    if (suffix > 0 && units.magic_compat) {
      report_ignored(next.substr(suffix), sink);
      return acc;
    }
    const Continuation c = scan_operator(text, number->end + suffix);
    if (c.op == 0) {
      return acc;
    }
    op = c.op;
    pos = c.next;
  }
  return acc;
}

diag::Outcome<double> parse_coordinate(std::string_view text, const CoordinateUnits& units, Axis axis, bool relative,
                                       diag::DiagnosticSink* sink) {
  if (!read_number(text, skip_lead(text))) {
    return invalid("Coordinate value cannot be parsed: \"" + std::string{text} + "\"");
  }
  const auto v = parse_distance(text, units, DistanceRequest{axis, relative, 1}, sink);
  if (!v.ok()) {
    return v.error();
  }
  return static_cast<double>(v.value());
}

namespace {

// dbwValueFormat: trailing zeros, then a trailing point, removed.
std::string trimmed(double v, int places) {
  std::array<char, 64> buf{};
  std::snprintf(buf.data(), buf.size(), "%.*f", places, v);
  std::string s = buf.data();
  while (s.size() > 1 && s.back() == '0') {
    s.pop_back();
  }
  if (s.size() > 1 && s.back() == '.') {
    s.pop_back();
  }
  return s;
}

std::string with_suffix(std::string text, const UnitsState& units, std::string_view suffix) {
  if (units.print) {
    text += suffix;
  }
  return text;
}

float output_scale(const CoordinateUnits& scale) {
  if (!scale.cif_scale) {
    return 1.0F;  // CIFGetOutputScale without a style
  }
  const auto [s, e] = *scale.cif_scale;
  return static_cast<float>(10 * s) / static_cast<float>(e * 1000);
}

std::string format_scaled(std::int64_t value, float factor, ValueKind kind, const UnitsState& units,
                          std::string_view suffix) {
  const bool area = kind == ValueKind::area;
  const float f = area ? static_cast<float>(value) * factor * factor : static_cast<float>(value) * factor;
  return with_suffix(trimmed(static_cast<double>(f), area ? 6 : 3), units, suffix);
}

std::string format_user(std::int64_t value, const UnitsState& units, const std::array<std::int64_t, 4>& g,
                        ValueKind kind, Axis axis) {
  const std::int64_t px = g[2] - g[0];
  const std::int64_t py = g[3] - g[1];
  if (kind == ValueKind::area) {
    const float f = static_cast<float>(value) / static_cast<float>(px * py);
    return with_suffix(trimmed(static_cast<double>(f), 6), units, "gx*gy");
  }
  const bool x = axis == Axis::x;
  const float f = static_cast<float>(value) / static_cast<float>(x ? px : py);
  return with_suffix(trimmed(static_cast<double>(f), 3), units, x ? "gx" : "gy");
}

}  // namespace

std::string format_value(std::int64_t value, const UnitsState& units, const CoordinateUnits& scale, ValueKind kind,
                         Axis axis) {
  const bool area = kind == ValueKind::area;
  switch (units.unit) {
    case InputUnit::internal:
      return with_suffix(std::to_string(value), units, area ? "i^2" : "i");
    case InputUnit::lambda: {
      const float oscale = static_cast<float>(scale.lambda_per) / static_cast<float>(scale.internal_per);
      return format_scaled(value, oscale, kind, units, area ? "l^2" : "l");
    }
    case InputUnit::microns:
      return format_scaled(value, output_scale(scale), kind, units, area ? "um^2" : "um");
    case InputUnit::user:
      if (scale.grid) {
        return format_user(value, units, *scale.grid, kind, axis);
      }
      break;
    case InputUnit::default_units:
      break;
  }
  return std::to_string(value);
}

namespace {

// ToolSnapToGrid on one axis: the nearer bracketing grid line, ties upward.
db::Coord snap_axis(db::Coord x, db::Coord origin, db::Coord pitch) {
  if (pitch <= 0) {
    return x;
  }
  const std::int64_t t = std::int64_t{x} - origin;
  const std::int64_t line = pitch * (t / pitch) + origin;
  const std::int64_t lo = t >= 0 ? line : line - pitch;
  const std::int64_t hi = t >= 0 ? line + pitch : line;
  const std::int64_t pick = std::abs(x - lo) < std::abs(hi - x) ? lo : hi;
  return static_cast<db::Coord>(std::clamp<std::int64_t>(pick, db::kMinusInfinity, db::kInfinity));
}

}  // namespace

db::Point snap_point(db::Point p, const SnapGrid& grid) {
  return db::Point{snap_axis(p.x, grid.origin.x, grid.pitch_x), snap_axis(p.y, grid.origin.y, grid.pitch_y)};
}

}  // namespace hefesto::cmd
