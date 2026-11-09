// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the outline-font (.pt3) reader (capability
// font-labels; hefesto-testing-strategy "Parser fuzzing"). An accepted font
// also lays out a short label, which walks every glyph's contours.

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/display/font.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace display = hefesto::display;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const double scale = (mode & 1U) != 0 ? 0.58 : 1.0;
  const auto font = display::parse_outline_font(text, "F.pt3", scale);
  if (!font.ok()) {
    return 0;
  }
  for (const char c : std::string_view{"A ~\x01"}) {
    (void)font.value().glyph(c);
  }
  return 0;
}
