// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Bus and vector notation (openspec/changes/m8-schematic design.md "Area
// contracts" NETLIST; capability schematic-netlisting "Bus and vector
// notation").
//
// Owner: NETLIST.

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace hefesto::schem {

/// The bit names of a net or instance name in bus notation, in order; a
/// plain name gives itself.
[[nodiscard]] std::vector<std::string> expand_bus(std::string_view name);

}  // namespace hefesto::schem
