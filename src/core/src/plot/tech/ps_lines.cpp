// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `postscript` style lines of the `plot` section (capability
// plot-output "PostScript style lines"; m10-plot-route design D6).
// Patterns, colours and layer styles are prepended, as Magic's lists, so
// the last line read comes first. Q-MISC-50 (2), fixed in both modes: a
// numeric field Magic reads with an unchecked sscanf must hold a number,
// else the line is reported and ignored.
//
// Ported from Magic VLSI 8.3.694 plot/plotPS.c (PlotPSTechLine), Copyright
// (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: PLOTTECH.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "display/c_scan.hpp"
#include "plot/tech/reader.hpp"

namespace hefesto::plot::detail {

namespace {

constexpr std::string_view kLineCount = "PLOT-TECH-002";
constexpr std::string_view kField = "PLOT-TECH-003";

/// sscanf(field, "%d"): the leading decimal integer, as C stores a long
/// into an int (low 32 bits); nullopt when the field has none.
std::optional<int> scan_decimal(std::string_view field) {
  display::cscan::Scanner scan(field);
  const std::optional<std::int64_t> value = scan.integer(10);
  if (!value) {
    return std::nullopt;
  }
  return static_cast<int>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(*value)));
}

/// sscanf(field, "%08lx"): at most eight characters read as hexadecimal
/// into an unsigned long.
std::optional<std::uint64_t> scan_hex8(std::string_view field) {
  display::cscan::Scanner scan(field.substr(0, 8));
  const std::optional<std::int64_t> value = scan.integer(16);
  if (!value) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(*value);
}

void not_an_integer(tech::Reporter& rep, const std::string& field) {
  rep.emit(kField, "\"ps\" line field \"" + field + "\" is not an integer");
}

void pattern_line(std::span<const std::string> tokens, tech::Reporter& rep, PlotTech& out) {
  PsPattern pattern;
  const std::optional<int> index = scan_decimal(tokens[0]);
  if (!index) {
    not_an_integer(rep, tokens[0]);
    return;
  }
  pattern.index = *index;
  for (std::size_t i = 0; i < pattern.rows.size(); ++i) {
    const std::optional<std::uint64_t> row = scan_hex8(tokens[1 + i]);
    if (!row) {
      not_an_integer(rep, tokens[1 + i]);
      return;
    }
    pattern.rows[i] = *row;
  }
  out.ps_patterns.insert(out.ps_patterns.begin(), pattern);
}

void color_line(std::span<const std::string> tokens, tech::Reporter& rep, PlotTech& out) {
  PsColor color;
  const std::optional<int> index = scan_decimal(tokens[0]);
  if (!index) {
    not_an_integer(rep, tokens[0]);
    return;
  }
  color.index = *index;
  for (std::size_t i = 0; i < color.cmyk.size(); ++i) {
    const std::optional<int> component = scan_decimal(tokens[1 + i]);
    if (!component) {
      not_an_integer(rep, tokens[1 + i]);
      return;
    }
    color.cmyk[i] = static_cast<std::uint8_t>(static_cast<unsigned>(*component) & 0xFFU);
  }
  out.ps_colors.insert(out.ps_colors.begin(), color);
}

/// The fill field: S, X, B or an integer (StrIsInt, then atoi).
std::optional<int> fill_of(const std::string& field) {
  if (field == "X") {
    return kPsCross;
  }
  if (field == "B") {
    return kPsBorder;
  }
  if (field == "S") {
    return kPsSolid;
  }
  if (!display::cscan::is_int(field)) {
    return std::nullopt;
  }
  return static_cast<int>(display::cscan::atoi(field));
}

void layer_line(std::span<const std::string> tokens, const tech::Technology& tech, tech::Reporter& rep,
                PlotTech& out) {
  if (!display::cscan::is_int(tokens[1])) {
    rep.emit(kField, "2nd field must be an integer");
    return;
  }
  const int color = static_cast<int>(display::cscan::atoi(tokens[1]));
  const std::optional<int> fill = fill_of(tokens[2]);
  if (!fill) {
    rep.emit(kField, "3rd field must be an integer or \"S\", \"X\", or \"B\".");
    return;
  }
  // DBTechNoisyNameMask, then the layer-composition expansion (each type's
  // DBLayerTypeMaskTbl entry is the type itself) restricted to
  // DBUserLayerBits.
  const tech::TypeList list = tech::name_mask(tech.data(), tokens[0], &rep, tech::Noise::warning);
  PsStyle style;
  for (tech::TileType t = tech::kFirstUserType; t < tech.user_type_end(); ++t) {
    if (list.types.test(t)) {
      style.types.set(t);
    }
  }
  style.color = color;
  style.stipple = *fill;
  out.ps_styles.insert(out.ps_styles.begin(), std::move(style));
}

}  // namespace

void postscript_line(std::span<const std::string> tokens, const tech::Technology& tech, tech::Reporter& rep,
                     PlotTech& out) {
  switch (tokens.size()) {
    case 9:
      pattern_line(tokens, rep, out);
      return;
    case 5:
      color_line(tokens, rep, out);
      return;
    case 3:
      layer_line(tokens, tech, rep, out);
      return;
    default:
      rep.emit(kLineCount, "\"ps\" lines must have either 9, 5, or 3 arguments.");
      return;
  }
}

}  // namespace hefesto::plot::detail
