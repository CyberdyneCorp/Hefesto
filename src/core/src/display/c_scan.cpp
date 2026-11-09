// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "display/c_scan.hpp"

#include <cstdlib>
#include <limits>

namespace hefesto::display::cscan {

namespace {

int digit_value(char c) noexcept {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return 99;
}

bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

}  // namespace

std::vector<std::string_view> fgets_chunks(std::string_view text, std::size_t size) {
  std::vector<std::string_view> chunks;
  const std::size_t limit = size > 1 ? size - 1 : 1;
  std::size_t pos = 0;
  while (pos < text.size()) {
    std::size_t end = pos;
    while (end < text.size() && end - pos < limit) {
      ++end;
      if (text[end - 1] == '\n') {
        break;
      }
    }
    chunks.push_back(text.substr(pos, end - pos));
    pos = end;
  }
  return chunks;
}

bool is_white(std::string_view line, bool comment_ok) noexcept {
  if (comment_ok && !line.empty() && line.front() == '#') {
    return true;
  }
  for (const char c : line) {
    if (!is_space(c)) {
      return false;
    }
  }
  return true;
}

bool is_int(std::string_view s) noexcept {
  if (!s.empty() && (s.front() == '-' || s.front() == '+')) {
    s.remove_prefix(1);
  }
  for (const char c : s) {
    if (!is_digit(c)) {
      return false;
    }
  }
  return true;
}

std::int64_t atoi(std::string_view s) noexcept {
  Scanner scan(s);
  return scan.integer(10).value_or(0);
}

std::optional<double> leading_number(std::string_view s) noexcept {
  const std::string copy(s);
  char* end = nullptr;
  const double value = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str()) {
    return std::nullopt;
  }
  return value;
}

void Scanner::skip_space() noexcept {
  while (pos_ < input_.size() && is_space(input_[pos_])) {
    ++pos_;
  }
}

std::optional<std::int64_t> Scanner::integer(int base) noexcept {
  skip_space();
  std::size_t p = pos_;
  bool negative = false;
  if (p < input_.size() && (input_[p] == '-' || input_[p] == '+')) {
    negative = input_[p] == '-';
    ++p;
  }
  const bool zero_prefix = p < input_.size() && input_[p] == '0';
  const bool x_follows = zero_prefix && p + 1 < input_.size() && (input_[p + 1] == 'x' || input_[p + 1] == 'X');
  const bool hex_digit_after_x = x_follows && p + 2 < input_.size() && digit_value(input_[p + 2]) < 16;
  if (base == 0) {
    base = hex_digit_after_x ? 16 : (zero_prefix ? 8 : 10);
  }
  if (base == 16 && hex_digit_after_x) {
    p += 2;
  }
  std::int64_t value = 0;
  const std::size_t first = p;
  constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
  while (p < input_.size() && digit_value(input_[p]) < base) {
    const std::int64_t digit = digit_value(input_[p]);
    // Saturate like strtol instead of overflowing (found by fuzz_cmap).
    value = value > (kMax - digit) / base ? kMax : value * base + digit;
    ++p;
  }
  if (p == first) {
    return std::nullopt;
  }
  pos_ = p;
  return negative ? -value : value;
}

std::optional<std::string> Scanner::word(std::size_t width) {
  skip_space();
  const std::size_t first = pos_;
  while (pos_ < input_.size() && !is_space(input_[pos_]) && pos_ - first < width) {
    ++pos_;
  }
  if (pos_ == first) {
    return std::nullopt;
  }
  return std::string(input_.substr(first, pos_ - first));
}

std::optional<char> Scanner::character() noexcept {
  if (pos_ >= input_.size()) {
    return std::nullopt;
  }
  return input_[pos_++];
}

std::optional<std::string> Scanner::until_newline(std::size_t width) {
  const std::size_t first = pos_;
  while (pos_ < input_.size() && input_[pos_] != '\n' && pos_ - first < width) {
    ++pos_;
  }
  if (pos_ == first) {
    return std::nullopt;
  }
  return std::string(input_.substr(first, pos_ - first));
}

bool Scanner::literal(std::string_view text) noexcept {
  if (input_.substr(pos_, text.size()) != text) {
    return false;
  }
  pos_ += text.size();
  return true;
}

}  // namespace hefesto::display::cscan
