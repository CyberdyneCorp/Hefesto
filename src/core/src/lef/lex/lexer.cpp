// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF/DEF tokenizer (LefNextToken, lefRead.c:160-232) and message
// counter (LefError, lefRead.c:249-375) (capability lef-read "LEF/DEF
// tokenizer", "LEF/DEF message reporting"; design D4, D6).
//
// The lexer keeps LefNextToken's buffer semantics: each line read is an
// fgets of at most 2048 characters, cut at its first NUL as C string code
// sees it; a quoted token that reaches the line end appends the next read
// to the buffer (not counted as a line); terminating a token overwrites the
// character after it, so the character right after a closing quote is
// dropped. Unlike Magic, the end of input is final: once a read finds no
// more input every later call answers end of input (Magic re-reads its
// stale buffer after a quoted token runs into the end of the file).
//
// Owner: LEFR (design D2).

#include "hefesto/lef/lex/lexer.hpp"

#include <algorithm>
#include <span>
#include <utility>

#include "hefesto/gds/stream/io.hpp"

namespace hefesto::lef {

namespace {

constexpr std::size_t kChunk = std::size_t{64} * 1024;

constexpr std::string_view kNoticeCode = "LEF-LEX-001";
constexpr std::string_view kTotalsCode = "LEF-LEX-002";

/// isspace in the C locale.
bool is_space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/// Skips whitespace other than the line end, as LefNextToken does.
std::size_t skip_blanks(const std::string& text, std::size_t pos) noexcept {
  while (pos < text.size() && is_space(text[pos]) && text[pos] != '\n') {
    ++pos;
  }
  return pos;
}

/// A token can start at `pos`: not a comment, a line end or the end.
bool token_at(const std::string& text, std::size_t pos) noexcept {
  return pos < text.size() && text[pos] != '#' && text[pos] != '\n';
}

/// Cuts `text` at its first NUL at or after `from` (C string semantics).
void cut_at_nul(std::string& text, std::size_t from) {
  const std::size_t nul = text.find('\0', from);
  if (nul != std::string::npos) {
    text.resize(nul);
  }
}

std::string_view dialect_name(Dialect dialect) noexcept {
  return dialect == Dialect::lef ? "LEF" : "DEF";
}

std::string_view severity_label(diag::Severity severity) noexcept {
  switch (severity) {
    case diag::Severity::error:
      return "Error";
    case diag::Severity::warning:
      return "Warning";
    default:
      return "Message";
  }
}

/// LefError's suppression notice; its spelling ("Read" or "read") is
/// Magic's.
std::string notice_text(Dialect dialect, diag::Severity severity) {
  const std::string name{dialect_name(dialect)};
  switch (severity) {
    case diag::Severity::error:
      return name + " Read:  Further errors will not be reported.";
    case diag::Severity::warning:
      return name + " read:  Further warnings will not be reported.";
    default:
      return name + " read:  Further messages will not be reported.";
  }
}

diag::Diagnostic with_severity(std::string_view code, std::string text, diag::Severity severity) {
  diag::Diagnostic d = diag::make(code, std::move(text));
  d.severity = severity;
  return d;
}

std::string total_text(Dialect dialect, std::uint32_t count, std::string_view kind) {
  return std::string{dialect_name(dialect)} + " Read: encountered " + std::to_string(count) + " " +
         std::string{kind} + (count == 1 ? "" : "s") + " total.";
}

// What next() returns when a read that answered `got` (false: end of
// input) stops the token stream: the read's error, or no token.
diag::Outcome<std::optional<Token>> stream_end(const diag::Outcome<bool>& got) {
  if (!got.ok()) {
    return got.error();
  }
  return std::optional<Token>{};
}

}  // namespace

// --- Lexer ---------------------------------------------------------------------------

diag::Outcome<bool> Lexer::refill() {
  if (chunk_pos_ < chunk_.size()) {
    return true;
  }
  if (source_done_) {
    return false;
  }
  chunk_.resize(kChunk);
  auto got = source_->read(std::span<std::byte>{chunk_});
  if (!got.ok()) {
    chunk_.clear();
    return got.error();
  }
  chunk_.resize(got.value());
  chunk_pos_ = 0;
  source_done_ = got.value() == 0;
  return !source_done_;
}

// fgets(buf, max_chars + 1, f): appends at most `max_chars` characters,
// through the first line end; false at end of input when nothing was read.
// With max_chars 0 it reads nothing and answers true, as fgets with n = 1.
diag::Outcome<bool> Lexer::read_line(std::size_t max_chars, std::string& out) {
  if (max_chars == 0) {
    return true;
  }
  std::size_t taken = 0;
  while (taken < max_chars) {
    auto more = refill();
    if (!more.ok()) {
      return more.error();
    }
    if (!more.value()) {
      break;
    }
    const char c = static_cast<char>(chunk_[chunk_pos_++]);
    out.push_back(c);
    ++taken;
    ++consumed_;
    if (c == '\n') {
      break;
    }
  }
  return taken > 0;
}

// Reads lines until one holds a token; false at end of input.
diag::Outcome<bool> Lexer::start_line() {
  for (;;) {
    buffer_.clear();
    buffer_offset_ = consumed_;
    auto got = read_line(kLineMax, buffer_);
    if (!got.ok() || !got.value()) {
      return got;
    }
    cut_at_nul(buffer_, 0);
    ++line_;
    const std::size_t pos = skip_blanks(buffer_, 0);
    if (token_at(buffer_, pos)) {
      next_ = pos;
      has_next_ = true;
      return true;
    }
  }
}

// A token that starts with `"` runs to the next `"` not preceded by `\`;
// at a line end the next read is appended to the buffer. `pos` ends after
// the closing quote; false when the input ends inside the token.
diag::Outcome<bool> Lexer::scan_quoted(std::size_t& pos) {
  ++pos;
  while (pos < buffer_.size() && (buffer_[pos] != '"' || buffer_[pos - 1] == '\\')) {
    if (buffer_[pos] == '\n') {
      buffer_.resize(pos + 1);
      // fgets(nexttoken + 1, LEF_LINE_MAX - (nexttoken - line), f)
      auto got = read_line(kLineMax - pos - 1, buffer_);
      if (!got.ok() || !got.value()) {
        return got;
      }
      cut_at_nul(buffer_, pos + 1);
    }
    ++pos;
  }
  if (pos < buffer_.size() && buffer_[pos] == '"') {
    ++pos;
  }
  return true;
}

std::size_t Lexer::scan_plain(std::size_t pos) const noexcept {
  while (pos < buffer_.size() && !is_space(buffer_[pos])) {
    ++pos;
  }
  return pos;
}

// Terminates the token ending at `pos` (overwriting, and so skipping, the
// character there) and finds the start of the next one on this line.
void Lexer::finish_token(std::size_t pos) noexcept {
  if (pos < buffer_.size()) {
    ++pos;
  }
  pos = skip_blanks(buffer_, pos);
  has_next_ = token_at(buffer_, pos);
  next_ = pos;
}

diag::Outcome<std::optional<Token>> Lexer::next(EolMode mode) {
  if (ended_) {
    return std::optional<Token>{};
  }
  if (!has_next_) {
    auto started = start_line();
    if (!started.ok() || !started.value()) {
      ended_ = true;
      return stream_end(started);
    }
    if (mode == EolMode::report) {
      return std::optional<Token>{Token{"\n", line_, buffer_offset_ + next_, true}};
    }
  }
  const std::size_t start = next_;
  std::size_t end = start;
  if (buffer_[start] == '"') {
    auto closed = scan_quoted(end);
    if (!closed.ok() || !closed.value()) {
      ended_ = true;
      return stream_end(closed);
    }
  } else {
    end = scan_plain(start);
  }
  Token token{buffer_.substr(start, end - start), line_, buffer_offset_ + start, false};
  finish_token(end);
  return std::optional<Token>{std::move(token)};
}

std::optional<Token> Lexer::take(EolMode mode) {
  auto got = next(mode);
  if (!got.ok()) {
    failure_ = got.error();
    return std::nullopt;
  }
  return std::move(got).value();
}

double Lexer::estimate() const noexcept {
  if (size_hint_ == 0) {
    return 0.0;
  }
  return std::min(1.0, static_cast<double>(consumed_) / static_cast<double>(size_hint_));
}

// --- MessageCounter --------------------------------------------------------------------

bool MessageCounter::count(diag::Severity severity) {
  std::uint32_t* counter = &messages_;
  if (severity == diag::Severity::error) {
    counter = &errors_;
  } else if (severity == diag::Severity::warning) {
    counter = &warnings_;
  }
  return (*counter)++ < kLimit;
}

void MessageCounter::report(diag::DiagnosticSink& sink, Dialect dialect, diag::Severity severity,
                            std::string_view code, std::string_view text, std::optional<std::uint32_t> line) {
  const std::uint32_t before = severity == diag::Severity::error     ? errors_
                               : severity == diag::Severity::warning ? warnings_
                                                                     : messages_;
  if (count(severity)) {
    std::string prefix{dialect_name(dialect)};
    prefix += line ? " read, Line " + std::to_string(*line) + " (" : " read (";
    prefix += severity_label(severity);
    prefix += "): ";
    sink.emit(with_severity(code, prefix + std::string{text}, severity));
  } else if (before == kLimit) {
    sink.emit(with_severity(kNoticeCode, notice_text(dialect, severity), severity));
  }
}

void MessageCounter::summarize(diag::DiagnosticSink& sink, Dialect dialect) {
  if (errors_ != 0) {
    sink.emit(diag::make(kTotalsCode, total_text(dialect, errors_, "error")));
  }
  if (warnings_ != 0) {
    sink.emit(diag::make(kTotalsCode, total_text(dialect, warnings_, "warning")));
  }
  errors_ = 0;
  warnings_ = 0;
  messages_ = 0;
}

}  // namespace hefesto::lef
