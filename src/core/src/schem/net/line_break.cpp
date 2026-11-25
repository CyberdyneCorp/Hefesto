// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// "Line breaking" (capability schematic-netlisting; Q-SCHEM-20, replicate):
// trailing blanks removed, then the field rule for lines with no quote
// characters, or the quote rule, whose quote state is carried across lines.
//
// Owner: NETLIST.

#include "hefesto/schem/net/postprocess.hpp"

#include "line_fields.hpp"

namespace hefesto::schem {

namespace {

using detail::ascii_lower;
using detail::is_blank;
using detail::split_fields;

bool is_quote(char c) noexcept {
  return c == '{' || c == '}' || c == '\'';
}

std::string_view trim_right(std::string_view line) noexcept {
  while (!line.empty() && is_blank(line.back())) {
    line.remove_suffix(1);
  }
  return line;
}

/// The first field, lower-cased, contains `.inc(lude)?`, `.lib`, `.title`,
/// `.save` or `.write`; or the line matches `^\*\* ..._path:`.
bool never_wrapped(std::string_view line) {
  const auto fields = split_fields(line);
  if (!fields.empty()) {
    const std::string head = ascii_lower(fields.front());
    for (const std::string_view word : {".inc", ".lib", ".title", ".save", ".write"}) {
      if (head.find(word) != std::string::npos) {
        return true;
      }
    }
  }
  return line.size() >= 12 && line.starts_with("** ") && line.substr(6, 6) == "_path:";
}

std::string_view continuation(std::string_view line) noexcept {
  return line.starts_with('*') ? "\n*+" : "\n+";
}

/// The field rule: wrap before a field when the segment, counted up to the
/// end of that field with the blanks before each field, exceeds kMaxSegment;
/// the field after a `+` counts as zero.
std::string field_rule(std::string_view line) {
  std::string out;
  std::size_t segment = 0;
  std::size_t i = 0;
  bool first = true;
  while (i < line.size()) {
    const std::size_t blanks = i;
    while (i < line.size() && is_blank(line[i])) {
      ++i;
    }
    while (i < line.size() && !is_blank(line[i])) {
      ++i;
    }
    const std::size_t length = i - blanks;
    if (!first && segment + length > kMaxSegment) {
      out += continuation(line);
      segment = 0;
    } else {
      segment += length;
    }
    first = false;
    out += line.substr(blanks, i - blanks);
  }
  return out;
}

/// The quote rule: read character by character; wrap before a blank when
/// no quote is open and the segment before it is kMaxSegment characters or
/// longer; the next segment starts after that blank (golden case SCHEM-NL-05:
/// a sky130 line of exactly 130 characters before a blank wraps there, and a
/// continuation line of `+`, the blank and 129 characters does not).
std::string quote_rule(std::string_view line, bool wrap, bool& quote_open) {
  std::string out;
  std::size_t segment = 0;
  for (const char c : line) {
    const bool wraps = wrap && is_blank(c) && !quote_open && segment >= kMaxSegment;
    if (wraps) {
      out += continuation(line);
      segment = 0;
    }
    if (is_quote(c)) {
      quote_open = !quote_open;
    }
    out += c;
    segment += wraps ? 0 : 1;
  }
  return out;
}

}  // namespace

std::string break_lines(std::span<const ProcessedLine> lines) {
  std::string out;
  bool quote_open = false;
  for (const ProcessedLine& processed : lines) {
    const std::string_view line = trim_right(processed.text);
    if (!line.empty()) {
      const bool wrap = !processed.architecture_code && !never_wrapped(line);
      if (quote_open || line.find_first_of("{}'") != std::string_view::npos) {
        out += quote_rule(line, wrap, quote_open);
      } else {
        out += wrap ? field_rule(line) : std::string{line};
      }
    }
    out += '\n';
  }
  return out;
}

}  // namespace hefesto::schem
