// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The shared via geometry generator (capabilities lef-read "Generated via
// geometry", "LEF VIA and VIARULE definitions", def-read "Generated DEF
// vias"; openspec/changes/m6-lef-def-oasis/design.md D4, D6, D8 and the
// decision table's "Shared generators" note): the shapes of a via given by
// VIARULE parameters (CUTSIZE, LAYERS, CUTSPACING, ENCLOSURE, ROWCOL, and
// with magic-compat OFF also ORIGIN, OFFSET and PATTERN, Q-LEFDEF-62), with
// the cut grown to the output style's contact size (lef/via/cut.hpp,
// Q-LEFDEF-17). LEF read, DEF read and DEF write all call this one
// generator.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFVIA fills
// in the implementation in src/core/src/lef/via/ (design D2).

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/lef/tech/table.hpp"

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::lef {

/// The VIARULE parameters of one generated via, in internal units.
struct GeneratedViaParams {
  std::string bottom_layer;  // LAYERS: LEF names
  std::string cut_layer;
  std::string top_layer;
  db::Point cut_size;          // CUTSIZE x y
  db::Point cut_spacing;       // CUTSPACING x y
  db::Point bottom_enclosure;  // ENCLOSURE bottom x y
  db::Point top_enclosure;     // ENCLOSURE top x y
  std::int32_t rows = 1;       // ROWCOL
  std::int32_t columns = 1;
  db::Point origin;         // ORIGIN (OFF only)
  db::Point bottom_offset;  // OFFSET (OFF only)
  db::Point top_offset;
  std::string pattern;  // PATTERN (OFF only); empty: every cut

  friend bool operator==(const GeneratedViaParams&, const GeneratedViaParams&) = default;
};

/// The shapes of a generated via, relative to its origin.
struct ViaGeometry {
  std::vector<ViaShape> shapes;

  friend bool operator==(const ViaGeometry&, const ViaGeometry&) = default;
};

class ViaGenerator {
 public:
  /// The geometry of the via `params` describe. SCAFFOLD: returns no
  /// shapes until LEFVIA lands it.
  [[nodiscard]] static ViaGeometry generated(const GeneratedViaParams& params, const LayerTable& table,
                                             const mask::OutputStyle& style, const compat::CompatSettings& compat);
};

}  // namespace hefesto::lef
