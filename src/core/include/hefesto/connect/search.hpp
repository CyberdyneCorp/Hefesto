// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Connectivity searches (capability layout-connectivity; design.md D6). Nets
// are derived on demand from the current geometry, never stored ("Connectivity
// is computed on demand"). `select net`, `select short`, `goto` and net
// highlighting use them; DEF and extraction tools reuse them later.
//   search_flat          one cell (DBSrConnect); with magic-compat ON it
//                        reproduces Magic's cross-plane single-tile step
//                        ("Flat search cross-plane quirk"), OFF does the
//                        full search
//   search_hierarchical  every descended cell in root coordinates
//                        (DBTreeCopyConnect), material under port labels of
//                        equal index in one definition is connected
// (Magic ref: database/DBconnect.c)
//
// Owner: SELECT (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "hefesto/connect/tree.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/db/plane.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::connect {

/// Where a search starts and what bounds it.
struct SearchStart {
  /// The start area: the first tile of `types` overlapping it starts the net.
  db::Rect area;
  /// The requested start types (Magic's mask argument).
  tech::TypeMask types;
  /// Tiles not overlapping `bounds` are neither reported nor traversed.
  db::Rect bounds = db::whole_plane();
};

/// Which labels a hierarchical search reports (Magic's SelectDoLabels):
/// none, every attached label (subcell labels get their instance path
/// prefix), or every attached label with its own text only ("simple").
enum class LabelPolicy : std::uint8_t { none, full, simple };

/// A connect table other than the technology's (Magic's `connect`
/// argument): the types `type` connects to. SelectRegion uses one in which
/// a type connects only to itself.
using ConnectTable = std::function<tech::TypeMask(tech::TileType type)>;

struct SearchOptions {
  bool magic_compat = true;
  /// Hierarchical only: the labels attached to the net.
  LabelPolicy labels = LabelPolicy::none;
  /// Hierarchical only: the expansion state (empty: descend everywhere).
  Descend descend;
  /// Empty: the technology's connects-to relation.
  ConnectTable connect;
};

/// One piece of a net. Flat: a tile of the searched cell (for a split tile,
/// `tile.type` and `tile.right_side` give the side reached). Hierarchical:
/// the piece added to the net in root coordinates, clipped to the bounds:
/// `tile.type` is the tile's type and a split tile keeps its transformed
/// diagonal and side (the other side's type is space).
struct NetTile {
  tech::PlaneId plane = 0;
  db::TileView tile;
  /// Hierarchical only: the instance path of the cell holding the tile
  /// (element names joined by '/', empty for the root).
  std::string use_path;
};

/// A label attached to the net (hierarchical searches with labels).
struct NetLabel {
  const db::Label* label = nullptr;
  /// The text as copied: the root's labels keep theirs, subcell labels get
  /// their hierarchical path prefix.
  std::string text;
  db::Rect area;  // root coordinates
  /// The label as copied: root rectangle, transformed justification,
  /// rotation and offset, and `text`.
  db::Label copy;
};

/// Receives the net. Return false to stop the search.
class NetVisitor {
 public:
  NetVisitor() = default;
  NetVisitor(const NetVisitor&) = default;
  NetVisitor& operator=(const NetVisitor&) = default;
  virtual ~NetVisitor() = default;

  virtual bool on_tile(const NetTile& tile) = 0;
  virtual bool on_label(const NetLabel&) { return true; }
};

/// "Connectivity within one cell", with the magic-compat quirk.
[[nodiscard]] diag::Status search_flat(const db::CellDef& def, const tech::Technology& tech,
                                       const SearchStart& start, const SearchOptions& options, NetVisitor& visit);

/// "Hierarchical connectivity" over `root` and every descended cell.
[[nodiscard]] diag::Status search_hierarchical(const db::CellDef& root, const tech::Technology& tech,
                                               const SearchStart& start, const SearchOptions& options,
                                               NetVisitor& visit);

}  // namespace hefesto::connect
