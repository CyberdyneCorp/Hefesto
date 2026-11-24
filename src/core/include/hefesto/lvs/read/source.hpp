// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The input of a netlist or setup-file reader (openspec/changes/
// m9-lvs-simulation design.md D6, "Area contracts" LVSREAD; capability
// lvs-netlist-input "Format selection", "In-memory netlist sources").
// Layout and schematic netlists produced in memory go through the same
// readers as files.
//
// Owner: LVSREAD.

#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace hefesto::lvs {

/// The netlist formats of lvs-netlist-input.
enum class Format : std::uint8_t { spice, cdl, verilog, sim, ext };

/// One reader input. With `bytes` set the text is read from memory and
/// `name` only labels diagnostics and the report; otherwise `name` is a path
/// read through ReadOptions::files.
struct Source {
  std::string name;
  std::optional<std::string> bytes;
};

}  // namespace hefesto::lvs
