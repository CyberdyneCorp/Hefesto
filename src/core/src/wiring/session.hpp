// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private: the editor-facing operations of the wiring tool shared by `wire`
// and `trackwire` (WirePickType, WireAddLeg, WireShowLeg, WireAddContact of
// Magic's wiring/wireOps.c), and the session helpers they need. Each
// operation reports Magic's messages through the command context and
// returns a failed status on the errors that stop it. Owner: WIRE.

#pragma once

#include <optional>
#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/wiring/wiring.hpp"

namespace hefesto::wiring {

/// Everything an operation needs.
struct WireSession {
  cmd::CommandContext* ctx = nullptr;
  editor::EditorState* ed = nullptr;
  const tech::Technology* tech = nullptr;
  bool magic_compat = true;

  /// TxPrintf text (WIRE-REPORT-001) / a non-failing TxError (WIRE-REPORT-002).
  void report(std::string text) const;
  void emit(std::string_view code, std::string text) const;

  /// Magic's EditRootDef (the edit cell when it is its own root); nullptr
  /// without an edit cell.
  [[nodiscard]] db::CellDef* edit_root() const;
  /// The cell the box belongs to (EditorState::box_root, else the edit cell).
  [[nodiscard]] db::CellDef* box_root() const;

  /// The units of typed coordinates and printed values.
  [[nodiscard]] cmd::CoordinateUnits units() const;
  /// cmdParseCoord (internal units).
  [[nodiscard]] db::Coord parse_coord(std::string_view text, bool relative, bool is_x) const;
  /// DBWPrintValue for a length.
  [[nodiscard]] std::string format_length(db::Coord value) const;

  /// The current `wiring` section, scaled to the session grid.
  [[nodiscard]] WiringSection section() const;
};

/// The session of a `wire`/`trackwire` handler; CMD-USAGE-001 without an
/// editor.
[[nodiscard]] diag::Outcome<WireSession> wire_session(cmd::CommandContext& ctx);

/// A failed result carrying `code` and `text`.
[[nodiscard]] cmd::CommandResult fail(std::string_view code, std::string text);

/// The cursor (EditorState::command_point) in a layout window, root
/// coordinates of that window; nullopt without a pointer in a window.
struct Cursor {
  const view::LayoutWindow* window = nullptr;
  db::Point point;
  db::Rect area;  // the area under the pixel, at least 1 x 1 (ToolGetPoint)
};
[[nodiscard]] std::optional<Cursor> cursor(const editor::EditorState& ed);

/// WirePickType with an explicit type (or no type) and width.
void pick_type(const WireSession& s, std::optional<tech::TileType> type, db::Coord width);
/// WirePickType from the material under `root_point` (edit root
/// coordinates), or under the cursor when nullopt.
[[nodiscard]] cmd::CommandResult pick_type_at(const WireSession& s, std::optional<db::Point> root_point);
/// WireAddLeg towards `root_point`, or the cursor when nullopt.
[[nodiscard]] cmd::CommandResult add_leg(const WireSession& s, std::optional<db::Point> root_point, LegMode mode);
/// WireShowLeg (silent; never fails).
void show_leg(const WireSession& s);
/// WireAddContact to `type` with `width`, or to the material under the
/// cursor when nullopt.
[[nodiscard]] cmd::CommandResult add_contact(const WireSession& s, std::optional<tech::TileType> type,
                                             db::Coord width);
/// SelectClear.
void clear_selection(const WireSession& s);

}  // namespace hefesto::wiring
