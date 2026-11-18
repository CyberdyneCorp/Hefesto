// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF layer table, via table and non-default rules (capability
// lef-tech-section; openspec/changes/m6-lef-def-oasis/design.md D4, D5):
// - LayerTable: the records of the tech `lef` section (route, cut/via,
//   masterslice, overlap, bound, ignore and obstruction entries, aliases
//   and canonical names), keyed by LEF name in a util::MagicHashTable so
//   that enumeration follows Magic's hash order; the aliases of a layer
//   share one record. Built at tech load
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

  /// The record named `lef_name` (an alias or a canonical name). Aliases of
  /// one layer return the same pointer.
  [[nodiscard]] const LayerRecord* find(std::string_view lef_name) const;

  /// The record a writer uses for magic type `type` (Q-LEFDEF-92 order).
  /// SCAFFOLD: returns nullptr until LEFTECH lands it.
  [[nodiscard]] const LayerRecord* magic_to_lef(tech::TileType type) const;

  /// Visits every LEF name in Magic's hash order, with its record (aliases
  /// visit the same record).
  void enumerate(const Visitor& visit) const;

  /// The number of LEF names (aliases included).
  [[nodiscard]] std::size_t size() const noexcept { return names_.size(); }

 private:
  // One record per layer, shared by its aliases as Magic's lefLayer is
  // (lef-tech-section "LEF name aliasing and canonical names"): names_ maps
  // each LEF name to an index into records_, so scale and fill_defaults
  // update a layer once and find returns one pointer per layer.
  std::vector<LayerRecord> records_;
  util::MagicHashTable<std::size_t> names_;
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

/// What one rule of a NONDEFAULTRULE names (lefRule::lefInfo).
enum class RuleTarget : std::uint8_t {
  none,   // the name had no entry (Magic's null lefInfo)
  layer,  // an entry of the LEF layer table
  via,    // a VIA definition
};

/// One rule of a NONDEFAULTRULE (lefRule): a layer or via and its values.
/// Distances are in internal units; the extension is in half units.
struct NonDefaultRuleLayer {
  std::string name;  // the name looked up
  RuleTarget target = RuleTarget::none;
  db::Coord width = 0;
  db::Coord spacing = 0;
  db::Coord extension = 0;

  friend bool operator==(const NonDefaultRuleLayer&, const NonDefaultRuleLayer&) = default;
};

/// A NONDEFAULTRULE (LefRules), filled by LEF read (lef-read "LEF
/// NONDEFAULTRULE section") and DEF read/write.
struct NonDefaultRule {
  std::string name;  // the rule set's name (after Q-LEFDEF-9 renaming)
  bool hard_spacing = false;  // HARDSPACING was read (no effect on reading)
  /// The rules in Magic's list order: built by prepending, so the last rule
  /// read is first.
  std::vector<NonDefaultRuleLayer> layers;

  friend bool operator==(const NonDefaultRule&, const NonDefaultRule&) = default;
};

/// The session's VIA definitions, keyed by name in Magic's hash order.
using ViaTable = util::MagicHashTable<ViaDefinition>;
/// The session's non-default rules, keyed by name.
using RuleTable = util::MagicHashTable<NonDefaultRule>;

}  // namespace hefesto::lef
