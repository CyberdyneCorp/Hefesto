// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared via geometry generator (capabilities lef-read "Generated via
// geometry", "LEF VIA and VIARULE definitions", def-read "Generated DEF
// vias"; openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D4, D6, D8 and the
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

/// The values a LEF VIARULE GENERATE gave (ViaDefinition::rule, in
/// 1/10000 micron) in input units of `units_per_micron` per micron (DEF:
/// the UNITS DISTANCE MICRONS value; LEF: 10000), rounded half away from
/// zero. A value that does not fit an int32, or a non-positive
/// `units_per_micron`, is left out (nullopt). ROWCOL stays 1 x 1.
[[nodiscard]] ViaParamSet rule_params(const ViaRuleGenerate& rule, double units_per_micron);

/// The VIARULE GENERATE parameters stored under `rule_name`, or nullptr
/// when the name has no via record or its record is not a VIARULE
/// GENERATE (Magic stores none: Q-LEFDEF-61 fix).
[[nodiscard]] const ViaRuleGenerate* find_via_rule(const ViaTable& vias, std::string_view rule_name);

/// Completes `given` (Q-LEFDEF-61 fix, both modes): each omitted LAYERS,
/// CUTSIZE, CUTSPACING or ENCLOSURE takes the value of `rule` (the LEF
/// VIARULE the definition names, through rule_params, when known and when
/// it gives that value), otherwise 0 (no layers), with the warning
/// LEF-VIA-007 naming the via and the parameter (Hefesto-only: Magic reads
/// an uninitialised value). With magic-compat ON the DEF reader keeps a
/// parameter's last value within the read and passes it as given; only one
/// never set in the read is omitted here.
[[nodiscard]] GeneratedViaParams complete_params(const ViaParamSet& given, const ViaParamSet* rule,
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

/// A parsed DEF 5.8 cut pattern, kept as the runs the text gives (its size
/// follows the text, not rows x columns, which a hostile ROWCOL makes huge).
struct CutPattern {
  struct Run {
    std::uint64_t end_bit;  // the run covers the bits up to here (from the previous run's end)
    std::uint8_t digit;     // the repeated hex digit, most significant bit leftmost
    friend bool operator==(const Run&, const Run&) = default;
  };
  struct Group {
    std::uint64_t end_row;  // the group covers the rows up to here (from the previous group's end)
    std::vector<Run> runs;
    friend bool operator==(const Group&, const Group&) = default;
  };
  std::vector<Group> groups;  // the first group is the bottom row

  /// Whether the cut at `row` (0 bottom), `column` (0 left) is present.
  [[nodiscard]] bool at(std::int32_t row, std::int32_t column) const;
};

/// A DEF 5.8 cut pattern for `rows` x `columns` cuts: `<n>_<hex>` row
/// groups joined by `_`, each giving n rows; a row's hex digits give 4
/// columns each, the most significant bit first (leftmost), `R<k><digit>`
/// repeating a digit k times; the first group is the bottom row. nullopt
/// when the text does not parse or does not cover every row.
[[nodiscard]] std::optional<CutPattern> cut_pattern(std::string_view pattern, std::int32_t rows, std::int32_t columns);

}  // namespace hefesto::lef
