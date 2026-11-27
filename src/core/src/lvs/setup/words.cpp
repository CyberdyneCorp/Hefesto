// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the setup commands: keyword matching, numbers, lists
// and glob patterns.
//
// Owner: LVSSETUP.

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "hefesto/lvs/setup/codes.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "runtime.hpp"

namespace hefesto::lvs::setup {

std::optional<std::size_t> match_keyword(std::string_view word, std::span<const std::string_view> keywords) {
  std::optional<std::size_t> found;
  std::size_t count = 0;
  for (std::size_t i = 0; i < keywords.size(); ++i) {
    if (keywords[i] == word) {
      return i;
    }
    if (!word.empty() && keywords[i].starts_with(word)) {
      found = i;
      ++count;
    }
  }
  return count == 1 ? found : std::nullopt;
}

std::string bad_option(std::string_view kind, std::string_view word, std::span<const std::string_view> keywords) {
  std::string text = "bad " + std::string{kind} + " \"" + std::string{word} + "\": must be ";
  for (std::size_t i = 0; i < keywords.size(); ++i) {
    if (i > 0) {
      text += keywords.size() > 2 ? ", " : " ";
    }
    if (i + 1 == keywords.size() && i > 0) {
      text += "or ";
    }
    text += keywords[i];
  }
  return text;
}

cmd::CommandResult command_error(std::string text) {
  auto result = cmd::CommandResult::failure(diag::make(setup_codes::kCommandError, std::move(text)));
  result.raises = true;
  return result;
}

cmd::CommandResult wrong_args(std::string_view usage) {
  return command_error("wrong # args: should be \"" + std::string{usage} + "\"");
}

namespace {

std::size_t skip_blanks(std::string_view text, std::size_t i) noexcept {
  while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
    ++i;
  }
  return i;
}

/// The digits from `i` as a value; nullopt on overflow. `i` moves past them.
std::optional<std::int64_t> digits(std::string_view text, std::size_t& i) noexcept {
  constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max() / 10;
  std::int64_t value = 0;
  for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i) {
    if (value > kMax) {
      return std::nullopt;
    }
    value = value * 10 + (text[i] - '0');
  }
  return value;
}

}  // namespace

std::optional<std::int64_t> parse_integer(std::string_view text) noexcept {
  std::size_t i = skip_blanks(text, 0);
  const bool negative = i < text.size() && text[i] == '-';
  if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
    ++i;
  }
  const std::size_t first = i;
  const auto value = digits(text, i);
  if (!value || i == first || skip_blanks(text, i) != text.size()) {
    return std::nullopt;
  }
  return negative ? -*value : *value;
}

std::optional<double> parse_real(std::string_view text) noexcept {
  if (text.empty() || text.size() > 256) {
    return std::nullopt;
  }
  const std::string copy{text};
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(copy.c_str(), &end);
  if (end == copy.c_str()) {
    return std::nullopt;
  }
  while (*end == ' ' || *end == '\t') {
    ++end;
  }
  if (*end != '\0' || std::isnan(value)) {
    return std::nullopt;
  }
  return value;
}

diag::Outcome<std::vector<std::string>> split_list(std::string_view text) {
  auto list = pcell::split_tcl_list(text);
  if (!list) {
    return diag::make(setup_codes::kCommandError, list.error().text);
  }
  return std::move(list).value();
}

std::string make_list(std::span<const std::string> elements) {
  return pcell::merge_tcl_list(elements);
}

namespace {

/// Matches the bracket class at `p` (just after `[`) against `c`; moves
/// `p` past the closing bracket. False when the class is unterminated.
bool match_class(std::string_view pattern, std::size_t& p, char c, bool& matched) {
  matched = false;
  while (p < pattern.size() && pattern[p] != ']') {
    char low = pattern[p];
    char high = low;
    if (p + 2 < pattern.size() && pattern[p + 1] == '-' && pattern[p + 2] != ']') {
      high = pattern[p + 2];
      p += 2;
    }
    if (low > high) {
      std::swap(low, high);
    }
    matched = matched || (c >= low && c <= high);
    ++p;
  }
  if (p >= pattern.size()) {
    return false;
  }
  ++p;  // past ']'
  return true;
}

}  // namespace

namespace {

/// Where the pattern continues when its element at `p` (not `*`) matches
/// `c`; nullopt when it does not.
std::optional<std::size_t> step(std::string_view pattern, std::size_t p, char c) {
  const char pc = pattern[p];
  if (pc == '[') {
    std::size_t q = p + 1;
    bool matched = false;
    return match_class(pattern, q, c, matched) && matched ? std::optional{q} : std::nullopt;
  }
  if (pc == '\\' && p + 1 < pattern.size()) {
    return pattern[p + 1] == c ? std::optional{p + 2} : std::nullopt;
  }
  return pc == '?' || pc == c ? std::optional{p + 1} : std::nullopt;
}

}  // namespace

bool glob_match(std::string_view pattern, std::string_view text) noexcept {
  // Iterative matcher with one backtrack point, the last `*`.
  std::size_t p = 0;
  std::size_t t = 0;
  std::size_t star = std::string_view::npos;
  std::size_t resume = 0;
  while (t < text.size()) {
    if (p < pattern.size() && pattern[p] == '*') {
      star = p++;
      resume = t;
      continue;
    }
    const auto next = p < pattern.size() ? step(pattern, p, text[t]) : std::nullopt;
    if (next) {
      p = *next;
      ++t;
      continue;
    }
    if (star == std::string_view::npos) {
      return false;
    }
    p = star + 1;
    t = ++resume;
  }
  while (p < pattern.size() && pattern[p] == '*') {
    ++p;
  }
  return p == pattern.size();
}

}  // namespace hefesto::lvs::setup
