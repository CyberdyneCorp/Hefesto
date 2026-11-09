// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the cursor glyph reader (capability
// display-style-files; hefesto-testing-strategy "Parser fuzzing").

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  static const auto kTable = [] {
    hefesto::display::StyleTable table;
    table.by_short_name[static_cast<unsigned char>('K')] = 7;
    table.by_short_name[static_cast<unsigned char>('W')] = 1;
    return table;
  }();
  hefesto::diag::NullSink sink;
  (void)hefesto::display::parse_glyphs(hefesto::fuzz::as_text(data, size), "f.glyphs", kTable, sink);
  return 0;
}
