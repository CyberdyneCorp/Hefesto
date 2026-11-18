// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared via geometry generator (capabilities lef-read "Generated via
// geometry", "LEF VIA and VIARULE definitions", def-read "Generated DEF
// vias"; openspec/changes/m6-lef-def-oasis/design.md D4, D6, D8 and the
// decision table's "Shared generators" note): the shapes of a via given by
// VIARULE parameters (CUTSIZE, LAYERS, CUTSPACING, ENCLOSURE, ROWCOL, and
// with magic-compat OFF also ORIGIN, OFFSET and PATTERN, Q-LEFDEF-62), with
// each cut grown to the output style's contact size (lef/via/cut.hpp,
// Q-LEFDEF-17). DEF read uses it for VIAS, and LEF read for vias defined by
// a via rule with magic-compat OFF (Q-LEFDEF-7): one generator.
//
// Owner: LEFVIA (design D2).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/lef/via/context.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::lef {

/// LAYERS: the bottom, cut and top layers, as the reader resolved them
/// (lef-read "LEF layer name resolution"); nullopt = did not resolve.
struct ViaLayers {
  std::optional<tech::TileType> bottom;
  std::optional<tech::TileType> cut;
  std::optional<tech::TileType> top;

  friend bool operator==(const ViaLayers&, const ViaLayers&) = default;
};

/// The LAYERS order check, run where LAYERS is read (defRead.c:2045-2083):
/// when the middle layer is not a contact, the warning "Improper layer order
/// for VIARULE." (LEF-VIA-005); then the cut is swapped with the top layer
/// if that is a contact, else with the bottom layer if that is one, else
/// the error "No cut layer specified in VIARULE." (LEF-VIA-006).
void order_layers(ViaLayers& layers, const ViaContext& context);

/// The parameters of one generated via. Distances are integers in the
/// input database units (DEF units; LEF read converts its microns), with
/// `scale` input units per internal unit (DEF read's oscale, binary32).
struct GeneratedViaParams {
  ViaLayers layers;
  db::Point cut_size;          // CUTSIZE x y
  db::Point cut_spacing;       // CUTSPACING x y
  db::Point bottom_enclosure;  // ENCLOSURE bottom x y
  db::Point top_enclosure;     // ENCLOSURE top x y
  std::int32_t rows = 1;       // ROWCOL
  std::int32_t columns = 1;
  float scale = 1.0F;
  // magic-compat OFF only (Q-LEFDEF-62); DEF 5.8 semantics:
  db::Point origin;         // ORIGIN: shifts every shape
  db::Point bottom_offset;  // OFFSET: shifts the bottom enclosure rectangle
  db::Point top_offset;     //         and the top one (cuts stay)
  std::string pattern;      // PATTERN (see cut_pattern); empty: every cut

  friend bool operator==(const GeneratedViaParams&, const GeneratedViaParams&) = default;
};

/// The parameters a definition gave; nullopt = omitted. ROWCOL defaults to
/// 1 x 1 at each definition, so it is never omitted.
struct ViaParamSet {
  std::optional<ViaLayers> layers;
  std::optional<db::Point> cut_size;
  std::optional<db::Point> cut_spacing;
  std::optional<db::Point> bottom_enclosure;
  std::optional<db::Point> top_enclosure;
  std::int32_t rows = 1;
  std::int32_t columns = 1;
  float scale = 1.0F;
  db::Point origin;
  db::Point bottom_offset;
  db::Point top_offset;
  std::string pattern;
};

/// Completes `given` (Q-LEFDEF-61 fix, both modes): each omitted LAYERS,
/// CUTSIZE, CUTSPACING or ENCLOSURE takes the value of `rule` (the LEF
/// VIARULE the definition names, when known), otherwise 0 (no layers), with
/// the warning LEF-VIA-007 naming the via and the parameter (Hefesto-only:
/// Magic reads an uninitialised value). With magic-compat ON the DEF reader
/// keeps a parameter's last value within the read and passes it as given;
/// only one never set in the read is omitted here.
[[nodiscard]] GeneratedViaParams complete_params(const ViaParamSet& given, const GeneratedViaParams* rule,
                                                 std::string_view via_name, const ViaContext& context);

/// The geometry of a generated via, in half internal units about its
/// origin: the normal type and contact area (the top layer), and the extra
/// rectangles list head first (the last cut generated, ..., the first cut,
/// then the bottom rectangle). Shapes on a layer that did not resolve are
/// left out (Magic adds them with type -1).
struct ViaGeometry {
  std::optional<tech::TileType> type;
  db::Rect area;
  std::vector<ViaShape> shapes;

  friend bool operator==(const ViaGeometry&, const ViaGeometry&) = default;
};

class ViaGenerator {
 public:
  /// LefGenViaGeometry (lefRead.c:2545-2626) for `params`, each cut grown
  /// to the output style's contact size; with magic-compat OFF, ORIGIN,
  /// OFFSET and PATTERN applied too (Q-LEFDEF-62). LEF-VIA-008 reports a
  /// PATTERN that does not parse (every cut is kept).
  [[nodiscard]] static ViaGeometry generated(const GeneratedViaParams& params, const ViaContext& context);
};

/// Puts generated geometry into a via record, as LefGenViaGeometry does:
/// the contact area and normal type become the geometry's, the shapes are
/// prepended to the record's extra list, and the record is marked
/// generated. Rectangles read before stay in the extra list.
void apply_generated(ViaDefinition& via, const ViaGeometry& geometry);

/// A DEF 5.8 cut pattern for `rows` x `columns` cuts: `<n>_<hex>` row
/// groups joined by `_`, each giving n rows; a row's hex digits give 4
/// columns each, the most significant bit first (leftmost), `R<k><digit>`
/// repeating a digit k times; the first group is the bottom row. present[row]
/// [column] (row 0 bottom, column 0 left); nullopt when the text does not
/// parse or does not cover every row.
[[nodiscard]] std::optional<std::vector<std::vector<bool>>> cut_pattern(std::string_view pattern, std::int32_t rows,
                                                                        std::int32_t columns);

}  // namespace hefesto::lef
