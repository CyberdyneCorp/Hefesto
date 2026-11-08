// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/cmd/value.hpp"

#include <array>
#include <charconv>
#include <string_view>

namespace hefesto::cmd {

namespace {

constexpr std::string_view kListSpecials = " \t\r\n;{}\"\\$[]";

bool needs_list_quoting(std::string_view s) {
  return s.empty() || s.find_first_of(kListSpecials) != std::string_view::npos || s.front() == '#';
}

bool braces_balanced_without_backslash(std::string_view s) {
  int depth = 0;
  for (const char c : s) {
    if (c == '\\') {
      return false;
    }
    depth += c == '{' ? 1 : (c == '}' ? -1 : 0);
    if (depth < 0) {
      return false;
    }
  }
  return depth == 0;
}

std::string backslash_escaped(std::string_view s) {
  std::string out;
  for (const char c : s) {
    if (c == '\n') {
      out += "\\n";
      continue;
    }
    if (kListSpecials.find(c) != std::string_view::npos || c == '#') {
      out += '\\';
    }
    out += c;
  }
  return out;
}

// One element of a Tcl list.
std::string list_element(std::string_view s) {
  if (!needs_list_quoting(s)) {
    return std::string{s};
  }
  if (braces_balanced_without_backslash(s)) {
    return "{" + std::string{s} + "}";
  }
  return backslash_escaped(s);
}

std::string real_to_string(double v) {
  std::array<char, 32> buffer{};
  const auto [end, ec] = std::to_chars(buffer.data(), buffer.data() + buffer.size(), v);
  std::string out(buffer.data(), ec == std::errc{} ? end : buffer.data());
  if (out.find_first_of(".eEn") == std::string::npos) {
    out += ".0";  // keep reals distinguishable from integers, as Tcl does
  }
  return out;
}

struct ToString {
  std::string operator()(std::monostate) const { return {}; }
  std::string operator()(std::int64_t v) const { return std::to_string(v); }
  std::string operator()(double v) const { return real_to_string(v); }
  std::string operator()(const std::string& v) const { return v; }
  std::string operator()(const Value::List& list) const {
    std::string out;
    for (const auto& element : list) {
      if (!out.empty()) {
        out += ' ';
      }
      out += list_element(element.to_string());
    }
    return out;
  }
};

}  // namespace

std::string Value::to_string() const {
  return std::visit(ToString{}, data_);
}

}  // namespace hefesto::cmd
