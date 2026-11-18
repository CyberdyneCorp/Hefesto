// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF/DEF tokenizer and message reporting (capability lef-read
// "LEF/DEF tokenizer", "LEF/DEF message reporting";
// openspec/changes/m6-lef-def-oasis/design.md D4, D6):
// - Lexer: LefNextToken and LefEstimate (lefRead.c) over a byte source:
//   2048-character line reads, `#` comments, quoted strings, `;` glued to a
//   number kept in its token, the end-of-line token of END matching, and
//   the line and byte position of each token for messages and progress.
//   LEF read and DEF read share it, each with its own keyword tables;
// - MessageCounter: LefError's counters (100 printed messages of each
//   kind, then one "further ... will not be reported" notice) and the
//   totals printed at the end of a read.
//
// The statement helpers built on both (END matching, statement and section
// skipping, keyword lookup, number parsing) are in lef/lex/statements.hpp.
//
// Owner: LEFR (design "As-landed scaffold contracts"; the scaffold created
// this header).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"

namespace hefesto::gds {
class ByteSource;
}

namespace hefesto::lef {

/// One token and where it starts.
struct Token {
  std::string text;
  std::uint32_t line = 0;    // 1-based
  std::uint64_t offset = 0;  // byte offset in the decompressed stream
  /// The end-of-line token of EolMode::report (text "\n"): LefNextToken's
  /// answer when it had to read a new line.
  bool end_of_line = false;

  friend bool operator==(const Token&, const Token&) = default;
};

/// LefNextToken's `ignore_eol` argument.
enum class EolMode : std::uint8_t {
  ignore,  // ignore_eol TRUE: line ends are whitespace
  report,  // ignore_eol FALSE: reading a new line first yields an end-of-line token
};

class Lexer {
 public:
  /// The characters of one line read (lefInt.h LEF_LINE_MAX). A longer line
  /// is read as consecutive pieces, each counted as a line.
  static constexpr std::size_t kLineMax = 2048;

  /// Tokenizes `source`, which must outlive the lexer. `size_hint` is the
  /// input size in bytes for estimate(); 0 when unknown.
  explicit Lexer(gds::ByteSource& source, std::uint64_t size_hint = 0) : source_(&source), size_hint_(size_hint) {}

  /// The next token; nullopt at end of input. A read error of the source
  /// is returned once; afterwards the lexer is at end of input.
  [[nodiscard]] diag::Outcome<std::optional<Token>> next(EolMode mode = EolMode::ignore);

  /// next() for the readers: a read error ends the input and is kept in
  /// failure().
  [[nodiscard]] std::optional<Token> take(EolMode mode = EolMode::ignore);

  /// The read error that ended the input, if any.
  [[nodiscard]] const std::optional<diag::Diagnostic>& failure() const noexcept { return failure_; }

  /// The current line (1-based; 0 before the first read): the number of
  /// line reads so far, comment and blank lines included, the continuation
  /// reads of a multi-line quoted token not.
  [[nodiscard]] std::uint32_t line() const noexcept { return line_; }

  /// The tokenized input.
  [[nodiscard]] gds::ByteSource& source() const noexcept { return *source_; }

  /// The bytes consumed by line reads so far.
  [[nodiscard]] std::uint64_t consumed() const noexcept { return consumed_; }

  /// The fraction of the input consumed, in [0, 1], for progress reports
  /// (Q-LEFDEF-88); 0 when the size is unknown.
  [[nodiscard]] double estimate() const noexcept;

 private:
  diag::Outcome<bool> refill();
  diag::Outcome<bool> read_line(std::size_t max_chars, std::string& out);
  diag::Outcome<bool> start_line();
  diag::Outcome<bool> scan_quoted(std::size_t& pos);
  std::size_t scan_plain(std::size_t pos) const noexcept;
  void finish_token(std::size_t pos) noexcept;

  gds::ByteSource* source_;
  std::uint64_t size_hint_ = 0;
  std::uint32_t line_ = 0;
  std::uint64_t consumed_ = 0;
  // Read-ahead from the source.
  std::vector<std::byte> chunk_;
  std::size_t chunk_pos_ = 0;
  bool source_done_ = false;
  // The current line (a C string: cut at its first NUL), as LefNextToken's
  // static buffer, and the start of the next token in it.
  std::string buffer_;
  std::uint64_t buffer_offset_ = 0;
  std::size_t next_ = 0;
  bool has_next_ = false;  // LefNextToken's nexttoken != NULL
  bool ended_ = false;
  std::optional<diag::Diagnostic> failure_;
};

/// Which reader a message belongs to: the prefix of LefError's lines.
enum class Dialect : std::uint8_t { lef, def };

/// LefError's counters for one read. Errors, warnings and messages (info)
/// are counted separately; LEF and DEF messages share the counters, as
/// LefError's static counters do.
class MessageCounter {
 public:
  /// The cap of messages printed per kind (lefInt.h LEF_MAX_ERRORS).
  static constexpr std::uint32_t kLimit = 100;

  /// Counts `severity` (debug counts as a message); true when the message
  /// is to be printed (fewer than kLimit of its kind before it).
  bool count(diag::Severity severity);

  /// LefError: counts the message and emits it to `sink` as
  /// "<LEF|DEF> read, Line <n> (Error|Warning|Message): <text>" (without a
  /// line when `line` is nullopt). The message after the cap is replaced by
  /// the one "further ... will not be reported" notice (LEF-LEX-001);
  /// later ones are only counted. `code` is the caller's code.
  void report(diag::DiagnosticSink& sink, Dialect dialect, diag::Severity severity, std::string_view code,
              std::string_view text, std::optional<std::uint32_t> line);

  /// LefError(LEF_SUMMARY / DEF_SUMMARY): emits the nonzero error and
  /// warning totals (LEF-LEX-002) and resets every counter.
  void summarize(diag::DiagnosticSink& sink, Dialect dialect);

  [[nodiscard]] std::uint32_t errors() const noexcept { return errors_; }
  [[nodiscard]] std::uint32_t warnings() const noexcept { return warnings_; }
  [[nodiscard]] std::uint32_t messages() const noexcept { return messages_; }

 private:
  std::uint32_t errors_ = 0;
  std::uint32_t warnings_ = 0;
  std::uint32_t messages_ = 0;
};

}  // namespace hefesto::lef
