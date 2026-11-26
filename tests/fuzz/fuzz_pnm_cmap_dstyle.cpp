// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the colour-map and display-style files the PNM plot
// style loads (`color` and `dstyle` lines; capability plot-output "PNM
// style lines"; hefesto-testing-strategy "Parser fuzzing"; m10-plot-route
// design D20). The first byte selects the file kind.
//
// Scaffold version (m10-plot-route tasks.md 3.1): it reads the input with
// the display parsers until PLOTTECH (task 4.2) adds the plot loaders.
//
// Owner: PLOTTECH.

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  constexpr std::size_t kMaxEntries = std::size_t{1} << 16;
  const auto map = hefesto::display::parse_color_map(text, "mos.7bit.mraster.cmap", true, kMaxEntries);
  if ((mode & 1U) != 0 && map.ok()) {
    hefesto::diag::NullSink sink;
    (void)hefesto::display::parse_dstyle(text, "mos.7bit.mraster_dstyle", map.value(), true, sink, 0x7F);
  }
  return 0;
}
