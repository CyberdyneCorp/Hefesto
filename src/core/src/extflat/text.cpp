// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Text helpers of the `.ext` reader (see text.hpp).
// Owner: EXTSUB (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D10).

#include "text.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace hefesto::extflat::detail {

namespace {

// keyTable of extflat/extparse.h, in its order (LookupStruct scans it in
// order, so the order decides nothing but ambiguity, which is symmetric).
constexpr std::array<KeyInfo, 26> kKeys{{
    {"abstract", Key::abstract_, 0},
    {"adjust", Key::adjust, 4},
    {"attr", Key::attr, 8},
    {"cap", Key::cap, 4},
    {"connect", Key::connect, 7},
    {"device", Key::device, 11},
    {"distance", Key::dist, 4},
    {"equiv", Key::equiv, 3},
    {"fet", Key::fet, 12},
    {"killnode", Key::killnode, 2},
    {"merge", Key::merge, 3},
    {"node", Key::node, 7},
    {"parameters", Key::parameters, 3},
    {"port", Key::port, 8},
    {"primitive", Key::primitive, 0},
    {"resist", Key::resistor, 4},
    {"resistclasses", Key::resistclass, 1},
    {"rnode", Key::rnode, 5},
    {"scale", Key::scale, 4},
    {"subcap", Key::subcap, 3},
    {"substrate", Key::substrate, 3},
    {"tech", Key::tech, 2},
    {"timestamp", Key::timestamp, 2},
    {"use", Key::use, 9},
    {"version", Key::version, 2},
    {"style", Key::style, 2},
}};

constexpr char ascii_lower(char c) noexcept {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// The length of the case-insensitive common prefix (the table holds lower
// case names only, so LookupStruct's case rule reduces to this).
std::size_t common_prefix(std::string_view word, std::string_view name) noexcept {
  std::size_t i = 0;
  while (i < word.size() && i < name.size() && ascii_lower(word[i]) == name[i]) {
    ++i;
  }
  return i;
}

struct Digits {
  bool negative = false;
  std::size_t begin = 0;  // first digit
  std::size_t end = 0;    // one past the last digit
};

// White space, sign and the digit run of strtol (base 10).
Digits scan_digits(std::string_view text) noexcept {
  Digits d;
  std::size_t i = 0;
  while (i < text.size() && c_isspace(text[i])) {
    ++i;
  }
  if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
    d.negative = text[i] == '-';
    ++i;
  }
  d.begin = i;
  while (i < text.size() && text[i] >= '0' && text[i] <= '9') {
    ++i;
  }
  d.end = i;
  return d;
}

// strtol's value of the digit run, saturating at the 64-bit range.
std::int64_t saturating_value(std::string_view text, const Digits& d) noexcept {
  constexpr std::uint64_t kLimit = std::uint64_t{1} << 63U;
  std::uint64_t magnitude = 0;
  for (std::size_t i = d.begin; i < d.end; ++i) {
    const auto digit = static_cast<std::uint64_t>(text[i] - '0');
    if (magnitude > (kLimit - digit) / 10) {
      magnitude = kLimit;
      break;
    }
    magnitude = magnitude * 10 + digit;
  }
  if (d.negative) {
    return magnitude >= kLimit ? std::numeric_limits<std::int64_t>::min() : -static_cast<std::int64_t>(magnitude);
  }
  return magnitude >= kLimit ? std::numeric_limits<std::int64_t>::max() : static_cast<std::int64_t>(magnitude);
}

}  // namespace

std::optional<KeyInfo> lookup_key(std::string_view word) noexcept {
  std::optional<KeyInfo> match;
  bool ambiguous = false;
  for (const KeyInfo& info : kKeys) {
    if (common_prefix(word, info.name) != word.size()) {
      continue;
    }
    if (word.size() == info.name.size()) {
      return info;  // exact match
    }
    ambiguous = ambiguous || match.has_value();
    match = info;
  }
  if (ambiguous) {
    return std::nullopt;
  }
  return match;
}

std::int64_t c_atol(std::string_view text) noexcept {
  const Digits d = scan_digits(text);
  return saturating_value(text, d);
}

std::int32_t c_atoi(std::string_view text) noexcept {
  // glibc: (int) strtol(...), the low 32 bits of the saturated long.
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(c_atol(text))));
}

double c_atof(std::string_view text) noexcept {
  const std::string copy{text};
  return std::strtod(copy.c_str(), nullptr);
}

std::optional<std::int32_t> scan_int(std::string_view text, std::size_t* consumed) noexcept {
  const Digits d = scan_digits(text);
  if (d.begin == d.end) {
    return std::nullopt;
  }
  if (consumed != nullptr) {
    *consumed = d.end;
  }
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(saturating_value(text, d))));
}

std::int32_t round_half_up_trunc(double v) noexcept {
  const double r = 0.5 + v;
  if (std::isnan(r)) {
    return 0;
  }
  if (r >= 2147483647.0) {
    return std::numeric_limits<std::int32_t>::max();
  }
  if (r <= -2147483648.0) {
    return std::numeric_limits<std::int32_t>::min();
  }
  return static_cast<std::int32_t>(r);
}

std::string_view LineReader::physical_line() {
  const std::size_t start = pos_;
  std::size_t end = text_.find('\n', start);
  if (end == std::string_view::npos) {
    end = text_.size();
    pos_ = end;
  } else {
    pos_ = end + 1;
  }
  std::string_view content = text_.substr(start, end - start);
  // fgets stores a NUL byte, and efReadLine stops at it.
  if (const std::size_t nul = content.find('\0'); nul != std::string_view::npos) {
    content = content.substr(0, nul);
  }
  ++line_number_;
  return content;
}

std::optional<std::string> LineReader::logical_line() {
  std::string line;
  too_long_ = false;
  for (;;) {
    if (pos_ >= text_.size()) {
      return std::nullopt;  // fgets() == NULL, even inside a continuation
    }
    const std::string_view content = physical_line();
    if (!too_long_ && line.size() + content.size() > max_line_bytes_) {
      too_long_ = true;
      line.clear();
    }
    if (!too_long_) {
      line.append(content);
    }
    if (content.empty() || content.back() != '\\') {
      return line;
    }
    if (!too_long_) {
      line.pop_back();  // the backslash is overwritten by the next line
    }
  }
}

std::optional<Line> LineReader::next() {
  for (;;) {
    std::optional<std::string> logical = logical_line();
    if (!logical) {
      return std::nullopt;
    }
    if (too_long_) {
      return Line{{}, line_number_, true};
    }
    if (!logical->empty() && logical->front() == '#') {
      continue;
    }
    std::vector<std::string> tokens = tokenize(*logical);
    if (!tokens.empty()) {
      return Line{std::move(tokens), line_number_, false};
    }
  }
}

namespace {

// The characters of one token from `get` (efReadLine's inner loop); `get`
// is left on the terminating character.
std::string scan_token(std::string_view line, std::size_t& get) {
  const std::size_t n = line.size();
  std::string token;
  bool inquote = false;
  for (; get < n; ++get) {
    const char c = line[get];
    if (c == '"') {
      inquote = !inquote;
      continue;
    }
    if (!inquote && c_isspace(c)) {
      break;
    }
    if (c == '\\' && get + 1 == n) {
      ++get;  // a backslash at the end of the line is dropped
      break;
    }
    token.push_back(c);
  }
  return token;
}

}  // namespace

std::vector<std::string> tokenize(std::string_view line) {
  std::vector<std::string> tokens;
  std::size_t get = 0;
  const std::size_t n = line.size();
  while (get < n) {
    while (get < n && c_isspace(line[get])) {
      ++get;
    }
    const std::size_t start = get;
    std::string token = scan_token(line, get);
    if (get == start) {
      break;
    }
    if (get < n) {
      ++get;  // the terminating character
    }
    tokens.push_back(std::move(token));
  }
  return tokens;
}

}  // namespace hefesto::extflat::detail
