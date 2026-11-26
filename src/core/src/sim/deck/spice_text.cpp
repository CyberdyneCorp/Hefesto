// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Owner: SIMDECK. See spice_text.hpp.

#include "hefesto/sim/deck/spice_text.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <locale>
#include <sstream>
#include <utility>

namespace hefesto::sim {

namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }

std::size_t skip_digits(std::string_view text, std::size_t i) {
  while (i < text.size() && is_digit(text[i])) {
    ++i;
  }
  return i;
}

// The end of an exponent `e[+-]digits` at i, or i when there is none.
std::size_t exponent_end(std::string_view text, std::size_t i) {
  if (i >= text.size() || (text[i] != 'e' && text[i] != 'E')) {
    return i;
  }
  std::size_t j = i + 1;
  if (j < text.size() && (text[j] == '+' || text[j] == '-')) {
    ++j;
  }
  return j < text.size() && is_digit(text[j]) ? skip_digits(text, j) : i;
}

// The length of the decimal number at the start of `text` (0: none).
std::size_t number_length(std::string_view text) {
  std::size_t i = !text.empty() && (text[0] == '+' || text[0] == '-') ? 1 : 0;
  const std::size_t integer_end = skip_digits(text, i);
  std::size_t end = integer_end;
  if (end < text.size() && text[end] == '.') {
    end = skip_digits(text, end + 1);
  }
  const bool has_digits = integer_end > i || end > integer_end + 1;
  return has_digits ? exponent_end(text, end) : 0;
}

double scale_of(std::string_view suffix) {
  const std::string s = ascii_lower(suffix);
  if (s.starts_with("meg")) {
    return 1e6;
  }
  if (s.starts_with("mil")) {
    return 25.4e-6;
  }
  static constexpr std::array<std::pair<char, double>, 8> kScales = {
      {{'t', 1e12}, {'g', 1e9}, {'k', 1e3}, {'m', 1e-3}, {'u', 1e-6}, {'n', 1e-9}, {'p', 1e-12}, {'f', 1e-15}}};
  for (const auto& [letter, scale] : kScales) {
    if (!s.empty() && s.front() == letter) {
      return scale;
    }
  }
  return 1.0;
}

}  // namespace

std::string ascii_lower(std::string_view text) {
  std::string out(text);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return out;
}

std::optional<double> parse_spice_number(std::string_view text) {
  const std::size_t n = number_length(text);
  if (n == 0) {
    return std::nullopt;
  }
  std::istringstream in{std::string{text.substr(0, n)}};
  in.imbue(std::locale::classic());
  double value = 0.0;
  in >> value;
  if (in.fail()) {
    return std::nullopt;
  }
  return value * scale_of(text.substr(n));
}

std::vector<std::string> spice_tokens(std::string_view line) {
  std::vector<std::string> tokens;
  std::string current;
  for (const char c : line) {
    const bool separator = std::isspace(static_cast<unsigned char>(c)) != 0 || c == '(' || c == ')' || c == ',' ||
                           c == '=';
    if (!separator) {
      current.push_back(c);
    } else if (!current.empty()) {
      tokens.push_back(std::move(current));
      current.clear();
    }
  }
  if (!current.empty()) {
    tokens.push_back(std::move(current));
  }
  return tokens;
}

std::string format_g12(double value) {
  std::array<char, 64> buffer{};
  const int n = std::snprintf(buffer.data(), buffer.size(), "%.12g", value);
  return std::string(buffer.data(), static_cast<std::size_t>(n > 0 ? n : 0));
}

}  // namespace hefesto::sim
