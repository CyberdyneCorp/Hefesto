// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// libFuzzer target for readspice's netlist reader (capability readspice
// "Line preprocessing", "Subcircuit recognition"; hefesto-testing-strategy
// "Parser fuzzing"). The first byte picks the continuation mode
// (Q-SCRIPT-32) and CDL mode; the rest is the netlist. Every logical line
// is tokenized as readspice does, including the PININFO pairs.
//
// Written by TOOLS; tests/fuzz/ is shared (INTEGRATION, design.md D2).

#include <cstddef>
#include <cstdint>

#include "fuzz_common.hpp"
#include "hefesto/tools/auxiliary.hpp"
#include "hefesto/tools/readspice.hpp"
#include "hefesto/tools/spice_lines.hpp"

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  namespace tools = hefesto::tools;
  const auto [mode, text] = hefesto::fuzz::split_mode(hefesto::fuzz::as_text(data, size));
  const auto continuation = (mode & 1U) != 0 ? tools::ContinuationMode::fixed : tools::ContinuationMode::magic;
  tools::SpiceLineReader reader{text, continuation, (mode & 2U) != 0};
  while (const auto line = reader.next()) {
    const tools::SpiceLine parsed = tools::tokenize_spice_line(*line);
    if (parsed.keyword == ".pininfo") {
      for (const auto& token : parsed.tokens) {
        (void)tools::tcl_split(token, ":");
      }
    }
    if (!parsed.tokens.empty()) {
      (void)tools::tcl_file_rootname(parsed.tokens.front());
    }
  }
  (void)tools::strip_reflib_lines(text);
  return 0;
}
