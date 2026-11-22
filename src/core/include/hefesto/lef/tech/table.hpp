// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The LEF layer table, via table and non-default rules (capability
// lef-tech-section; openspec/changes/m6-lef-def-oasis/design.md D4, D5):
// - LayerTable: the records of the tech `lef` section (route, cut/via,
//   masterslice, overlap, bound, ignore and obstruction entries, aliases
//   and canonical names), keyed by LEF name in a util::MagicHashTable so
//   that enumeration follows Magic's hash order; the aliases of a layer
//   share one record. Its key table is Magic's LefInfo key order: the
//   readers and DEF write note every via name in it too (note_key), so one
//   insertion history covers the layers and the vias (Q-LEFDEF-135).
//   Built at tech load
//   (from_section), completed from the DRC rules (fill_defaults,
//   Q-LEFDEF-1, 2) and rescaled by `scalegrid` (scale); the session hooks
//   are in lef/tech/lifecycle.hpp;
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

/// The LEF class of a layer record (lefInt.h CLASS_*). `cut` is Magic's
/// CLASS_VIA: a cut layer or a via definition. The tech section never makes
/// an `obstruction` record: an obstruction line makes a route or cut record
/// whose obstruction type is set (lef-tech-section "Keyword to LEF class
/// mapping").
enum class LayerClass : std::uint8_t { ignore, route, cut, masterslice, overlap, bound, obstruction };

/// The preferred routing direction of a route layer.
enum class RouteDirection : std::uint8_t { unset, horizontal, vertical };

/// One record of the LEF layer table (Magic's lefLayer). Distances are in
/// internal units; a contact area is in half internal units, centred on the
/// origin (lef-tech-section "Default contact area").
///
/// The values the DRC rules complete (route width and spacing, and the
/// contact area of a `contact`-flavor cut; Q-LEFDEF-1, 2):
/// - with magic-compat ON they hold Magic's numbers, sentinels included: -1,
///   or (-1, -1, -1, -1) for a contact area, until filled. A sentinel scales
///   like any value, so it can become -2 or 0 and stay unfilled;
/// - with magic-compat OFF an unfilled value is std::nullopt (the unset
///   flag). Scaling keeps it unset and fill_defaults replaces it.
/// A `cut`-flavor record's contact area is Magic's null rectangle
/// (0, 0, 0, 0), GeoNullRect; scaling keeps it (0, 0, 0, 0).
struct LayerRecord {
  LayerClass layer_class = LayerClass::ignore;
  tech::TileType type = 0;              // the normal magic type (0 = none)
  tech::TileType obstruction_type = 0;  // Magic's obsType, painted by OBS geometry (0 = none)
  /// obstruction_type plus, for a cut record, the secondary via-obstruction
  /// type of a two-type obstruction line (Magic's info.via.obsType).
  tech::TypeMask obstruction_types;
  std::optional<db::Coord> width;
  std::optional<db::Coord> spacing;
  std::optional<db::Coord> pitch;
  RouteDirection direction = RouteDirection::unset;
  std::optional<db::Rect> contact_area;  // a cut's contact area
  std::string canonical_name;            // the name the writers print

  friend bool operator==(const LayerRecord&, const LayerRecord&) = default;
};

namespace tech_detail {
class SectionReader;
}

class LayerTable {
 public:
  using Visitor = std::function<void(std::string_view lef_name, const LayerRecord& record)>;

  /// Parses the tech `lef` section (LefTechLine for each line): keywords,
  /// magic type resolution, class mapping, aliases and canonical names, with
  /// Magic's tech-load messages (LEF-TECH-*, never fatal) to `sink`. `rules`
  /// is the current DRC style at the technology's own (lambda) grid, as when
  /// Magic reads the section; nullptr means no rules (every default is 0).
  [[nodiscard]] static LayerTable from_section(const tech::RawSection& section, const tech::Technology& tech,
                                               const drc::RuleSet* rules, const compat::CompatSettings& compat,
                                               diag::DiagnosticSink& sink);
  /// The same with no DRC rules and magic-compat ON.
  [[nodiscard]] static LayerTable from_section(const tech::RawSection& section, const tech::Technology& tech,
                                               diag::DiagnosticSink& sink);

  /// Completes widths, spacings and contact areas from `rules` (internal
  /// units), lef-tech-section "Filling defaults after grid scaling". With
  /// magic-compat ON this is LefTechSetDefaults: values of exactly -1 are
  /// filled, and an unfilled spacing overwrites the width (Q-LEFDEF-2); the
  /// tech load runs it only when the grid is not 1:1. With magic-compat OFF
  /// every unset value gets its own default; it runs after every tech load
  /// and grid change. A record with no normal type gets 0 (Magic indexes
  /// its rule table with -1 there: fixed in both modes).
  void fill_defaults(const drc::RuleSet& rules, const tech::Technology& tech, const compat::CompatSettings& compat);

  /// Rescales every record once by n/d (LefTechScale): route width, spacing
  /// and pitch truncate toward zero, contact areas floor (DBScalePoint).
  /// Unset values stay unset; nothing is refilled.
  void scale(std::int32_t n, std::int32_t d);

  /// The record named `lef_name` (an alias or a canonical name). Aliases of
  /// one layer return the same pointer.
  [[nodiscard]] const LayerRecord* find(std::string_view lef_name) const;
  /// The same, writable (for the LEF and DEF readers).
  [[nodiscard]] LayerRecord* find_mutable(std::string_view lef_name);
  /// HashLookOnly(&LefInfo, name) != NULL: `lef_name` is a key of the shared
  /// key order, with or without a record (a released name, a noted via or
  /// rule name included).
  [[nodiscard]] bool has_key(std::string_view lef_name) const;

  /// Binds `lef_name` to a new record, whose canonical name defaults to
  /// `lef_name`; a name already bound moves to the new record. For the
  /// layers and vias the LEF and DEF readers add.
  LayerRecord& add(std::string_view lef_name, LayerRecord record);
  /// Leaves `lef_name` in the table with no record (lefRemoveGeneratedVias):
  /// find and enumerate then treat it as absent.
  void release(std::string_view lef_name);

  /// HashFind(&LefInfo, name) with no record set: inserts `lef_name` as a
  /// key with no record when it is absent, so it takes its place in the
  /// shared key order (a VIA definition's name, a NONDEFAULTRULE name that
  /// names nothing); a key already present keeps its place and record
  /// (def-write "Table enumeration order", Q-LEFDEF-135).
  void note_key(std::string_view lef_name);

  /// LefRedefined on one alias of a shared record (Q-LEFDEF-8 fix): the
  /// via redefinition takes `lef_name` away from its layer record, which
  /// the other names keep. `lef_name` is released (find and enumerate treat
  /// it as absent; its hash slot stays) and, when the record's canonical
  /// name was `lef_name`, the record takes the first other name bound to
  /// it in hash order (Magic takes the first name of the whole table,
  /// which may belong to an unrelated entry). Nothing happens when
  /// `lef_name` has no record.
  void redefine_alias(std::string_view lef_name);

  /// defGetType(type, ..., TRUE): the first record in enumeration order
  /// whose class is cut for a contact type (route or masterslice otherwise)
  /// and whose normal or obstruction type is `type` (Q-LEFDEF-92 order);
  /// nullptr when none. Contact types are those of the technology the
  /// table was read with.
  [[nodiscard]] const LayerRecord* magic_to_lef(tech::TileType type) const;

  /// Visits every LEF name in Magic's hash order, with its record (aliases
  /// visit the same record).
  void enumerate(const Visitor& visit) const;

  using KeyVisitor = std::function<void(std::string_view key, const LayerRecord* record)>;
  /// Visits every key of the shared key order (Magic's LefInfo), records or
  /// not: a released name, a noted via name and a reserved name visit
  /// nullptr.
  void enumerate_keys(const KeyVisitor& visit) const;

  /// The number of LEF names (aliases included).
  [[nodiscard]] std::size_t size() const noexcept { return names_.size(); }

 private:
  friend class tech_detail::SectionReader;

  /// The indices of the records some name is bound to, each once.
  [[nodiscard]] std::vector<std::size_t> live_records() const;

  // One record per layer, shared by its aliases as Magic's lefLayer is
  // (lef-tech-section "LEF name aliasing and canonical names"): names_ maps
  // each LEF name to an index into records_, so scale and fill_defaults
  // update a layer once and find returns one pointer per layer.
  std::vector<LayerRecord> records_;
  util::MagicHashTable<std::size_t> names_;
  tech::TypeMask contact_types_;  // DBIsContact of the section's technology
};

/// One shape of a via definition (half internal units in a ViaDefinition).
struct ViaShape {
  tech::TileType type = 0;
  db::Rect area;

  friend bool operator==(const ViaShape&, const ViaShape&) = default;
};

/// LAYERS of a generated via: the bottom, cut and top layers, as the
/// reader resolved them (lef-read "LEF layer name resolution"); nullopt =
/// did not resolve.
struct ViaLayers {
  std::optional<tech::TileType> bottom;
  std::optional<tech::TileType> cut;
  std::optional<tech::TileType> top;

  friend bool operator==(const ViaLayers&, const ViaLayers&) = default;
};

/// The parameters of a LEF `VIARULE <name> GENERATE` section, kept so that
/// a generated via naming the rule takes the values it omits (def-read
/// "Generated DEF vias", Q-LEFDEF-61 fix; Magic keeps none). Distances are
/// in 1/10000 micron; nullopt: the rule did not give the value.
/// - layers: the first two routing layers in LAYER order (bottom, top) and
///   the contact layer (cut), when all three were named;
/// - cut_size: the size of the cut layer's RECT;
/// - cut_spacing: the cut layer's SPACING x BY y (centre to centre) minus
///   the cut size, the DEF CUTSPACING (edge to edge);
/// - bottom_enclosure, top_enclosure: `ENCLOSURE a b` of the bottom and top
///   routing layers as (a, b).
struct ViaRuleGenerate {
  std::optional<ViaLayers> layers;
  std::optional<db::Point> cut_size;
  std::optional<db::Point> cut_spacing;
  std::optional<db::Point> bottom_enclosure;
  std::optional<db::Point> top_enclosure;

  friend bool operator==(const ViaRuleGenerate&, const ViaRuleGenerate&) = default;
};

/// A VIA definition from LEF, DEF or a generated via: Magic's lefLayer of
/// class VIA (lef-read "LEF VIA and VIARULE definitions"). Areas are in
/// half internal units (Magic doubles via geometry so that a via centre may
/// fall between grid points). Built by lef/via/store.hpp (LEFVIA).
struct ViaDefinition {
  LayerClass layer_class = LayerClass::cut;  // Magic's CLASS_VIA
  std::optional<tech::TileType> type;        // the normal type; nullopt = none (Magic's -1)
  std::optional<tech::TileType> obstruction_type;
  db::Rect area;                 // the contact area; (0,0,0,0) = not set yet (GeoNullRect)
  std::vector<ViaShape> shapes;  // the extra rectangles, list head first
  std::string canonical_name;
  bool generated = false;       // from VIARULE parameters or a DEF write
  std::uint32_t ref_count = 1;  // refCnt: 0 marks a DEF write's temporary via
  /// A LEF VIARULE GENERATE section's parameters (nullopt for every other
  /// definition).
  std::optional<ViaRuleGenerate> rule;

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
