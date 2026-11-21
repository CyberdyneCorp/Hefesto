// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private pieces of the LEF macro writer (capability lef-write; design D7),
// shared by the files of src/core/src/lef/write/:
// - MacroState: the state of one lefWriteMacro call (the flat copy, the
//   type sets, the LEF name map, the boundary, the yank buffer and the
//   output);
// - RecordWriter: lefWriteGeometry, the PORT and OBS records with their
//   LAYER lines (geometry.cpp);
// - the yank steps lefYankGeometry and lefYankContacts and the erase of a
//   pin's material from the flat copy (geometry.cpp);
// - the pin and obstruction passes (pins.cpp, obstruction.cpp).
//
// Owner: LEFW (design D2).

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/lef/tech/table.hpp"
#include "hefesto/lef/write/macro.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::lef::write_detail {

// lefWrite.c's indentation strings.
inline constexpr std::string_view kIn0 = "  ";
inline constexpr std::string_view kIn1 = "    ";
inline constexpr std::string_view kIn2 = "      ";
inline constexpr std::string_view kIn3 = "        ";

/// defMakeInverseLayerMap(LAYER_MAP_VIAS): the LEF record of each type, or
/// nullptr when the type has no LEF name.
using NameMap = std::array<const LayerRecord*, tech::kMaxTileTypes>;

[[nodiscard]] NameMap make_name_map(const tech::Technology& tech, const LayerTable& layers);

/// DBAllButSpaceAndDRCBits.
[[nodiscard]] tech::TypeMask all_but_space_and_drc();

/// DBResidueMask: a user contact's residues, a stacked type's component
/// contacts.
[[nodiscard]] tech::TypeMask residue_mask(const tech::Technology& tech, tech::TileType type);

/// lefYankGeometry / lefYankContacts and the erase, over one plane.
struct Yank {
  const tech::Technology& tech;
  const NameMap& names;
  const tech::TypeMask& route;
  bool magic_compat = true;
};

/// lefYankGeometry: copies the routing material of `source`'s tiles that
/// overlap `area` into `target` (plane `plane`): a non-contact type in R
/// with a LEF name, or each residue of a contact that is in R, has a LEF
/// name and lives on `plane`. With `clip` the copy is clipped to it
/// (Q-LEFDEF-38 OFF); otherwise whole tiles are copied.
void yank_routing(const Yank& y, const db::Plane& source, tech::PlaneId plane, const db::Rect& area,
                  db::Plane& target, const std::optional<db::Rect>& clip = std::nullopt);

/// Magic's tile client marks in the yank buffer (lefWriteGeometry marks
/// every tile it visits), by tile identity (db::TileView::tile). A paint
/// keeps a tile's identity where Magic's keeps its struct (and its mark):
/// a contact painted over exactly a routing tile's lower-left part changes
/// that tile in place (Q-LEFDEF-136).
class TileMarks {
 public:
  [[nodiscard]] bool test(const void* tile) const { return marked_.contains(tile); }
  void set(const void* tile) { marked_.insert(tile); }
  void reset(const void* tile) { marked_.erase(tile); }
  void clear() noexcept { marked_.clear(); }
  /// Drops the marks of the tiles of `plane` around `area` that a paint
  /// of `area` freed: `before` is what tiles() gave before the paint.
  /// A freed tile's storage can be reused by a later paint, so its mark
  /// must not outlive it (Magic's new tiles are never marked).
  void forget_freed(const std::vector<const void*>& before, const db::Plane& plane, const db::Rect& area);
  /// The tiles of `plane` overlapping `area` grown by one unit, every type.
  [[nodiscard]] static std::vector<const void*> tiles(const db::Plane& plane, const db::Rect& area);

 private:
  std::unordered_set<const void*> marked_;
};

/// lefYankContacts: copies the non-split contact tiles of `source`
/// overlapping `area` whose contact (or, for a stacked type, component
/// contact) has its home plane on `plane` and a LEF name. With `marks`
/// (magic-compat ON) the marks of the yank tiles each paint frees are
/// dropped.
void yank_contacts(const Yank& y, const db::Plane& source, tech::PlaneId plane, const db::Rect& area,
                   db::Plane& target, TileMarks* marks);

/// lefEraseGeometry: erases every tile of `selection` from `flat`, plane
/// `plane` only.
void erase_from(const Yank& y, const db::Plane& selection, tech::PlaneId plane, db::Plane& flat);

enum class Block : std::uint8_t { port, obstruction };
enum class Round : std::uint8_t { routing, cut };

/// lefWriteGeometry: the records of one PORT or OBS block.
class RecordWriter {
 public:
  RecordWriter(std::string& out, const NameMap& names, float oscale, LefUnits units, bool magic_compat)
      : out_(out), names_(names), oscale_(oscale), units_(units), magic_compat_(magic_compat) {}

  /// Starts a block: no record written yet, no current layer.
  void begin(Block block);
  /// The records written in the block.
  [[nodiscard]] int writes() const noexcept { return writes_; }

  /// The pending PIN header: called before the block's first record. A cut
  /// round never writes the header with magic-compat ON (Magic's search
  /// stops at the first cut and the header is left pending), so its cuts
  /// are dropped until a routing record has been written.
  std::function<void()> header;

  /// Writes the tiles of `yank` overlapping `area` whose type is in
  /// `route`, in tile order. A cut round writes only contact tiles. With
  /// magic-compat ON a tile is visited once per block (Magic's client
  /// marks), so a cut painted in place over a routing tile already written
  /// is not written (Q-LEFDEF-136), nor the second side of a split tile
  /// (Q-LEFDEF-39); with it OFF each cut is written once per block.
  void write_plane(const tech::Technology& tech, const db::Plane& yank, const tech::TypeMask& route,
                   const db::Rect& area, Round round);

  /// The block's tile marks (magic-compat ON), for yank_contacts; nullptr
  /// with magic-compat OFF.
  [[nodiscard]] TileMarks* marks() noexcept { return magic_compat_ ? &marks_ : nullptr; }

 private:
  bool record(const db::TileView& t);
  void shape(const db::TileView& t);
  /// One tile of write_plane with magic-compat ON (lefWriteGeometry).
  bool visit_marked(const tech::Technology& tech, const db::TileView& t, Round round);
  /// One tile of write_plane with magic-compat OFF.
  bool visit_native(const tech::Technology& tech, const db::TileView& t, Round round);

  std::string& out_;
  const NameMap& names_;
  float oscale_;
  LefUnits units_;
  bool magic_compat_;
  Block block_ = Block::port;
  int writes_ = 0;
  tech::TileType last_type_ = tech::kSpace;
  std::set<std::tuple<db::Coord, db::Coord, db::Coord, db::Coord, db::Coord>> written_cuts_;
  TileMarks marks_;
};

/// One pin rectangle remembered for the -hide 0 obstruction cut-out
/// (lefWrite.c's labelLinkedList).
struct Cutout {
  tech::TileType type = tech::kSpace;
  db::Rect area;
};

/// The state of one lefWriteMacro call.
struct MacroState {
  MacroState(const MacroContext& c, const db::CellDef& d, const MacroOptions& o) : ctx(c), cell(d), options(o) {}

  const MacroContext& ctx;
  const db::CellDef& cell;
  MacroOptions options;  // setback is -1 after the degenerate check
  db::CellLibrary scratch;
  db::CellDef* flat = nullptr;
  bool sets_ready = false;
  TypeSets sets;
  NameMap names{};
  db::Rect boundary;
  std::vector<Cutout> cutouts;
  std::string* out = nullptr;
  std::vector<diag::Diagnostic>* messages = nullptr;

  [[nodiscard]] const tech::Technology& tech() const { return *ctx.tech; }
  [[nodiscard]] Yank yank() const { return Yank{*ctx.tech, names, sets.route, ctx.magic_compat}; }
  [[nodiscard]] RecordWriter writer() const {
    return RecordWriter{*out, names, ctx.oscale, ctx.units, ctx.magic_compat};
  }
  void message(std::string_view code, std::string text, bool native = false) const;
};

/// A fresh scratch cell with the technology's planes (__lefYank__,
/// SelectDef).
[[nodiscard]] std::unique_ptr<db::CellDef> scratch_cell(const tech::Technology& tech, std::string name);

/// The pins, in port-index order (pins.cpp).
[[nodiscard]] diag::Status write_pins(MacroState& m);

/// The OBS block (obstruction.cpp).
[[nodiscard]] diag::Status write_obstructions(MacroState& m);

/// lefWritePinHeader's power test and the names-verbatim warning.
[[nodiscard]] bool breaks_lef_tokens(std::string_view name) noexcept;
void warn_name(const MacroState& m, std::string_view what, std::string_view name);

}  // namespace hefesto::lef::write_detail
