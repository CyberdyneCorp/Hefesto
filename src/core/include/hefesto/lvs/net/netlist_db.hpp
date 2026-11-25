// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs::NetlistDb: the netlist database the LVS readers fill and the
// comparison reads (openspec/changes/m9-lvs-simulation design.md D6, "Area
// contracts" LVSREAD; capability lvs-netlist-input "Netlist database model",
// "Device classes"). The scaffold declares only the identities; LVSREAD adds
// devices, nets, pins, properties and the interned classes.
//
// Owner: LVSREAD.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hefesto::lvs {

/// Index of a cell in NetlistDb::cells.
using CellId = std::uint32_t;

/// Devices per run (design D18, Q-LVS-36): a setting may lower it and never
/// raise it past twice this value.
inline constexpr std::uint64_t kMaxDevicesPerRun = 50'000'000;

struct Cell {
  std::string name;
};

struct NetlistDb {
  std::vector<Cell> cells;
};

}  // namespace hefesto::lvs
