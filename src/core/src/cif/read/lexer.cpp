// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The CIF lexer (Magic's cif/CIFrdutils.c:348-560, 1394-1460). Owner: CIF.

#include "hefesto/cif/read/lexer.hpp"

namespace hefesto::cif {

namespace {

bool is_digit(int ch) noexcept {
  return ch >= '0' && ch <= '9';
}

bool is_upper(int ch) noexcept {
  return ch >= 'A' && ch <= 'Z';
}

bool is_space(int ch) noexcept {
  return ch == ' ' || (ch >= '\t' && ch <= '\r');
}

}  // namespace

int Lexer::take() noexcept {
  if (pos_ >= text_.size()) {
    return kEof;
  }
  return static_cast<unsigned char>(text_[pos_++]);
}

bool Lexer::is_blank(int ch) noexcept {
  return !(is_digit(ch) || is_upper(ch) || ch == '-' || ch == ';' || ch == '(' || ch == ')' || ch == kEof);
}

void Lexer::skip_blanks() noexcept {
  while (is_blank(peek())) {
    if (take() == '\n') {
      ++line_;
    }
  }
}

void Lexer::skip_separators() noexcept {
  for (int ch = peek(); is_upper(ch) || is_blank(ch); ch = peek()) {
    if (take() == '\n') {
      ++line_;
    }
  }
}

void Lexer::skip_to_semicolon() noexcept {
  for (int ch = peek(); ch != ';' && ch != kEof; ch = peek()) {
    if (take() == '\n') {
      ++line_;
    }
  }
}

bool Lexer::skip_semicolon() noexcept {
  skip_blanks();
  if (peek() != ';') {
    return false;
  }
  take();
  skip_blanks();
  return true;
}

bool Lexer::skip_comment() noexcept {
  take();  // the '('
  std::int64_t opens = 1;
  while (opens > 0) {
    const int ch = take();
    if (ch == '(') {
      ++opens;
    } else if (ch == ')') {
      --opens;
    } else if (ch == '\n') {
      ++line_;
    } else if (ch == kEof) {
      return false;
    }
  }
  return true;
}

std::optional<std::int64_t> Lexer::signed_integer() noexcept {
  skip_separators();
  const bool negative = peek() == '-';
  if (negative) {
    take();
  }
  if (!is_digit(peek())) {
    return std::nullopt;
  }
  std::int64_t value = 0;
  while (is_digit(peek())) {
    const int digit = take() - '0';
    value = value > (kIntegerLimit - digit) / 10 ? kIntegerLimit : value * 10 + digit;
  }
  return negative ? -value : value;
}

std::string Lexer::name() {
  while (peek() == ' ' || peek() == '\t') {
    take();
  }
  std::string out;
  for (int ch = peek(); ch != kEof && !is_space(ch) && ch != ';'; ch = peek()) {
    out.push_back(static_cast<char>(take()));
  }
  return out;
}

std::string Lexer::layer_name() {
  skip_blanks();
  std::string out;
  constexpr std::size_t kMaxChars = 5;  // MAXCHARS + 1 in CIFParseLayer
  while (out.size() < kMaxChars && (is_digit(peek()) || is_upper(peek()))) {
    out.push_back(static_cast<char>(take()));
  }
  return out;
}

}  // namespace hefesto::cif
