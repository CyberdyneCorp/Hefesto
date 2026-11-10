// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers shared by the view command handlers (capability
// ui-windows-and-views): window lookup, Magic-style messages, coordinate
// parsing and printing, the box as seen from a window.
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/view/window.hpp"

namespace hefesto::view::detail {

/// The session and the window a command applies to.
struct Target {
  editor::EditorState& editor;
  LayoutWindow& window;
};

[[nodiscard]] cmd::CommandResult fail(std::string_view code, std::string text);
/// The attached session, or CMD-SESSION-001 (cmd::require_editor's contract).
[[nodiscard]] diag::Outcome<editor::EditorState*> session(const cmd::CommandContext& ctx);
/// The current window, or VIEW-WINDOW-003 with `no_window_text`.
[[nodiscard]] diag::Outcome<Target> pointed(const cmd::CommandContext& ctx,
                                            std::string_view no_window_text = "Point to a window first.");
/// Prints a note (Magic's TxPrintf: stdout) without failing the command.
void print(cmd::CommandContext& ctx, std::string text);
/// Converts a failed lookup into a command result.
[[nodiscard]] cmd::CommandResult failure(const diag::Diagnostic& d);

/// A coordinate in the unit rules of M1: a number in lambda (no suffix or
/// `l`) or internal units (`i`), converted with the session grid scale and
/// rounded half away from zero. Full coordinate units (microns, grid
/// multiples) belong to cmd-coordinates-and-units.
[[nodiscard]] std::optional<db::Coord> parse_coord(std::string_view text, const tech::GridScale& grid);
/// A coordinate printed in lambda (integral when exact).
[[nodiscard]] std::string format_coord(db::Coord value, const tech::GridScale& grid);
/// "(llx, lly) to (urx, ury)".
[[nodiscard]] std::string format_rect(const db::Rect& r, const tech::GridScale& grid);
/// strtod over the whole word (sscanf "%f" semantics when `prefix_ok`).
[[nodiscard]] std::optional<double> parse_number(std::string_view text, bool prefix_ok = false);
/// Magic's StrIsInt + atoi.
[[nodiscard]] std::optional<std::int64_t> parse_int(std::string_view text);
/// Magic's Lookup (cmd::lookup: case-insensitive, exact match wins, then a
/// unique prefix) as an index: -1 when ambiguous, -2 when none.
[[nodiscard]] int lookup(std::string_view word, std::initializer_list<std::string_view> choices);
[[nodiscard]] int lookup_nocase(std::string_view word, std::span<const std::string_view> choices);

/// The coordinate units of the session for cmd::parse_coordinate: grid
/// scale, default unit (`units`, else `snap`), the CIF output scale and the
/// current window's grid.
[[nodiscard]] cmd::CoordinateUnits coordinate_units(const editor::EditorState& editor);

/// The box in the window's root coordinates. The session keeps the box in
/// edit-cell coordinates, so the box belongs to the window when the edit
/// cell is the window's root.
[[nodiscard]] std::optional<db::Rect> box_in_window(const editor::EditorState& editor, const LayoutWindow& window);
/// Recomputes every window caption from the edit cell ("Window captions").
void refresh_captions(editor::EditorState& editor);

/// Registers one handler; false when the name is taken.
bool add(cmd::CommandRegistry& registry, std::string name, std::string synopsis, std::string help, cmd::Handler handler);

// Registration of the handler groups (one translation unit each).
bool register_view_ops(cmd::CommandRegistry& registry);    // xview scroll center findbox
bool register_view_zoom(cmd::CommandRegistry& registry);   // view zoom
bool register_window_ops(cmd::CommandRegistry& registry);  // openwindow closewindow window* over underneath crosshair
bool register_display_ops(cmd::CommandRegistry& registry); // expand unexpand see grid redraw reset updatedisplay
bool register_wizard_commands(cmd::CommandRegistry& registry);  // *watch *coord *winddebug *files
bool register_feedback_command(cmd::CommandRegistry& registry);  // feedback

}  // namespace hefesto::view::detail
