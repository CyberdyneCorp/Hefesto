// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The order of port labels in a GDS structure (capability gds-write "Label
// TEXT records"; openspec/changes/m5-mask-gds-cif/design.md D12, D22,
// Q-MASK-205 replicate). Magic's calmaOutFunc pushes each port label on the
// head of a list while it walks the cell's labels, copies the list (now in
// reverse label order) into an array and qsorts it by port index. The
// reference Magic runs on glibc, whose qsort is a stable merge sort, so
// labels with equal port index come out in reverse label-list order.
//
// Hefesto sorts with its own stable merge sort, so the order does not depend
// on the host C library (macOS qsort is not stable) and is the same on every
// leg.
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "hefesto/db/label.hpp"

namespace hefesto::gds {

namespace labels_detail {

/// Merges the sorted runs [lo, mid) and [mid, hi) of `from` into `to`,
/// taking from the right run only when it is strictly smaller.
template <typename T, typename Less>
void merge_runs(std::vector<T>& from, std::vector<T>& to, std::size_t lo, std::size_t mid, std::size_t hi, Less& less) {
  std::size_t i = lo;
  std::size_t j = mid;
  std::size_t k = lo;
  while (i < mid && j < hi) {
    to[k++] = less(from[j], from[i]) ? std::move(from[j++]) : std::move(from[i++]);
  }
  while (i < mid) {
    to[k++] = std::move(from[i++]);
  }
  while (j < hi) {
    to[k++] = std::move(from[j++]);
  }
}

}  // namespace labels_detail

/// A stable bottom-up merge sort of `items` by `less` (a strict weak
/// order): equal elements keep their relative order. O(n log n) time, n
/// scratch elements, no recursion.
template <typename T, typename Less>
void stable_merge_sort(std::vector<T>& items, Less less) {
  const std::size_t n = items.size();
  if (n < 2) {
    return;
  }
  std::vector<T> scratch(n);
  std::vector<T>* from = &items;
  std::vector<T>* to = &scratch;
  for (std::size_t width = 1; width < n; width = width > n / 2 ? n : 2 * width) {
    for (std::size_t lo = 0; lo < n; lo += std::min(2 * width, n - lo)) {
      labels_detail::merge_runs(*from, *to, lo, std::min(lo + width, n), lo + std::min(2 * width, n - lo), less);
    }
    std::swap(from, to);
  }
  if (from != &items) {
    items = std::move(*from);
  }
}

/// The indices into `labels` of the port labels (any port-direction flag),
/// in the order calmaOutFunc writes them: the reversed label list sorted
/// ascending by port index (unsigned), stably.
[[nodiscard]] std::vector<std::size_t> port_label_order(std::span<const db::Label> labels);

}  // namespace hefesto::gds
