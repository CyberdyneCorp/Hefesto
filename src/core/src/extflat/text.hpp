// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Text helpers of the `.ext` reader (capability ext-flattening-read "Line
// tokenization" and "Keyword recognition"): efReadLine, the keyword table
// with LookupStruct matching, and the C conversions (atoi, atof, sscanf
// "%d") the reader applies to tokens, made total for untrusted input.
// (Magic ref: extflat/EFread.c:770-885, extflat/extparse.h:31-62,
// utils/lookup.c:166-230)
//
// Owner: EXTSUB (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D10).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace hefesto::extflat::detail {

/// The keywords of a `.ext` line (extparse.h Key), in keyTable order.
enum class Key : std::uint8_t {
  abstract_,
  adjust,
  attr,
  cap,
  connect,
  device,
  dist,
  equiv,
  fet,
  killnode,
  merge,
  node,
  parameters,
  port,
  primitive,
  resistor,
  resistclass,
  rnode,
  scale,
  subcap,
  substrate,
  tech,
  timestamp,
  use,
  version,
  style,
};

struct KeyInfo {
  std::string_view name;
  Key key;
  std::size_t min_tokens;
};

/// LookupStruct over keyTable: an exact match (case-insensitive) wins, a
/// unique prefix matches, an ambiguous or unknown word is nullopt.
[[nodiscard]] std::optional<KeyInfo> lookup_key(std::string_view word) noexcept;

/// C `atoi` as glibc computes it (strtol saturating at 64 bits, then
/// truncated to 32 bits): leading white space, an optional sign, digits;
/// 0 when no digit follows.
[[nodiscard]] std::int32_t c_atoi(std::string_view text) noexcept;
/// The same as a 64-bit value without the truncation (timestamps).
[[nodiscard]] std::int64_t c_atol(std::string_view text) noexcept;
/// C `atof` in the C locale (0 when nothing parses).
[[nodiscard]] double c_atof(std::string_view text) noexcept;
/// sscanf "%d" at the start of `text`: nullopt when no integer is there;
/// `consumed` receives the characters read.
[[nodiscard]] std::optional<std::int32_t> scan_int(std::string_view text, std::size_t* consumed = nullptr) noexcept;
/// (int)(0.5 + v) as C computes it (truncation toward zero), saturated to
/// the 32-bit range and 0 for NaN instead of undefined behaviour.
[[nodiscard]] std::int32_t round_half_up_trunc(double v) noexcept;

/// isspace() in the C locale, for one byte.
[[nodiscard]] constexpr bool c_isspace(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/// The tokens of one line, as the record readers take them.
using Args = std::span<const std::string_view>;

/// One logical line split into tokens (efReadLine). The tokens view the
/// reader's buffer: they stay valid until its next call.
struct Line {
  std::vector<std::string_view> tokens;
  std::uint32_t number = 0;  // the last physical line read (efReadLineNum)
  bool too_long = false;     // the logical line passed ReadLimits::max_line_bytes
};

/// efReadLine over a whole file held in memory.
class LineReader {
 public:
  LineReader(std::string_view text, std::size_t max_line_bytes) noexcept
      : text_(text), max_line_bytes_(max_line_bytes) {}

  /// The next non-empty, non-comment logical line into `out` (its buffers
  /// are reused); false at the end of the file (a continuation cut by the
  /// end of file is dropped, as fgets returning NULL ends efReadLine).
  [[nodiscard]] bool next(Line& out);

  /// A line with owned tokens (for readers outside the netlist hot path).
  struct OwnedLine {
    std::vector<std::string> tokens;
    std::uint32_t number = 0;
    bool too_long = false;
  };
  /// next(Line&) with owned tokens; nullopt at the end of the file.
  [[nodiscard]] std::optional<OwnedLine> next();

 private:
  [[nodiscard]] bool logical_line(std::string_view& out);
  [[nodiscard]] std::string_view physical_line();

  std::string_view text_;
  std::size_t pos_ = 0;
  std::uint32_t line_number_ = 0;
  std::size_t max_line_bytes_;
  bool too_long_ = false;
  std::string joined_;   // a logical line continued over physical lines
  std::string storage_;  // the characters of the current tokens
};

/// The tokenizer of efReadLine on one logical line: white space separates
/// tokens, double quotes group (and are removed), a backslash is copied
/// (and dropped at the end of the line). The token count is not capped
/// here; the reader rejects lines over 128 tokens. The characters go to
/// `storage` (resized to at least the line's length) and `out` (cleared) receives views of them.
void tokenize_into(std::string_view line, std::string& storage, std::vector<std::string_view>& out);
/// tokenize_into with owned tokens.
[[nodiscard]] std::vector<std::string> tokenize(std::string_view line);

}  // namespace hefesto::extflat::detail
