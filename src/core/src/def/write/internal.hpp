// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The DEF writer's private interfaces (capability def-write;
// openspec/changes/m6-lef-def-oasis/design.md D9): the session state one
// write reads (Context), Magic's single LefInfo table from the layer
// table's shared key order and the via records (InfoEntry, info_order;
// "Table enumeration order", Q-LEFDEF-135), the magic-to-LEF layer map (LayerMap, defMakeInverseLayerMap),
// the sections the writer assembles, and the net classification shared by
// the nets and the blockages.
//
// Owner: DEFW (design D2).

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/extflat/read.hpp"
#include "hefesto/extract/tech/style_view.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/lef/via/regions.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::drc {
class RuleSet;
}

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::def::write_detail {

// The codes of docs/diagnostics/codes/def.md, WRITE section.
inline constexpr std::string_view kProgress = "DEF-WRITE-001";      // TxPrintf lines
inline constexpr std::string_view kCannotOpen = "DEF-WRITE-002";    // the output file cannot be opened
inline constexpr std::string_view kUnnamed = "DEF-WRITE-003";       // (UNNAMED) cell
inline constexpr std::string_view kNoExt = "DEF-WRITE-004";         // no .ext netlist
inline constexpr std::string_view kStale = "DEF-WRITE-005";         // Q-LEFDEF-110 OFF
inline constexpr std::string_view kPinLayer = "DEF-WRITE-006";      // pin on an unmapped layer (Q-LEFDEF-114)
inline constexpr std::string_view kFloating = "DEF-WRITE-007";      // nothing at a node location
inline constexpr std::string_view kNoVia = "DEF-WRITE-008";         // via position not in the index
inline constexpr std::string_view kArgument = "DEF-WRITE-009";      // def write argument notes
inline constexpr std::string_view kNoCell = "DEF-WRITE-010";        // no target cell / technology
inline constexpr std::string_view kUnits = "DEF-WRITE-011";         // -units value <= 0 (Q-LEFDEF-104 fix)
inline constexpr std::string_view kMagicCrash = "DEF-WRITE-012";    // Hefesto-only: Magic crashes here

/// lefFileOpen's names for `base` (def-write "Output file naming"), in the
/// order they are tried.
[[nodiscard]] std::vector<std::string> output_names(std::string_view base, std::string_view cell_name,
                                                    bool magic_compat);

/// %.10g of a binary32 value widened to binary64.
[[nodiscard]] std::string g10(double value);
[[nodiscard]] inline std::string g10(float value) {
  return g10(static_cast<double>(value));
}
/// f32(v) * s, binary32.
[[nodiscard]] inline float scaled(std::int64_t v, float s) {
  return static_cast<float>(v) * s;
}

/// One key of Magic's LefInfo table, which holds the layers and the vias in
/// one hash table. Hefesto keeps their records in LayerTable and ViaTable
/// and their keys in the layer table's shared key order; info_order pairs
/// each key with its record.
struct InfoEntry {
  std::string key;
  const lef::LayerRecord* layer = nullptr;
  const lef::ViaDefinition* via = nullptr;

  [[nodiscard]] bool live() const noexcept { return layer != nullptr || via != nullptr; }
  /// The record's class; a via record is Magic's CLASS_VIA (cut).
  [[nodiscard]] lef::LayerClass layer_class() const noexcept;
  /// The normal magic type; nullopt for Magic's -1.
  [[nodiscard]] std::optional<tech::TileType> type() const noexcept;
  /// The obstruction type (obsType); nullopt for Magic's -1.
  [[nodiscard]] std::optional<tech::TileType> obstruction_type() const noexcept;
  [[nodiscard]] const std::string& canonical_name() const noexcept;
};

/// LefInfo in Magic's enumeration order: the shared key order of `layers`
/// (LayerTable::enumerate_keys), which holds the tech-section names, the
/// layers the readers add and every via name the readers and DEF write
/// note, in their one insertion history, growth included (Q-LEFDEF-135).
/// Keys with no record (cleared via slots, released and reserved names)
/// are listed with no record, as Magic keeps them. A via named like a
/// layer shadows it in place (def-read). A via the key order does not hold
/// is not listed: every session path notes its via names. The pointers are
/// valid until either table next changes.
[[nodiscard]] std::vector<InfoEntry> info_order(const lef::LayerTable& layers, const lef::ViaTable& vias);

/// One entry of the magic-to-LEF layer map (LefMapping).
struct LefMapping {
  std::optional<std::string> name;          // lefName; nullopt = NULL
  bool has_record = false;                  // lefInfo != NULL
  bool via_class = false;                   // lefInfo->lefClass == CLASS_VIA
  std::optional<db::Coord> route_width;     // lefInfo->info.route.width (layer records)
};

/// defMakeInverseLayerMap(LAYER_MAP_VIAS): for every type, the first record
/// in LefInfo order of the type's class (cut for a contact, route or
/// masterslice otherwise) whose normal or obstruction type is the type
/// (defGetType; Q-LEFDEF-92).
class LayerMap {
 public:
  LayerMap(const std::vector<InfoEntry>& order, const tech::Technology& tech);

  [[nodiscard]] const LefMapping& at(tech::TileType type) const noexcept;
  [[nodiscard]] const std::optional<std::string>& name(tech::TileType type) const noexcept {
    return at(type).name;
  }

 private:
  std::vector<LefMapping> map_;
  LefMapping none_;
};

/// The state one write reads and the messages it prints, in order.
struct Context {
  const tech::Technology& tech;
  const db::CellDef& cell;
  float scale = 1.0F;  // s
  bool compat = true;  // magic-compat
  const drc::RuleSet* rules = nullptr;
  const mask::OutputStyle* style = nullptr;
  const extract::StyleView& extract_style;
  const LayerMap& map;
  std::vector<diag::Diagnostic>& messages;
  /// Reads the unread subcells `area` reaches (DefWriteOptions::read_area
  /// bound to the cell and the write's messages); empty: none is read.
  std::function<void(const db::Rect& area)> read_area;

  void say(std::string_view code, std::string text) const;
  void say_native(std::string_view code, std::string text) const;
  /// The electrically active types (ExtCurStyle->exts_activeTypes; empty
  /// with no extraction style, Q-LEFDEF-40).
  [[nodiscard]] bool active(tech::TileType type) const;
};

// ---- Sections (sections.cpp) ------------------------------------------

/// defWriteHeader, with DIEAREA.
void write_header(const Context& c, std::string_view tech_name, std::int32_t units, std::string& out);
/// The VIAS section: the records of `order` it writes (Q-LEFDEF-91), with
/// `created` the names registered by this write.
void write_vias(const Context& c, const std::vector<InfoEntry>& order, const lef::DefViaContext& via_context,
                const std::vector<std::string>& created, std::string& out);
/// The COMPONENTS section (defCountComponents, defWriteComponents).
void write_components(const Context& c, std::string& out);
/// The PINS section (defCountPins, defWritePins).
void write_pins(const Context& c, std::string& out);
/// defTransPos.
[[nodiscard]] std::string_view def_orientation(const db::Transform& t) noexcept;

// ---- Nets (nets.cpp, route.cpp) ----------------------------------------

/// The classification of every node of the netlist, in visit order
/// (defnodeCount's EF_SPECIAL flags).
struct NetPlan {
  std::vector<bool> special;  // EF_SPECIAL
  std::vector<bool> skip;     // a space-typed node (the substrate)
  std::int32_t special_count = 0;
  std::int32_t regular_count = 0;
};

/// The project's "global nets and power names" (Q-LEFDEF-102).
struct GlobalNames {
  std::vector<std::string> globals;
  std::optional<std::string> vdd;
  std::optional<std::string> gnd;
};

/// defCountNets: classifies the nodes, printing Magic's notes.
[[nodiscard]] NetPlan classify_nets(const Context& c, const extflat::Netlist& netlist, const GlobalNames& names,
                                    bool all_special);

/// The name components of a node's preferred name.
[[nodiscard]] std::vector<std::string> name_components(const extflat::FlatNode& node);
/// defHNsprintf: the components joined by `divider`, the last sanitized.
[[nodiscard]] std::string def_name(const std::vector<std::string>& components, char divider);

/// The non-default rules a nets pass selects and creates.
class RuleBook {
 public:
  /// `table` is the session's rule table (Q-LEFDEF-99 ON) or a write-local
  /// one (OFF).
  explicit RuleBook(lef::RuleTable& table) : table_(table) {}

  /// The rule `<lefname>_width_<n>`, created for `type` with width `width`
  /// when absent. Returns its name and its first entry's width.
  struct Selected {
    std::string name;
    db::Coord width = 0;
    friend bool operator==(const Selected&, const Selected&) = default;
  };
  Selected select(const Context& c, tech::TileType type, const std::string& lef_name, db::Coord width);

 private:
  lef::RuleTable& table_;
};

/// The via of each contact region, by position key (the defViaTable).
using ViaPositions = std::map<std::string, std::string>;

/// One SPECIALNETS or NETS section body (defWriteNets).
enum class NetMode : std::uint8_t { regular, special, all_special };
void write_nets(const Context& c, const extflat::Netlist& netlist, const NetPlan& plan, NetMode mode,
                const ViaPositions& vias, RuleBook& rules, std::string& out);

// ---- Rules and blockages (rules.cpp, blockages.cpp) ---------------------

/// The NONDEFAULTRULES section of `table` (Q-LEFDEF-98, 99, 100).
void write_rules(const Context& c, const lef::RuleTable& table, const std::vector<InfoEntry>& order,
                 const lef::LayerTable& layers, const lef::ViaTable& vias, bool anal, std::string& out);

/// The BLOCKAGES section (defWriteBlockages, Q-LEFDEF-101 fixed).
void write_blockages(const Context& c, const std::vector<InfoEntry>& order, const extflat::Netlist* netlist,
                     const NetPlan& plan, std::string& out);

/// DBTechNameType of a `.ext` layer name; nullopt when the technology has
/// no such type (Magic reads DBConnectTbl[-1]: fixed, the node's geometry
/// is not searched).
[[nodiscard]] std::optional<tech::TileType> node_type(const Context& c, std::string_view layer);

}  // namespace hefesto::def::write_detail
