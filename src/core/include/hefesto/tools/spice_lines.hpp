// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SPICE line preprocessing of readspice (openspec/changes/m4-scripting/
// design.md D11; capability readspice "Line preprocessing", "Subcircuit
// recognition"; Q-SCRIPT-32): comment lines dropped, `*.PININFO` kept for
// CDL files, `+` continuation lines joined in Magic's spacing (magic-compat
// ON) or the fixed spacing (OFF), and the tokenization of Tcl's `split`.
// Untrusted input: fuzzed by fuzz_readspice.
//
// (Magic ref: tcltk/readspice.tcl:58-98)
//
// Owner: TOOLS (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::tools {

/// How a `+` line continues the logical line (Q-SCRIPT-32).
///   magic: one space first unless the continuation line itself ends with a
///          space (readspice.tcl:69; magic-compat ON);
///   fixed: one space first unless the logical line already ends with one
///          (magic-compat OFF).
enum class ContinuationMode : std::uint8_t { magic, fixed };

/// Reads logical lines from a netlist's text. Physical lines end at `\n`,
/// `\r\n` or `\r` (Tcl's `gets` in auto translation). As readspice.tcl
/// does, the first logical line returned is the empty line the procedure
/// starts with (`set lastline ""`), and the last pending line is returned
/// at the end.
class SpiceLineReader {
 public:
  SpiceLineReader(std::string_view text, ContinuationMode mode, bool cdl);

  /// The next logical line, or nullopt at the end.
  [[nodiscard]] std::optional<std::string> next();

  [[nodiscard]] ContinuationMode mode() const noexcept { return mode_; }
  [[nodiscard]] bool cdl() const noexcept { return cdl_; }

 private:
  /// The next physical line, or nullopt at the end of the text.
  [[nodiscard]] std::optional<std::string_view> physical_line();
  void append_continuation(std::string_view line);

  std::string_view text_;
  std::size_t position_ = 0;
  ContinuationMode mode_;
  bool cdl_;
  std::string pending_;     // the logical line being built (`lastline`)
  bool finished_ = false;   // the last pending line was returned
};

/// Tcl's `split $text $separators`: one element per separator character,
/// so consecutive separators give empty elements.
[[nodiscard]] std::vector<std::string> tcl_split(std::string_view text, std::string_view separators);

/// The tokens of a logical line and its keyword (readspice.tcl:89-98):
/// `split $line` on whitespace, keyword = token 0 in lower case; a Spectre
/// `inline subckt` line is re-split on space, tab, `(` and `)`, its first
/// token dropped, and its keyword becomes `.subckt`.
struct SpiceLine {
  std::string keyword;
  std::vector<std::string> tokens;
};
[[nodiscard]] SpiceLine tokenize_spice_line(std::string_view line);

/// Tcl's `string tolower` for ASCII letters.
[[nodiscard]] std::string ascii_lower(std::string_view text);

}  // namespace hefesto::tools
