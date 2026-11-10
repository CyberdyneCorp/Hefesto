// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the .cmap reader (capability display-style-files;
// hefesto-testing-strategy "Parser fuzzing"). An accepted map is written
// back and the written text must read again.

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include "fuzz_common.hpp"
#include "hefesto/display/styles.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace display = hefesto::display;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const bool compat = (mode & 1U) != 0;
  const auto map = display::parse_color_map(text, "f.cmap", compat);
  // The write-back round trip is limited to realistic maps: a legal map of
  // 2^24 entries written and read again exceeds libFuzzer's 2 GB limit under
  // ASan (regression seed corpus/fuzz_cmap/regression-oom-large-index).
  constexpr std::size_t kRoundTripEntries = std::size_t{1} << 16;
  if (!map.ok() || map.value().entries.size() > kRoundTripEntries) {
    return 0;
  }
  const auto again = display::parse_color_map(display::write_color_map(map.value()), "f.cmap", compat);
  if (!again.ok() || again.value().entries.size() != map.value().entries.size()) {
    std::abort();
  }
  return 0;
}
