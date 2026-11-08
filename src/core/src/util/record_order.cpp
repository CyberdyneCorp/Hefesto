// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/util/record_order.hpp"

#include <cstring>

namespace hefesto::util {

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
