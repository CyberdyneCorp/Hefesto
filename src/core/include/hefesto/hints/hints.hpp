// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Mask-hint operations (mask-hints; openspec/changes/m3-drc/design.md D10):
// the property-name convention, value text, the DRC inside test, copying
// and flattening hints between cells, and the library operations the M5
// mask readers and writers call (design D13).
//
// (Magic ref: database/DBio.c dbReadProperties (hint values); drc/DRCbasic.c
// drcTile :921-957, :1381-1417 and database/DBtiles.c DBSrPaintArea (inside
// test); database/DBcellcopy.c dbCopyMaskHintsFunc, DBCellCopyMaskHints,
// DBFlatCopyMaskHints and database/DBcellsrch.c dbMoveProp (copy, flatten,
// origin change); cif/CIFgen.c CIFOP_MASKHINTS, cif/CIFrdcl.c
// CIFPaintCurrent (M5 operations))
//
// Owner: HINTS (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/hints/region.hpp"

namespace hefesto::db {
class CellDef;
}

namespace hefesto::hints {

/// "MASKHINTS_": the key prefix of a hint property.
inline constexpr std::string_view kPropertyPrefix = "MASKHINTS_";

/// The hint name of a property key ("MASKHINTS_SRAM" -> "SRAM"), or
/// std::nullopt when `key` is not a hint key.
[[nodiscard]] std::optional<std::string_view> hint_name(std::string_view key) noexcept;
/// The property key of hint `name`.
[[nodiscard]] std::string property_key(std::string_view name);

/// The hint region `name` of `cell`, or nullptr when the cell has none.
[[nodiscard]] const HintRegion* find_hint(const db::CellDef& cell, std::string_view name);

/// A hint value read from text: the region and whether values after the
/// last complete group of four were dropped.
struct ParsedHint {
  HintRegion region;
  bool truncated = false;
};

/// The `.mag` hint value (dbReadProperties): whitespace-separated %d
/// integers (a token is read as far as it is an integer, so `4.5` reads 4),
/// each multiplied by `scale_up` when it is above 1 and then divided,
/// truncating, by `scale_down` when it is above 1, grouped in fours as
/// x1 y1 x2 y2 and painted in order. Fails (DB-PROP-001, "Mask-hint
/// "<rest>" has non-integer values!" where <rest> is the text from the bad
/// token on) when a token does not start with an integer; the caller then
/// drops the property. `truncated` is set when the count is not a multiple
/// of four (the caller reports "Mask-hint property number of values is not
/// divisible by four.  Truncated.").
[[nodiscard]] diag::Outcome<ParsedHint> parse_hint_value(std::string_view text, std::int64_t scale_up = 1,
                                                         std::int64_t scale_down = 1);
/// The strips as space-separated groups of four (property command, `.mag`).
[[nodiscard]] std::string format_hint_value(const HintRegion& region);

/// An edge segment of the basic check: the vertical segment x = `at`,
/// y in [lo, hi] (`vertical` true) or the horizontal one y = `at`,
/// x in [lo, hi].
struct Segment {
  bool vertical = true;
  db::Coord at = 0;
  db::Coord lo = 0;
  db::Coord hi = 0;

  friend bool operator==(const Segment&, const Segment&) = default;
};

/// Whether `segment` is inside `region` for exception/exemption scoping
/// (mask-hints "Mask hints scope DRC exception and exemption rules").
/// With `magic_compat` (Q-DRC-11): Magic's DBSrPaintArea walk of the hint
/// plane with the zero-width segment, which depends on the side of a touch
/// and on the strip decomposition. Without it (Q-MISC-13): the closed region
/// holds a part of positive length of the segment.
[[nodiscard]] bool inside(const HintRegion& region, const Segment& segment, bool magic_compat);

/// dbCopyMaskHintsFunc for every hint of `source`: the region clipped to
/// `clip` (in destination coordinates; std::nullopt = no clip), transformed
/// by `transform` and unioned into `destination`'s hint of the same name.
/// As in Magic the destination property is created (possibly empty) for
/// every hint the source has. File readers' bookkeeping (no undo
/// recording) is the caller's; read-only destinations are left unchanged
/// (db::set_property).
void copy_hints(const db::CellDef& source, db::CellDef& destination, const db::Transform& transform,
                const std::optional<db::Rect>& clip);

/// DBFlatCopyMaskHints (DBTreeSrCells with `mode`): copies into
/// `destination` the hints of the array elements of `root`'s descendants
/// that overlap `area` (in `root` coordinates), each clipped to `area` and
/// mapped by `to_destination` composed with the element's transform to
/// `root`. The root's own hints are not copied. With Descend::all every
/// element found is copied and searched; otherwise an element that is not
/// descended is copied and not searched, and a descended one is searched
/// but not copied (Magic's xMask rules). Uses `flatten` (into a new cell)
/// and the DRC interaction flattening (design D10, mask-hints "Copying,
/// flattening and transforming hints" (a) and (c)).
void flat_copy_hints(const db::CellDef& root, const db::Rect& area, const db::Transform& to_destination,
                     db::CellDef& destination, db::Descend mode = db::Descend::all);

/// dbMoveProp for hints: every hint region of `cell` translated by
/// (dx, dy) (origin change; `move origin` passes the negated new origin).
void translate_hints(db::CellDef& cell, db::Coord dx, db::Coord dy);

// --- M5 operations (design D10, D13) ---------------------------------------

/// `mask-hints <name>` output operator (CIFOP_MASKHINTS): the hint region
/// of `cell` with every coordinate multiplied by `scale` (the output
/// style's scalefactor); empty when the cell has no such hint. The caller
/// unions it into the layer being generated.
[[nodiscard]] HintRegion output_region(const db::CellDef& cell, std::string_view name, std::int32_t scale);

/// cifinput templayer `mask-hints <name>`: unions `region` (internal units)
/// into `cell`'s MASKHINTS_<name>, creating it; an empty region changes
/// nothing (Q-MASK-308/Q-MISC-11: the prefixed key is looked up, never the
/// bare name).
void add_input_region(db::CellDef& cell, std::string_view name, const HintRegion& region);

/// `gds maskhints`: H = `input` minus `generated` (both in output units),
/// each strip of H with every coordinate divided by `scale`, truncating
/// toward zero, and painted. The caller unions the result into
/// MASKHINTS_<layer> with add_input_region().
[[nodiscard]] HintRegion gds_maskhints_region(const HintRegion& input, const HintRegion& generated,
                                              std::int32_t scale);

}  // namespace hefesto::hints
