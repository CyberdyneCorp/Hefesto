// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF reader's view of the session's LEF tables (capability lef-read
// "LEF layer name resolution", "LEF LAYER section"; lef-tech-section "LEF
// table lifecycle"; design D4, D6): lef::LayerAccess over lef::LayerTable
// (LEFTECH) and the via table (LEFVIA). Magic keeps layers and vias in one
// hash table (LefInfo); Hefesto keeps them apart, so a name is looked up
// as a via first (a VIA that redefines a layer name shadows the layer, as
// lef/via/store.hpp says) and then as a layer.
//
// Owner: LEFR (design D2).

#pragma once

#include <set>
#include <string>
#include <string_view>

#include "hefesto/lef/read/layers.hpp"
#include "hefesto/lef/tech/table.hpp"

namespace hefesto::drc {
class RuleSet;
}

namespace hefesto::tech {
class Technology;
}

namespace hefesto::lef {

/// The LayerEntry of a via definition: class VIA, its types, and its
/// contact area when that is not degenerate (Magic's GEO_RECTNULL test).
[[nodiscard]] LayerEntry via_entry(const ViaDefinition& via);

/// The LayerEntry of a layer record: the cut's secondary obstruction type
/// is the type of `obstruction_types` other than the primary one.
[[nodiscard]] LayerEntry layer_entry(const LayerRecord& record);

class SessionLayers final : public LayerAccess {
 public:
  /// `rules` (internal units) gives the defaults of a ROUTE entry a LAYER
  /// section adds (Q-LEFDEF-5); nullptr: no DRC rules.
  SessionLayers(LayerTable& layers, const ViaTable& vias, const tech::Technology& tech, const drc::RuleSet* rules)
      : layers_(layers), vias_(vias), tech_(tech), rules_(rules) {}

  [[nodiscard]] std::optional<LayerEntry> find(std::string_view name) const override;

  /// A ROUTE record like a tech `routing` line's (Q-LEFDEF-5 fix): the DRC
  /// default width and spacing of `type` (3 and 4 when the rules give 0 or
  /// there are none), pitch 0, horizontal.
  void add_route(std::string_view name, tech::TileType type) override;

  void update_route(std::string_view name, const RouteUpdate& update) override;

  /// The name takes its place in the table's hash order with no record
  /// (LayerTable::add then release), once per name.
  void reserve(std::string_view name) override;

 private:
  LayerTable& layers_;
  const ViaTable& vias_;
  const tech::Technology& tech_;
  const drc::RuleSet* rules_;
  std::set<std::string, std::less<>> reserved_;
};

}  // namespace hefesto::lef
