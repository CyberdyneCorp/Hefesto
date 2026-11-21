// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The byte-level lexer of `.sch`/`.sym` files; see lexer.hpp.
//
// Owner: FORMAT.

#include "lexer.hpp"

#include <algorithm>

#include "numbers.hpp"

namespace hefesto::schem::file_detail {

namespace {

bool is_whitespace(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

}  // namespace

std::uint32_t Lexer::line() {
  const auto end = std::min(pos_, bytes_.size());
  if (end < line_pos_) {  // the position never moves back; keep the count safe anyway
    line_pos_ = 0;
    line_ = 1;
  }
  line_ += static_cast<std::uint32_t>(std::count(bytes_.begin() + static_cast<std::ptrdiff_t>(line_pos_),
                                                 bytes_.begin() + static_cast<std::ptrdiff_t>(end), '\n'));
  line_pos_ = end;
  return line_;
}

void Lexer::skip_whitespace() noexcept {
  while (pos_ < bytes_.size() && is_whitespace(bytes_[pos_])) {
    ++pos_;
  }
}

std::optional<double> Lexer::read_double() {
  skip_whitespace();
  const auto parsed = parse_c_double(bytes_.substr(pos_));
  if (!parsed) {
    return std::nullopt;
  }
  pos_ += parsed->length;
  return parsed->value;
}

std::optional<int> Lexer::read_int() {
  skip_whitespace();
  const auto parsed = parse_c_int(bytes_.substr(pos_));
  if (!parsed) {
    return std::nullopt;
  }
  pos_ += parsed->length;
  return parsed->value;
}

BraceString Lexer::read_brace_string() {
  const auto open = bytes_.find('{', pos_);
  if (open == std::string_view::npos) {
    pos_ = bytes_.size();
    return {{}, true};
  }
  pos_ = open + 1;
  return read_brace_body();
}

BraceString Lexer::read_brace_body() {
  BraceString out;
  bool escaped = false;
  while (pos_ < bytes_.size()) {
    const char c = bytes_[pos_++];
    if (c == '\r') {
      continue;  // CRs are dropped everywhere in a brace string
    }
    if (escaped) {
      out.value.push_back(c);
      escaped = false;
    } else if (c == '\\') {
      escaped = true;
    } else if (c == '}') {
      return out;
    } else {
      out.value.push_back(c);
    }
  }
  return {{}, true};
}

void Lexer::skip_to_line_end() noexcept {
  const auto lf = bytes_.find('\n', pos_);
  pos_ = lf == std::string_view::npos ? bytes_.size() : lf;
}

}  // namespace hefesto::schem::file_detail
