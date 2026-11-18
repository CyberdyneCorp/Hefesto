// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The DEF-write side of the via generator (capability def-write "Contact
// region identification", "Generated via definitions", "VIAS section
// content"; openspec/changes/m6-lef-def-oasis/design.md D9): the contact
// regions of a cell that DEF write emits as vias (defCountVias,
// defCountViaFunc, defWrite.c:1661-2040), the generated via of each region
// with its name (Q-LEFDEF-105, 106), their registration in the via table,
// and the RECT lines of a via record (defWriteVias, defWrite.c:2099-2256;
// Q-LEFDEF-107, 115).
//
// Owner: LEFVIA (design D2).

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::drc {
class RuleSet;
}

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::lef {

/// One contact region DEF write emits as a via.
struct ContactRegion {
  tech::TileType type = 0;  // the contact type T (a stacked type's residue on `plane`)
  tech::PlaneId plane = 0;  // the plane searched (T's home plane)
  db::Rect area;            // the contact rectangle C, internal units

  friend bool operator==(const ContactRegion&, const ContactRegion&) = default;
};

/// The contact regions of `cell`, plane by plane, in Magic's search order.
/// With magic-compat ON a region is the bounding box of its contact chain,
/// as in Magic; with it OFF the chain is split into maximal horizontal
/// strips, merged vertically when their x extents are equal, one region
/// each (Q-LEFDEF-111).
[[nodiscard]] std::vector<ContactRegion> contact_regions(const db::CellDef& cell, const tech::Technology& tech,
                                                         const compat::CompatSettings& compat);

/// What DEF write's via code reads.
struct DefViaContext {
  const tech::Technology& tech;
  /// The DRC rules of the surround queries; nullptr: no surround rules.
  const drc::RuleSet* rules = nullptr;
  /// The LEF name of a magic type (the magic-to-LEF layer map of def-write);
  /// nullopt when the type has none.
  std::function<std::optional<std::string>(tech::TileType)> lef_name;
  float scale = 1.0F;  // s, DEF units per internal unit (binary32)
  const compat::CompatSettings& compat;
  diag::DiagnosticSink& sink;
};

/// The LEF names of `table` (LayerTable::magic_to_lef, canonical names),
/// for DefViaContext::lef_name.
[[nodiscard]] std::function<std::optional<std::string>(tech::TileType)> lef_names(const LayerTable& table);

/// The generated via of one region: its name, its record (class VIA, type
/// T, area (−w,−h,w,h) half units, reference count 0, the surround
/// rectangles of the first two residues) and its position key
/// `<plane short name>_<xl>_<yl>`.
struct RegionVia {
  std::string name;
  ViaDefinition record;
  std::string position_key;
};

/// The generated via of `region` in `cell` (defCountViaFunc); nullopt when
/// T has no LEF name.
[[nodiscard]] std::optional<RegionVia> region_via(const db::CellDef& cell, const ContactRegion& region,
                                                  const DefViaContext& context);

/// The name of the generated via of `region`; nullopt when T has no LEF
/// name.
[[nodiscard]] std::optional<std::string> via_name(const db::CellDef& cell, const ContactRegion& region,
                                                  const DefViaContext& context);

/// What registering a cell's vias did.
struct DefVias {
  std::uint32_t count = 0;                        // the records created (the VIAS count)
  std::vector<std::string> created;               // their names, in creation order
  std::map<std::string, std::string> positions;   // position key -> via name, for the nets
};

/// defCountVias: the generated via of every region of `cell`; a name not in
/// `vias` (nor a layer name of `layers`) is inserted and counted, a known
/// one is reused uncounted.
[[nodiscard]] DefVias register_vias(const db::CellDef& cell, ViaTable& vias, const LayerTable& layers,
                                    const DefViaContext& context);

/// One `+ RECT <layer> ( x1 y1 ) ( x2 y2 )` of a VIAS record: binary32
/// values the writer prints with %.10g.
struct ViaRect {
  std::string layer;
  float x1 = 0;
  float y1 = 0;
  float x2 = 0;
  float y2 = 0;

  friend bool operator==(const ViaRect&, const ViaRect&) = default;
};

/// The RECTs of `via` (a record of type T) in the VIAS section: one per
/// residue of T in type order (its surround rectangle, else the area), then
/// the cut array of `style` (the current output style, nullptr: none),
/// else one cut the size of the area. The cut-array warnings "Warning: via
/// size is <z> but area width|height is <n>!" (LEF-VIA-009/010) go to the
/// sink, the height one with Magic's inverted test under magic-compat ON
/// (Q-LEFDEF-107). A residue or cut with no LEF name is skipped with
/// LEF-VIA-011 (Q-LEFDEF-115 fix; Magic prints "(null)").
[[nodiscard]] std::vector<ViaRect> via_rects(const ViaDefinition& via, std::string_view via_name,
                                             const mask::OutputStyle* style, const DefViaContext& context);

}  // namespace hefesto::lef
