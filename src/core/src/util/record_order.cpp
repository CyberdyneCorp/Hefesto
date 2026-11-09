// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// compare_natural is ported from Magic VLSI 8.3.694 database/DBcellname.c
// (strcmpbynum, made a total order per Q-CELL-12), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).

#include "hefesto/util/record_order.hpp"

#include <cctype>
#include <cstring>

namespace hefesto::util {

namespace {

int lower(char c) noexcept {
  return std::tolower(static_cast<unsigned char>(c));
}

bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

// Compares the digit runs at the heads of a and b numerically (leading
// zeros ignored, arbitrary length); advances both past their runs.
int compare_digit_runs(std::string_view& a, std::string_view& b) noexcept {
  const auto take = [](std::string_view& s) {
    std::size_t n = 0;
    while (n < s.size() && is_digit(s[n])) {
      ++n;
    }
    std::string_view run = s.substr(0, n);
    s.remove_prefix(n);
    while (run.size() > 1 && run.front() == '0') {
      run.remove_prefix(1);
    }
    return run;
  };
  const std::string_view ra = take(a);
  const std::string_view rb = take(b);
  if (ra.size() != rb.size()) {
    return ra.size() < rb.size() ? -1 : 1;
  }
  const int c = ra.compare(rb);
  return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

// One step of Magic's strcmpbynum loop: compares and consumes a digit run
// pair or one character pair; nonzero when they differ.
int natural_step(std::string_view& x, std::string_view& y) noexcept {
  if (is_digit(x.front()) && is_digit(y.front())) {
    return compare_digit_runs(x, y);
  }
  const int d = lower(x.front()) - lower(y.front());
  x.remove_prefix(1);
  y.remove_prefix(1);
  return d < 0 ? -1 : (d > 0 ? 1 : 0);
}

}  // namespace

// Moved from db/labels.cpp (MODEL) so CELL and MODEL share one definition.
int compare_natural(std::string_view a, std::string_view b) noexcept {
  std::string_view x = a;
  std::string_view y = b;
  while (!x.empty() && !y.empty()) {
    if (const int c = natural_step(x, y); c != 0) {
      return c;
    }
  }
  if (x.empty() != y.empty()) {
    return x.empty() ? -1 : 1;  // the natural prefix sorts first
  }
  return compare_bytes(a, b);
}

int compare_bytes(std::string_view a, std::string_view b) noexcept {
  const std::size_t common = std::min(a.size(), b.size());
  if (common > 0) {
    const int c = std::memcmp(a.data(), b.data(), common);
    if (c != 0) {
      return c < 0 ? -1 : 1;
    }
  }
  if (a.size() == b.size()) {
    return 0;
  }
  return a.size() < b.size() ? -1 : 1;
}

bool record_less(const RecordKey& a, const RecordKey& b, NameCompare names) noexcept {
  if (const int c = names(a.name, b.name); c != 0) {
    return c < 0;
  }
  if (a.x != b.x) {
    return a.x < b.x;
  }
  if (a.y != b.y) {
    return a.y < b.y;
  }
  return compare_bytes(a.type, b.type) < 0;
}

std::pair<std::string_view, std::string_view> order_terminals(std::string_view a, std::string_view b,
                                                              NameCompare names) noexcept {
  if (names(b, a) < 0) {
    return {b, a};
  }
  return {a, b};
}

}  // namespace hefesto::util
