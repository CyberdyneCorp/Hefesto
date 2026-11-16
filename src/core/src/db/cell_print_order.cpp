// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Ported from Magic VLSI 8.3.694 (fc5daa9e) database/DBcellname.c
// (strcmpbynum, qcompare, DBCellPrint), Copyright (C) 1985, 1990 Regents of
// the University of California (permissive license, THIRD_PARTY_NOTICES.md),
// and the algorithm of glibc's merge sort (stdlib/qsort.c msort_with_tmp),
// rewritten. Owner: MODEL.

#include "hefesto/db/cell_print_order.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace hefesto::db {

namespace {

bool is_digit(char c) noexcept {
  return c >= '0' && c <= '9';
}

// tolower in the C locale, as the signed char Magic subtracts.
int lower(char c) noexcept {
  const int v = static_cast<signed char>(c);
  return v >= 'A' && v <= 'Z' ? v - 'A' + 'a' : v;
}

// strtoul over the digit run at the front of `s` (saturating at ULONG_MAX
// as strtoul does); removes the run.
std::uint64_t take_number(std::string_view& s) noexcept {
  constexpr std::uint64_t kMax = std::numeric_limits<std::uint64_t>::max();
  std::uint64_t n = 0;
  std::size_t k = 0;
  for (; k < s.size() && is_digit(s[k]); ++k) {
    const auto d = static_cast<std::uint64_t>(s[k] - '0');
    n = n > (kMax - d) / 10 ? kMax : n * 10 + d;
  }
  s.remove_prefix(k);
  return n;
}

// One step of strcmpbynum on two non-empty names; 0 means "go on".
int compare_step(std::string_view& a, std::string_view& b) noexcept {
  if (!(is_digit(a.front()) && is_digit(b.front()))) {
    const int c1 = lower(a.front());
    const int c2 = lower(b.front());
    a.remove_prefix(1);
    b.remove_prefix(1);
    return c1 - c2;
  }
  const std::uint64_t n1 = take_number(a);
  const std::uint64_t n2 = take_number(b);
  if (n1 == n2) {
    return 0;
  }
  return n1 > n2 ? 1 : -1;
}

// glibc msort_with_tmp: sorts [lo, lo + n) of `v` with `cmp(a, b) <= 0`
// taking the left element first.
template <typename Cmp>
void merge_sort(std::vector<std::string>& v, std::vector<std::string>& tmp, std::size_t lo, std::size_t n, Cmp& cmp) {
  if (n <= 1) {
    return;
  }
  const std::size_t n1 = n / 2;
  merge_sort(v, tmp, lo, n1, cmp);
  merge_sort(v, tmp, lo + n1, n - n1, cmp);
  std::size_t i = lo;
  std::size_t j = lo + n1;
  const std::size_t mid = lo + n1;
  const std::size_t end = lo + n;
  std::size_t k = 0;
  while (i < mid && j < end) {
    tmp[k++] = cmp(v[i], v[j]) <= 0 ? std::move(v[i++]) : std::move(v[j++]);
  }
  while (i < mid) {
    tmp[k++] = std::move(v[i++]);
  }
  for (std::size_t m = 0; m < k; ++m) {
    v[lo + m] = std::move(tmp[m]);
  }
}

}  // namespace

int magic_strcmpbynum(std::string_view s1, std::string_view s2) noexcept {
  for (;;) {
    if (s2.empty()) {
      return s1.empty() ? 0 : 1;
    }
    if (s1.empty()) {
      return 1;
    }
    if (const int c = compare_step(s1, s2); c != 0) {
      return c;
    }
  }
}

std::vector<std::string> magic_cell_print_order(std::vector<std::string> names) {
  std::vector<std::string> tmp(names.size());
  auto qcompare = [](const std::string& a, const std::string& b) { return -magic_strcmpbynum(a, b); };
  merge_sort(names, tmp, 0, names.size(), qcompare);
  return {names.rbegin(), names.rend()};
}

}  // namespace hefesto::db
