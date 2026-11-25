// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs::Circuit: one side of an LVS run (openspec/changes/m9-lvs-simulation
// design.md D4, D16, "Area contracts" LVSHIER and LVSCMD). A side is a
// netlist source with its format and, for a layout or schematic side, the
// name map of the netlister that produced it (netlist::NameMap, written by
// MAPS in netlist/name_map.hpp after the M7 and M8 exits; Q-LVS-33). The
// readers fill `db` and `top`.
//
// Owner: LVSREAD.

#pragma once

#include <memory>
#include <optional>
#include <string>

#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/read/source.hpp"

namespace hefesto::netlist {
struct NameMap;  // netlist/name_map.hpp (MAPS)
}

namespace hefesto::lvs {

struct Circuit {
  Source source;
  Format format = Format::spice;
  /// The cell to compare; empty = the last top-level cell read.
  std::string cell;
  /// The netlister's name map of a layout or schematic side; nullptr for a
  /// netlist file.
  std::shared_ptr<const netlist::NameMap> names;
  NetlistDb db;
  std::optional<CellId> top;  // set once the source is read
};

}  // namespace hefesto::lvs
