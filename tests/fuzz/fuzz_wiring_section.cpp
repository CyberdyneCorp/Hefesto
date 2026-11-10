// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for the `wiring` technology section reader (capability
// wiring-tool "wiring technology section"; hefesto-testing-strategy "Parser
// fuzzing"). The first byte picks the grid scale; the rest is the body of a
// `wiring` section, split into lines and whitespace-separated tokens as the
// technology reader keeps raw sections, read against the wiring test
// technology (tests/data/wiring/wire.tech). Every contact is then scaled and
// the DRC default width of each type is asked for.
//
// Written by WIRE; tests/fuzz/ is shared (INTEGRATION, design.md D2).

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "fuzz_common.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "hefesto/wiring/wiring.hpp"

namespace {

std::shared_ptr<const hefesto::tech::Technology> technology() {
  static const auto tech = [] {
    hefesto::platform::MemoryFileSystem fs;
    hefesto::diag::NullSink sink;
    auto loaded = hefesto::tech::load_technology_text(hefesto::fuzz::source_file("tests/data/wiring/wire.tech"),
                                                      "/fuzz/wire.tech", fs, hefesto::tech::LoadOptions{}, sink);
    return loaded.ok() ? loaded.value() : hefesto::tech::builtin_technology();
  }();
  return tech;
}

std::vector<hefesto::tech::RawLine> raw_lines(std::string_view text) {
  std::vector<hefesto::tech::RawLine> lines;
  std::istringstream in{std::string{text}};
  std::string line;
  std::uint32_t number = 0;
  while (std::getline(in, line)) {
    hefesto::tech::RawLine raw{{}, "/fuzz/wire.tech", ++number};
    std::istringstream words{line};
    std::string word;
    while (words >> word) {
      raw.tokens.push_back(word);
    }
    lines.push_back(std::move(raw));
  }
  return lines;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace wiring = hefesto::wiring;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const hefesto::tech::GridScale grid{1 + (mode & 3U), 1 + ((mode >> 2U) & 3U)};
  const auto tech = technology();
  hefesto::diag::NullSink sink;
  wiring::WiringSection section = wiring::parse_wiring_lines(raw_lines(text), *tech, grid, &sink);
  section.scale_factor = (mode & 0x10U) != 0 ? 10 : section.scale_factor;
  for (const wiring::WiringContact& c : section.contacts) {
    (void)section.scaled(c);
  }
  for (std::size_t t = 0; t < tech->type_count(); ++t) {
    (void)wiring::default_layer_width(*tech, static_cast<hefesto::tech::TileType>(t), grid);
  }
  return 0;
}
