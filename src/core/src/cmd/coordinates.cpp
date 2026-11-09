// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Unit handling ported from Magic VLSI 8.3.694 commands/CmdSubrs.c
// (cmdScaleCoord), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).

#include "hefesto/cmd/coordinates.hpp"

#include <cmath>
#include <cstdlib>
#include <string>

#include "hefesto/diag/codes.hpp"

namespace hefesto::cmd {

namespace {

bool is_alnum(char c) noexcept {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
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

double lambda_to_internal(double v, const CoordinateUnits& u) {
  return v * static_cast<double>(u.internal_per) / static_cast<double>(u.lambda_per);
}

double grid_to_internal(double v, const CoordinateUnits& u, Axis axis, bool relative) {
  if (!u.grid) {
    return 0.0;  // no window: the parse ends with the value accumulated so far (Q-CMDEDIT-4)
  }
  const auto& g = *u.grid;
  const std::size_t lo = axis == Axis::x ? 0 : 1;
  const double pitch = static_cast<double>(g.at(lo + 2) - g.at(lo));
  return v * pitch + (relative ? 0.0 : static_cast<double>(g.at(lo)));
}

// CIFGetOutputScale(k) in single precision, then the double division.
double metric_to_internal(double v, double k, const CoordinateUnits& u) {
  if (!u.cif_scale) {
    return v;  // no output style: CIFGetOutputScale returns 1
  }
  const auto [s, e] = *u.cif_scale;
  const float divisor = static_cast<float>(10 * s) / static_cast<float>(static_cast<double>(e) * k);
  return v / static_cast<double>(divisor);
}

std::string_view default_suffix(DefaultUnit unit) noexcept {
  switch (unit) {
    case DefaultUnit::internal:
      return "i";
    case DefaultUnit::user:
      return "g";
    case DefaultUnit::microns:
      return "um";
    case DefaultUnit::lambda:
      break;
  }
  return "l";
}

void note(diag::DiagnosticSink* sink, std::string text) {
  if (sink != nullptr) {
    diag::Diagnostic d = diag::make(diag::codes::kCmdUsage, std::move(text));
    d.severity = diag::Severity::warning;
    sink->emit(d);
  }
}

}  // namespace

diag::Outcome<double> parse_coordinate(std::string_view text, const CoordinateUnits& units, Axis axis, bool relative,
                                       diag::DiagnosticSink* sink) {
  const std::string copy{text};
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str() || !std::isfinite(value)) {
    return diag::make(diag::codes::kCmdUsage, "Coordinate value cannot be parsed: \"" + copy + "\"");
  }
  std::string_view suffix{end};
  if (suffix.empty()) {
    suffix = default_suffix(units.default_unit);
  }
  if (suffix == "l") {
    return lambda_to_internal(value, units);
  }
  if (suffix == "i") {
    return value;
  }
  if (suffix == "g") {
    return grid_to_internal(value, units, axis, relative);
  }
  const double k = metric_k(suffix);
  if (k > 0.0) {
    return metric_to_internal(value, k, units);
  }
  if (k < 0.0) {
    note(sink, "Unknown metric prefix \"" + std::string{suffix.substr(0, 2)} + "\"; assuming internal units");
  } else {
    note(sink, "Unknown coordinate type at \"" + std::string{suffix} + "\"; assuming internal units");
  }
  return value;
}

}  // namespace hefesto::cmd
