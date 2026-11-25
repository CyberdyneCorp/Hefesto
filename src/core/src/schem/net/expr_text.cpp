// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `expr` markers in text and the printing of results (capability
// schematic-netlisting "expr evaluation"; design D9, D20). Numbers are
// printed with std::to_chars, the locale-independent equivalent of C
// `%.<n>g`.
//
// Owner: NETLIST.

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <vector>

#include "hefesto/schem/net/expr.hpp"

namespace hefesto::schem {

namespace {

/// Largest %g precision honoured; C's maximum useful one is 17 for
/// binary64, and anything above only pads.
constexpr int kMaxPrecision = 99;

struct Marker {
  std::size_t at = std::string_view::npos;  // position of `expr`
  std::size_t length = 0;                   // `expr(`, `expr_eng(` or `expr_eng4(`
  ExprStyle style = ExprStyle::plain;
};

/// The next marker at or after `from`.
Marker find_marker(std::string_view text, std::size_t from) noexcept {
  static constexpr std::pair<std::string_view, ExprStyle> kMarkers[] = {
      {"expr(", ExprStyle::plain},
      {"expr_eng(", ExprStyle::engineering},
      {"expr_eng4(", ExprStyle::engineering4},
  };
  for (std::size_t at = text.find("expr", from); at != std::string_view::npos; at = text.find("expr", at + 1)) {
    for (const auto& [marker, style] : kMarkers) {
      if (text.substr(at, marker.size()) == marker) {
        return {at, marker.size(), style};
      }
    }
  }
  return {};
}

/// For every `(` of a text, the position of its matching `)`: the first
/// later `)` that brings the nesting back to the level before it. npos for
/// an unmatched `(` and for every other character. One stack pass, so the
/// markers of a hostile text cost linear time, not quadratic.
using Matches = std::vector<std::size_t>;

Matches match_parens(std::string_view text) {
  Matches match(text.size(), std::string_view::npos);
  std::vector<std::size_t> open;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '(') {
      open.push_back(i);
    } else if (text[i] == ')' && !open.empty()) {
      match[open.back()] = i;
      open.pop_back();
    }
  }
  return match;
}

/// The extent of a marker's argument: [inner_begin, inner_end) is the
/// expression and `close` the position of its `)`; npos when unmatched.
struct Extent {
  std::size_t inner_begin = 0;
  std::size_t inner_end = 0;
  std::size_t close = std::string_view::npos;
  bool quoted = false;
};

/// `open_end` is the position just after the marker's `(`; `match` is
/// match_parens(text).
Extent find_extent(std::string_view text, const Matches& match, std::size_t open_end) noexcept {
  if (open_end < text.size() && text[open_end] == '\'') {
    const std::size_t quote = text.find('\'', open_end + 1);
    if (quote != std::string_view::npos && quote + 1 < text.size() && text[quote + 1] == ')') {
      return {open_end + 1, quote, quote + 1, true};
    }
  }
  const std::size_t close = match[open_end - 1];
  if (close == std::string_view::npos) {
    return {};
  }
  return {open_end, close, close, false};
}

/// `text` with every marker and each marker's matching `)` removed; the
/// quotes of `expr('...')` stay (golden case SCHEM-NL-03: a sky130 `ad`
/// left symbolic is written `'int((1 + 1)/2) * WN / 1 * 0.29'`).
std::string strip_markers(std::string_view text) {
  const Matches match = match_parens(text);
  std::vector<bool> drop(text.size(), false);
  for (Marker m = find_marker(text, 0); m.at != std::string_view::npos; m = find_marker(text, m.at + 1)) {
    const std::size_t open_end = m.at + m.length;
    std::fill(drop.begin() + static_cast<std::ptrdiff_t>(m.at), drop.begin() + static_cast<std::ptrdiff_t>(open_end),
              true);
    const Extent e = find_extent(text, match, open_end);
    if (e.close == std::string_view::npos) {
      continue;
    }
    drop[e.close] = true;
  }
  std::string out;
  out.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (!drop[i]) {
      out += text[i];
    }
  }
  return out;
}

std::string format_result(double value, ExprStyle style, const ExprOptions& options) {
  switch (style) {
    case ExprStyle::engineering: return format_engineering(value, options.ev_precision);
    case ExprStyle::engineering4: return format_engineering(value, 4);
    case ExprStyle::plain: break;
  }
  return format_g(value, 15);
}

}  // namespace

std::string format_g(double value, int precision) {
  if (std::isnan(value)) {
    return "nan";  // one spelling on every platform (D20), whatever the sign bit
  }
  if (precision < 0) {
    precision = 6;  // C: a negative precision is taken as if omitted
  }
  precision = std::min(std::max(precision, 1), kMaxPrecision);
  std::array<char, 512> buffer{};
  const auto [end, ec] =
      std::to_chars(buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::general, precision);
  if (ec != std::errc{}) {
    return {};
  }
  return std::string{buffer.data(), end};
}

std::string format_engineering(double value, int digits) {
  static constexpr std::pair<double, std::string_view> kLarge[] = {
      {1e12, "T"}, {1e9, "G"}, {1e6, "MEG"}, {1e3, "k"}};
  struct Small {
    double threshold;
    double factor;
    std::string_view suffix;
  };
  static constexpr Small kSmall[] = {{0.999999e-3, 1e3, "m"},
                                     {0.999999e-6, 1e6, "u"},
                                     {0.999999e-9, 1e9, "n"},
                                     {0.999999e-12, 1e12, "p"},
                                     {0.999999e-15, 1e15, "f"}};
  const double magnitude = std::fabs(value);
  if (std::isnan(value)) {
    return "nana";  // NaN prints "nana" with expr_eng (schematic-netlisting "expr evaluation", Q-SCHEM-36)
  }
  if (magnitude < 0.999999e-23) {
    return "0";
  }
  for (const auto& [power, suffix] : kLarge) {
    if (magnitude > 0.999999 * power) {
      return format_g(value / power, digits) + std::string{suffix};
    }
  }
  if (magnitude > 0.0999999) {
    return format_g(value, digits);
  }
  for (const auto& small : kSmall) {
    if (magnitude > small.threshold) {
      return format_g(value * small.factor, digits) + std::string{small.suffix};
    }
  }
  return format_g(value * 1e18, digits) + "a";
}

ExprText substitute_expr(std::string_view text, const ExprOptions& options) {
  ExprText out;
  const Matches match = match_parens(text);
  std::size_t pos = 0;
  for (Marker m = find_marker(text, 0); m.at != std::string_view::npos; m = find_marker(text, pos)) {
    out.text.append(text.substr(pos, m.at - pos));
    const Extent e = find_extent(text, match, m.at + m.length);
    const auto value = e.close == std::string_view::npos
                           ? std::nullopt
                           : evaluate_expr_value(text.substr(e.inner_begin, e.inner_end - e.inner_begin),
                                                 options.xschem_compat, &out.warnings);
    if (!value) {
      out.text += strip_markers(text.substr(m.at));
      return out;
    }
    out.text += format_result(*value, m.style, options);
    pos = e.close + 1;
  }
  out.text.append(text.substr(pos));
  return out;
}

}  // namespace hefesto::schem
