// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Test helpers of the merged-polygon and label tests (tasks.md 10.4, 10.5):
// hex bytes, a record writer over memory, mask planes painted from
// rectangles, and polygon areas. Owner: GDSW.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/gds/stream/io.hpp"
#include "hefesto/gds/stream/records.hpp"
#include "hefesto/mask/plane/plane.hpp"

namespace gdsw_test {

using Bytes = std::vector<std::byte>;

/// Bytes from hex text; every non-hex character is ignored.
inline Bytes hex(std::string_view text) {
  Bytes out;
  int high = -1;
  for (const char c : text) {
    int v = -1;
    if (c >= '0' && c <= '9') {
      v = c - '0';
    } else if (c >= 'a' && c <= 'f') {
      v = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
      v = c - 'A' + 10;
    }
    if (v < 0) {
      continue;
    }
    if (high < 0) {
      high = v;
    } else {
      out.push_back(static_cast<std::byte>(high * 16 + v));
      high = -1;
    }
  }
  return out;
}

/// A RecordWriter over memory; bytes() flushes first.
class Output {
 public:
  Output() : writer_(sink_) {}
  [[nodiscard]] hefesto::gds::RecordWriter& writer() noexcept { return writer_; }
  [[nodiscard]] Bytes bytes() {
    REQUIRE(writer_.flush().ok());
    return sink_.bytes();
  }

 private:
  hefesto::gds::MemorySink sink_;
  hefesto::gds::RecordWriter writer_;
};

/// A mask plane with `rects` painted in order.
inline void paint(hefesto::mask::MaskPlane& plane, std::initializer_list<hefesto::db::Rect> rects) {
  for (const hefesto::db::Rect& r : rects) {
    REQUIRE(plane.paint(r).ok());
  }
}

/// Twice the signed shoelace area of a closed polygon (64-bit).
inline std::int64_t twice_area(const std::vector<hefesto::db::Point>& polygon) {
  std::int64_t sum = 0;
  for (std::size_t i = 0; i < polygon.size(); ++i) {
    const hefesto::db::Point a = polygon[i];
    const hefesto::db::Point b = polygon[(i + 1) % polygon.size()];
    sum += std::int64_t{a.x} * b.y - std::int64_t{b.x} * a.y;
  }
  return sum;
}

}  // namespace gdsw_test
