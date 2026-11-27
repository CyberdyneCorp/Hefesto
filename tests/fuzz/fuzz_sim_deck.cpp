// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for deck preparation (capability
// simulation-netlist-preparation "Deck validation and sanitisation", "Raw
// decks", "Deck limits", "Deck delivery"; openspec/changes/
// m9-lvs-simulation design D11, D18, D20). The input is split at its first
// NUL byte: the part before is a raw deck, the part after is the file
// `/p/a.inc`, which the deck (and the file itself) may include. Lowered
// limits keep include loops and large decks cheap. A prepared deck must
// hold no `.control` section unless allowed, and no line longer than the
// delivery limit.
//
// Owner: SIMDECK; tests/fuzz/ is shared (INTEGRATION).

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/sim/deck/deck.hpp"

namespace {

namespace sim = hefesto::sim;

std::vector<std::string> split_lines(std::string_view text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = text.find('\n', start);
    lines.emplace_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return lines;
}

bool lower_equal(std::string_view line, std::string_view word) {
  const std::size_t at = line.find_first_not_of(" \t");
  if (at == std::string_view::npos || line.size() - at < word.size()) {
    return false;
  }
  for (std::size_t i = 0; i < word.size(); ++i) {
    const char c = line[at + i];
    if ((c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c) != word[i]) {
      return false;
    }
  }
  const std::size_t after = at + word.size();
  return after == line.size() || line[after] == ' ' || line[after] == '\t' || line[after] == '\r';
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string_view input{reinterpret_cast<const char*>(data), size};  // NOLINT
  const std::size_t nul = input.find('\0');
  hefesto::platform::MemoryFileSystem files;
  (void)files.create_directories("/p");
  if (nul != std::string_view::npos) {
    (void)files.write_atomic("/p/a.inc", input.substr(nul + 1));
  }
  sim::DeckContext context;
  context.files = &files;
  context.base_dir = "/p";
  context.limits = sim::DeckLimits{std::size_t{1} << 18, 8};
  sim::Testbench tb;
  tb.dut.kind = sim::CircuitSource::Kind::raw;
  tb.lines = split_lines(input.substr(0, nul));
  for (const bool allow : {false, true}) {
    tb.allow_control = allow;
    const auto deck = sim::prepare(tb, context);
    if (!deck.ok()) {
      continue;
    }
    const std::vector<std::string>& lines = deck.value().lines;
    for (std::size_t i = 0; i < lines.size(); ++i) {
      // Line 0 is the deck's title, which ngspice never parses: a title
      // reading `.control` starts no control section.
      if (lines[i].size() > 65535 || (!allow && i > 0 && lower_equal(lines[i], ".control"))) {
        __builtin_trap();
      }
    }
  }
  return 0;
}
