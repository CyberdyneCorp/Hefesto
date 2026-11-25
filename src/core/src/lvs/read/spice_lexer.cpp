// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SPICE/CDL lexer (lvs-netlist-input "SPICE lexical structure"). Lines are
// joined without recursion and the joined text is bounded by the logical
// line limit of Q-LVS-36 before it is copied, so no input grows memory past
// the limit. A run of comment lines is scanned for a following continuation
// only after a non-comment line, which keeps the lexer linear.
//
// Owner: LVSREAD.

#include "lvs/read/spice_lexer.hpp"

namespace hefesto::lvs::spice {

namespace {

bool is_separator(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

}  // namespace

std::string_view Lexer::peek_line(std::size_t pos) const noexcept {
  const auto end = text_.find('\n', pos);
  return text_.substr(pos, (end == std::string_view::npos ? text_.size() : end) - pos);
}

bool Lexer::append_physical(std::string& out) {
  for (;;) {
    const auto end = text_.find('\n', pos_);
    auto segment = peek_line(pos_);
    if (segment.ends_with('\r')) {
      segment.remove_suffix(1);
    }
    const bool joined = segment.ends_with('\\');
    if (joined) {
      segment.remove_suffix(1);
    }
    if (out.size() + segment.size() > max_line_) {
      return false;
    }
    out.append(segment);
    pos_ = end == std::string_view::npos ? text_.size() : end + 1;
    ++line_;
    if (!joined || pos_ >= text_.size()) {
      return true;
    }
  }
}

std::size_t Lexer::continuation_after_comments(std::uint32_t& lines) const noexcept {
  std::size_t pos = pos_;
  std::uint32_t skipped = 0;
  while (pos < text_.size()) {
    const auto line = peek_line(pos);
    if (line.starts_with('+')) {
      lines = skipped;
      return pos;
    }
    if (!line.starts_with('*')) {
      return std::string_view::npos;
    }
    pos += line.size() + 1;
    ++skipped;
  }
  return std::string_view::npos;
}

Lexer::Next Lexer::next(LogicalLine& out) {
  if (pos_ >= text_.size()) {
    return Next::end;
  }
  out.text.clear();
  out.line = line_ + 1;
  if (!append_physical(out.text)) {
    return Next::too_long;
  }
  const bool comment = out.text.starts_with('*');
  for (;;) {
    std::uint32_t skipped = 0;
    std::size_t next_pos = std::string_view::npos;
    if (comment) {
      next_pos = (pos_ < text_.size() && text_[pos_] == '+') ? pos_ : std::string_view::npos;
    } else {
      next_pos = continuation_after_comments(skipped);
    }
    if (next_pos == std::string_view::npos) {
      return Next::line;
    }
    pos_ = next_pos + 1;  // past the '+'
    line_ += skipped;
    if (out.text.size() + 1 > max_line_) {
      return Next::too_long;
    }
    out.text.push_back(' ');
    if (!append_physical(out.text)) {
      return Next::too_long;
    }
  }
}

std::vector<std::string> tokenize(std::string_view line) {
  std::vector<std::string> tokens;
  std::string current;
  char quote = 0;
  for (const char c : line) {
    if (quote != 0) {
      if (!is_separator(c)) {
        current.push_back(c);
      }
      if (c == quote) {
        quote = 0;
      }
      continue;
    }
    if (is_separator(c)) {
      if (!current.empty()) {
        tokens.push_back(std::move(current));
        current.clear();
      }
      continue;
    }
    if (c == '"' || (c == '\'' && !(!current.empty() && is_digit(current.back())))) {
      quote = c;
    }
    current.push_back(c);
  }
  if (!current.empty()) {
    tokens.push_back(std::move(current));
  }
  return tokens;
}

}  // namespace hefesto::lvs::spice
