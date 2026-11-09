// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the .dstyle reader (capability display-style-files;
// hefesto-testing-strategy "Parser fuzzing"). The first byte selects
// magic-compat and the default format version.

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"

namespace {

const hefesto::display::ColorMap& gray_map() {
  static const auto kMap = [] {
    hefesto::display::ColorMap map;
    map.entries.resize(256);
    for (std::size_t i = 0; i < map.entries.size(); ++i) {
      const auto v = static_cast<std::uint8_t>(i);
      map.entries[i] = hefesto::display::ColorEntry{v, v, v, i == 7 ? "gray7" : ""};
    }
    return map;
  }();
  return kMap;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  hefesto::diag::NullSink sink;
  const bool compat = (mode & 1U) != 0;
  const int version = (mode & 2U) != 0 ? 7 : 5;
  (void)hefesto::display::parse_dstyle(text, "f.OpenGL.dstyle", gray_map(), compat, sink, 0xFFFFFF, version);
  return 0;
}
