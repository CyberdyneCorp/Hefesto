// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers shared by the MODEL commands (commands*.cpp): session
// access, coordinate units (`snap`), layer lists and argument parsing.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::db::cmdutil {

/// Everything a MODEL command handler needs.
struct Session {
  editor::EditorState* editor = nullptr;
  const tech::Technology* tech = nullptr;
  bool magic_compat = true;
};

[[nodiscard]] diag::Outcome<Session> session(cmd::CommandContext& ctx);
[[nodiscard]] diag::Outcome<CellDef*> edit_cell(const Session& s);
[[nodiscard]] diag::Outcome<Rect> box(const Session& s);

/// Snap modes of the `snap` command; they decide the unit of coordinates
/// typed in and printed by `box` (EditorState::snap).
using Snap = editor::SnapMode;
[[nodiscard]] Snap snap_mode(const Session& s);
void set_snap_mode(const Session& s, Snap mode);
[[nodiscard]] std::string_view snap_name(Snap mode) noexcept;

/// A coordinate typed by the user: an integer with an optional unit suffix
/// `i` (internal) or `l` (lambda); otherwise in the current snap unit.
[[nodiscard]] diag::Outcome<Coord> parse_coord(const Session& s, std::string_view text);
/// A coordinate printed in the current snap unit.
[[nodiscard]] cmd::Value coord_value(const Session& s, Coord value);

[[nodiscard]] diag::Outcome<std::int64_t> parse_int(std::string_view text);

/// A comma/space separated layer list ("*" = every user type, "labels" =
/// the label pseudo-type); DB-CMD-003 on an unknown or ambiguous name.
[[nodiscard]] diag::Outcome<tech::TypeMask> parse_layers(const tech::Technology& tech, std::string_view text);
/// A single layer name.
[[nodiscard]] diag::Outcome<tech::TileType> parse_layer(const tech::Technology& tech, std::string_view text);

[[nodiscard]] diag::Diagnostic bad_argument(std::string text);
[[nodiscard]] cmd::CommandResult fail(const diag::Diagnostic& d);

/// The use the last `getcell` placed in the edit cell (Magic selects it);
/// stands in for the selection until cmd-selection lands. Kept in the
/// session settings layer under "model.current-use".
[[nodiscard]] CellUse* current_use(const Session& s);
void set_current_use(const Session& s, const CellUse* use);

/// Joins argument tokens from `first` with single spaces.
[[nodiscard]] std::string join_args(const std::vector<std::string>& args, std::size_t first);

bool add(cmd::CommandRegistry& registry, std::string name, std::string synopsis, std::string help,
         cmd::Handler handler);

// Command groups (commands.cpp, commands_labels.cpp, commands_cells.cpp).
bool register_paint_commands(cmd::CommandRegistry& registry);
bool register_label_commands(cmd::CommandRegistry& registry);
bool register_cell_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::db::cmdutil
