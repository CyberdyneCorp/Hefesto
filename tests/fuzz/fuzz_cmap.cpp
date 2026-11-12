// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the .cmap reader (capability display-style-files;
// hefesto-testing-strategy "Parser fuzzing"). An accepted map is written
// back and the written text must read again.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>

#include "fuzz_common.hpp"
#include "hefesto/display/styles.hpp"

namespace {

/// True when some line's fourth integer (the "%*d %*d %*d %d" index the reader
/// sizes the map from) is at least `limit`. A legal map of up to 2^24 entries
/// with 99-character names needs more than libFuzzer's 2 GB under ASan, as
/// Magic's own allocation would (regression seeds regression-oom-*); such
/// inputs are skipped before the reader allocates them.
bool declares_more_entries_than(std::string_view text, long long limit) {
  std::size_t start = 0;
  while (start < text.size()) {
    const auto end = std::min(text.find('\n', start), text.size());
    const std::string line(text.substr(start, end - start));
    const char* p = line.c_str();
    long long value = 0;
    int read = 0;
    for (; read < 4; ++read) {
      char* next = nullptr;
      value = std::strtoll(p, &next, 10);
      if (next == p) {
        break;
      }
      p = next;
    }
    if (read == 4 && value >= limit) {
      return true;
    }
    start = end + 1;
  }
  return false;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace display = hefesto::display;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const bool compat = (mode & 1U) != 0;
  if (declares_more_entries_than(text, 1LL << 20)) {
    return 0;
  }
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
