// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF layer table, via table and non-default rules (capability
// lef-tech-section; openspec/changes/m6-lef-def-oasis/design.md D4, D5):
// - LayerTable: the records of the tech `lef` section (route, cut/via,
//   masterslice, overlap, bound, ignore and obstruction entries, aliases
//   and canonical names), keyed by LEF name in a util::MagicHashTable so
//   that enumeration follows Magic's hash order. Built at tech load
//   (from_section), completed from the DRC rules (fill_defaults,
//   Q-LEFDEF-1, 2) and rescaled by `scalegrid` (scale);
// - ViaTable and RuleTable: the VIA definitions (LEF and DEF, generated
//   ones included) and the NONDEFAULTRULE table that persist across reads
//   and writes as Magic's globals do (Q-LEFDEF-99 ON), held in
//   lef::LefSession.
//
// Owner: INTEGRATION (this header, created by the M6 scaffold); LEFTECH
// fills in the layer table in src/core/src/lef/tech/, LEFVIA and LEFR the
// via and rule records (design D2, D4).

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/util/magic_hash_table.hpp"

namespace hefesto::tech {
struct RawSection;
class Technology;
}  // namespace hefesto::tech

namespace hefesto::drc {
class RuleSet;
}

namespace hefesto::lef {

/// The LEF class of a layer record (lefInfo.h CLASS_*).
enum class LayerClass : std::uint8_t { ignore, route, cut, masterslice, overlap, bound, obstruction };

/// The preferred routing direction of a route layer.
enum class RouteDirection : std::uint8_t { unset, horizontal, vertical };

/// One record of the LEF layer table. Distances are in internal units;
/// nullopt means "not set by the section or the DRC defaults".
struct LayerRecord {
  LayerClass layer_class = LayerClass::ignore;
  tech::TileType type = 0;           // the magic type (0 = none)
  tech::TypeMask obstruction_types;  // the types an obstruction paints
  std::optional<db::Coord> width;
  std::optional<db::Coord> spacing;
  std::optional<db::Coord> pitch;
  RouteDirection direction = RouteDirection::unset;
  std::optional<db::Rect> contact_area;  // a cut's contact area
  std::string canonical_name;            // the name the writers print

  friend bool operator==(const LayerRecord&, const LayerRecord&) = default;
};

class LayerTable {
 public:
  using Visitor = std::function<void(std::string_view lef_name, const LayerRecord& record)>;

  /// Parses the tech `lef` section. SCAFFOLD: returns an empty table until
  /// LEFTECH lands it.
  [[nodiscard]] static LayerTable from_section(const tech::RawSection& section, const tech::Technology& tech,
                                               diag::DiagnosticSink& sink);

  /// Completes widths and spacings from the DRC rules (Q-LEFDEF-1, 2).
  /// SCAFFOLD: no effect until LEFTECH lands it.
  void fill_defaults(const drc::RuleSet& rules, const tech::Technology& tech, const compat::CompatSettings& compat);

  /// Rescales every distance by n/d (`scalegrid`, finer-grid reads).
  /// SCAFFOLD: no effect until LEFTECH lands it.
  void scale(std::int32_t n, std::int32_t d);

  /// The record named `lef_name` (an alias or a canonical name).
  [[nodiscard]] const LayerRecord* find(std::string_view lef_name) const;

  /// The record a writer uses for magic type `type` (Q-LEFDEF-92 order).
  /// SCAFFOLD: returns nullptr until LEFTECH lands it.
  [[nodiscard]] const LayerRecord* magic_to_lef(tech::TileType type) const;

  /// Visits every record in Magic's hash order.
  void enumerate(const Visitor& visit) const;

  [[nodiscard]] std::size_t size() const noexcept { return records_.size(); }

 private:
  util::MagicHashTable<LayerRecord> records_;
};

/// One shape of a via definition.
struct ViaShape {
  tech::TileType type = 0;
  db::Rect area;

  friend bool operator==(const ViaShape&, const ViaShape&) = default;
};

/// A VIA definition from LEF, DEF or a generated via. LEFVIA extends it.
struct ViaDefinition {
  std::vector<ViaShape> shapes;
  bool generated = false;  // from VIARULE parameters or a DEF write

  friend bool operator==(const ViaDefinition&, const ViaDefinition&) = default;
};

/// A NONDEFAULTRULE: per-layer widths and spacings. LEFR extends it.
struct NonDefaultRule {
  bool hard_spacing = false;

  friend bool operator==(const NonDefaultRule&, const NonDefaultRule&) = default;
};

/// The session's VIA definitions, keyed by name in Magic's hash order.
using ViaTable = util::MagicHashTable<ViaDefinition>;
/// The session's non-default rules, keyed by name.
using RuleTable = util::MagicHashTable<NonDefaultRule>;

}  // namespace hefesto::lef
