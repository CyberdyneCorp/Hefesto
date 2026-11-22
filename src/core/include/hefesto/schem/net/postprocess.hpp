// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The rules that turn the netlist text into the final netlist lines
// (openspec/changes/m8-schematic design.md D9; capability
// schematic-netlisting "Element line post-processing" with the Q-SCHEM-22
// branch point and gawk case rules (Q-SCHEM-18), and "Line breaking",
// Q-SCHEM-20). Written natively from the spec; no awk, no external program.
//
// Owner: NETLIST.

#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::schem {

/// One piece of netlist text before post-processing: an element (the text
/// of one instance, with its spice prefix), the header code, or the lines of
/// "Netlist file structure" (comments, `.subckt` lines, markers).
struct NetlistChunk {
  std::string text;          // LF-separated lines
  std::string spice_prefix;  // put before each element name of this chunk
  /// Header code: a user-code block that has no begin/end marker lines.
  bool user_code = false;

  friend bool operator==(const NetlistChunk&, const NetlistChunk&) = default;
};

/// The begin and end lines of an architecture code block.
inline constexpr std::string_view kBeginUserCode = "**** begin user architecture code";
inline constexpr std::string_view kEndUserCode = "**** end user architecture code";

/// One line after "Element line post-processing", with what "Line breaking"
/// needs to know about it.
struct ProcessedLine {
  std::string text;
  /// Inside an architecture code block, or its begin or end line: never
  /// wrapped. Header code lines are wrapped like other lines.
  bool architecture_code = false;

  friend bool operator==(const ProcessedLine&, const ProcessedLine&) = default;
};

/// "Element line post-processing" of the whole netlist text. `xschem_compat`
/// is the Q-SCHEM-22 branch point (trailing `m=1`); every other rule is the
/// same in both modes. A vector element whose lines would pass kMaxBusBytes
/// is written unexpanded with a SCHEM-NET-006 warning to `warnings` (when
/// not null).
[[nodiscard]] std::vector<ProcessedLine> postprocess(std::span<const NetlistChunk> chunks, bool xschem_compat,
                                                     std::vector<diag::Diagnostic>* warnings = nullptr);

/// The longest segment "Line breaking" leaves on one line.
inline constexpr std::size_t kMaxSegment = 130;

/// "Line breaking": trailing blanks removed, long lines wrapped with `+`
/// (or `*+`) continuation lines, the quote state carried across lines.
/// Returns the text with LF line ends, one per line.
[[nodiscard]] std::string break_lines(std::span<const ProcessedLine> lines);

/// postprocess() then break_lines().
[[nodiscard]] std::string finish_netlist(std::span<const NetlistChunk> chunks, bool xschem_compat,
                                         std::vector<diag::Diagnostic>* warnings = nullptr);

/// The Q-SCHEM-22 branch point: whether a trailing ` m=1` is removed from
/// `line`. ON: from every line. OFF: only from device lines, which are lines
/// outside user-code blocks whose first field does not start with `*` or
/// `.`.
[[nodiscard]] bool strips_trailing_m1(std::string_view line, bool in_user_code, bool xschem_compat) noexcept;

/// `line` without a trailing match of ` [mM] *= *1 *$`.
[[nodiscard]] std::string remove_trailing_m1(std::string_view line);

}  // namespace hefesto::schem
