// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private input primitives of the .mag reader: Magic's character and line
// readers (FGETC, dbFgets) over an in-memory file, and a scanner that
// reproduces the sscanf conversions the reader uses (%d, %Ns, %c, %[^\n]).
// (Magic ref: database/DBio.c:4187-4214)

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace hefesto::cell::detail {

inline constexpr int kEof = -1;

/// An in-memory cell file read the way Magic reads a FILE.
class MagInput {
 public:
  explicit MagInput(std::string_view data) : data_(data) {}

  /// FGETC: the next byte (CR included) or kEof.
  int getc() noexcept { return pos_ < data_.size() ? static_cast<unsigned char>(data_[pos_++]) : kEof; }
  [[nodiscard]] int peek() const noexcept {
    return pos_ < data_.size() ? static_cast<unsigned char>(data_[pos_]) : kEof;
  }

  /// dbFgets: the next logical line with CR bytes dropped and the LF kept;
  /// lines starting with '#' are skipped and, in Hefesto (Q-CELL-1), so are
  /// blank lines. False at end of file with nothing read.
  bool fgets(std::string& line);

  /// Reads the rest of the current physical line (CR dropped, LF kept)
  /// without skipping anything; false at end of file with nothing read.
  bool rest_of_line(std::string& line);

  /// Skips whitespace bytes (GetRect between integers, LF included).
  int skip_space_then_getc() noexcept;

  [[nodiscard]] bool at_eof() const noexcept { return pos_ >= data_.size(); }

 private:
  std::string_view data_;
  std::size_t pos_ = 0;
};

/// sscanf-like scanning of one line. Each method is one conversion or
/// literal of a format string; a failed conversion leaves the scanner
/// in an unspecified position (callers stop at the first failure, as
/// sscanf does).
class Scan {
 public:
  explicit Scan(std::string_view text) : s_(text) {}

  /// A literal run of non-space format characters (no whitespace skip).
  bool lit(std::string_view word) noexcept;
  /// A whitespace directive: skips any run of whitespace (possibly none).
  void ws() noexcept;
  /// %Ns: skips whitespace, reads 1..max non-space characters.
  bool str(std::string& out, std::size_t max) noexcept;
  /// %d: skips whitespace, optional sign, at least one digit (32-bit wrap).
  bool integer(std::int32_t& out) noexcept;
  /// %lld (64-bit).
  bool integer64(std::int64_t& out) noexcept;
  /// %c: one character, no whitespace skip.
  bool chr(char& out) noexcept;
  /// %N[^\n]: 1..max characters other than LF, no whitespace skip.
  bool rest(std::string& out, std::size_t max = std::string::npos) noexcept;

  [[nodiscard]] std::string_view remaining() const noexcept { return s_.substr(pos_); }
  [[nodiscard]] bool at_end() const noexcept { return pos_ >= s_.size(); }

 private:
  std::string_view s_;
  std::size_t pos_ = 0;
};

[[nodiscard]] bool is_space(int c) noexcept;
[[nodiscard]] bool is_digit(int c) noexcept;

/// Whitespace-separated tokens of a line.
[[nodiscard]] std::size_t count_tokens(std::string_view text) noexcept;

/// The line without its trailing LF (for messages).
[[nodiscard]] std::string_view chomp(std::string_view line) noexcept;

}  // namespace hefesto::cell::detail
