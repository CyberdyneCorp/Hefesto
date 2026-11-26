// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the colour-map and display-style files the PNM plot
// style loads with its `color` and `dstyle` lines (capability plot-output
// "PNM style lines"; hefesto-testing-strategy "Parser fuzzing";
// m10-plot-route design D20). The first byte selects the file kind and the
// style limit; a colour map read from the input is then used to resolve
// the dstyle colours, as PlotLoadStyles does after PlotLoadColormap.
//
// Owner: PLOTTECH.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "fuzz_common.hpp"
#include "plot/tech/reader.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace detail = hefesto::plot::detail;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const detail::ColormapParse map = detail::parse_pnm_colormap(text);
  if (map.colors.size() > 128) {
    __builtin_trap();  // PlotLoadColormap's 128-entry table
  }
  if ((mode & 1U) != 0) {
    const std::vector<hefesto::plot::Rgb> display(mode >> 1U, hefesto::plot::Rgb{1, 2, 3});
    const std::size_t limit = (mode >> 4U) & 0x0FU;
    const detail::DstyleParse styles = detail::parse_pnm_dstyles(text, map.colors, display, limit);
    if (styles.styles.size() > limit) {
      __builtin_trap();  // never more than DBWNumStyles
    }
  }
  return 0;
}
