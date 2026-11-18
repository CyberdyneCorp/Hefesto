// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The DEF-write side of the via generator (capability def-write "Contact
// region identification", "Generated via definitions";
// openspec/changes/m6-lef-def-oasis/design.md D9): the contact regions of a
// cell that DEF write emits as vias (defCountVias, defCountViaFunc,
// defWrite.c:1661-2040), and the name of the generated via of each region
// (Q-LEFDEF-105, 106, 107, 111, 115). With magic-compat ON a region is the
// bounding box of a contact chain and the surround probe is asymmetric, as
// in Magic; with it OFF regions are the exact rectangles and the probe is
// symmetric.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFVIA fills
// in the implementation in src/core/src/lef/via/ (design D2).

#pragma once

#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::lef {

// The LEF layer table (lef/tech/table.hpp, scaffold task 2.5).
class LayerTable;

/// One contact region DEF write emits as a via.
struct ContactRegion {
  tech::TileType type = 0;  // the contact type
  db::Rect area;            // in internal units

  friend bool operator==(const ContactRegion&, const ContactRegion&) = default;
};

/// The contact regions of `cell`, in Magic's search order.
/// SCAFFOLD: returns none until LEFVIA lands it.
[[nodiscard]] std::vector<ContactRegion> contact_regions(const db::CellDef& cell, const LayerTable& table,
                                                         const compat::CompatSettings& compat);

/// The name of the generated via of `region` (defWrite.c:1428-1512: the
/// plane short name and the size, then the via's own suffix), unique among
/// the vias of one write. SCAFFOLD: returns "" until LEFVIA lands it.
[[nodiscard]] std::string via_name(const ContactRegion& region, const LayerTable& table,
                                   const compat::CompatSettings& compat);

}  // namespace hefesto::lef
