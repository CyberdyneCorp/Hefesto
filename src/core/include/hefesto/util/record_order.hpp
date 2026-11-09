// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Documented deterministic record order (capability hefesto-output-determinism,
// requirement "Deterministic order where Magic's order is arbitrary"):
//   1. primary name (byte-wise, or the owning capability's comparator)
//   2. lower-left x, then y
//   3. type
// and, for two-terminal coupling records, the smaller terminal name first.
// The natural (digit-aware) comparator of cell-file-format, compare_natural,
// plugs in as a NameCompare.

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace hefesto::util {

/// Three-way name comparison: negative, zero or positive.
using NameCompare = int (*)(std::string_view, std::string_view) noexcept;

/// Unsigned byte-wise comparison (memcmp order), independent of whether
/// `char` is signed on the host.
[[nodiscard]] int compare_bytes(std::string_view a, std::string_view b) noexcept;

/// The total natural order of cell-file-format "Natural ordering of names"
/// (Q-MODEL-9, Q-CELL-12; Magic's strcmpbynum): digit runs compare by value
/// (leading zeros ignored, any length), other characters case-insensitively,
/// a proper prefix first, and names still equal compare byte-wise.
/// Shared by MODEL (port renumbering) and CELL (.mag record order).
[[nodiscard]] int compare_natural(std::string_view a, std::string_view b) noexcept;

/// The sort key of one record. Views must outlive the comparison.
struct RecordKey {
  std::string_view name;
  std::int64_t x = 0;  // lower-left
  std::int64_t y = 0;
  std::string_view type;
};

/// Strict weak order on RecordKey following the rules above.
[[nodiscard]] bool record_less(const RecordKey& a, const RecordKey& b, NameCompare names = compare_bytes) noexcept;

/// Sorts `records` stably by the deterministic order; `key_of(record)` returns
/// a RecordKey that views into the record.
template <typename T, typename KeyOf>
void sort_records(std::vector<T>& records, KeyOf key_of, NameCompare names = compare_bytes) {
  std::stable_sort(records.begin(), records.end(),
                   [&](const T& a, const T& b) { return record_less(key_of(a), key_of(b), names); });
}

/// The two terminals of a coupling record, smaller name first.
[[nodiscard]] std::pair<std::string_view, std::string_view> order_terminals(std::string_view a, std::string_view b,
                                                                            NameCompare names = compare_bytes) noexcept;

}  // namespace hefesto::util
