// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private state shared by the .mag reader translation units
// (mag_reader.cpp, mag_reader_use.cpp, mag_reader_records.cpp,
// mag_elements.cpp).

#pragma once

#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <string_view>

#include "hefesto/cell/mag_io.hpp"
#include "mag_input.hpp"

namespace hefesto::cell::detail {

struct ReadState {
  ReadState(CellIoContext& io, db::CellDef& target, std::string_view text, const ReadOptions& read_options)
      : ctx(io), def(target), in(text), options(read_options) {}

  CellIoContext& ctx;
  db::CellDef& def;
  MagInput in;
  ReadOptions options;
  std::int64_t n = 1;  // read scale numerator ("Coordinate scaling on read")
  std::int64_t d = 1;  // read scale denominator
  std::set<std::string, std::less<>> seen_uses;  // dbUseTable: names used so far
  std::string line;

  [[nodiscard]] const tech::Technology& tech() const { return *ctx.tech; }
  void emit(std::string_view code, std::string text) const { ctx.sink.emit(diag::make(code, std::move(text))); }
  /// v * n (when n > 1), then / d (when d > 1), truncating toward zero.
  /// The product saturates: v comes from the file and may be huge (fuzz_mag_reader).
  [[nodiscard]] std::int32_t scale(std::int64_t v) const noexcept {
    if (n > 1) {
      constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
      constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
      v = v > kMax / n ? kMax : (v < kMin / n ? kMin : v * n);
    }
    if (d > 1) {
      v /= d;
    }
    return static_cast<std::int32_t>(v);
  }
};

/// dbReadUse: `st.line` holds the use line; on return the next line.
/// False on a format error or end of file.
bool read_use(ReadState& st);
/// dbReadLabels / dbReadElements / dbReadProperties: `st.line` holds the
/// section header; on return the line that ended the section. False on
/// end of file where a line is required.
bool read_labels(ReadState& st);
bool read_elements(ReadState& st);
bool read_properties(ReadState& st);

}  // namespace hefesto::cell::detail
