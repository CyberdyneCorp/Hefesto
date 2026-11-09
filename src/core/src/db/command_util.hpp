// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helpers shared by the MODEL commands (commands*.cpp): session
// access, coordinate units (`snap`), layer lists and argument parsing.

#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
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
  cmd::CommandContext* ctx = nullptr;
};

/// Prints a report line (stdout, Magic's TxPrintf) / a warning (stderr,
/// Magic's TxError) without failing the command.
void print(const Session& s, std::string text);
void warn(const Session& s, std::string text);

[[nodiscard]] diag::Outcome<Session> session(cmd::CommandContext& ctx);
[[nodiscard]] diag::Outcome<CellDef*> edit_cell(const Session& s);
[[nodiscard]] diag::Outcome<Rect> box(const Session& s);

/// Snap modes of the `snap` command; they decide the unit of coordinates
/// typed in and printed by `box` (EditorState::snap).
using Snap = editor::SnapMode;
[[nodiscard]] Snap snap_mode(const Session& s);
void set_snap_mode(const Session& s, Snap mode);
[[nodiscard]] std::string_view snap_name(Snap mode) noexcept;
/// Magic's DBWUnits as far as M1 needs it: `default` (un-suffixed values
/// follow the snap grid) until the deprecated `snap internal` switches it to
/// `internal` (cmd-coordinates-and-units, "Snap command"). Kept in the
/// session settings layer under "units" until the `units` command lands.
[[nodiscard]] bool units_internal(const Session& s);
void set_units_internal(const Session& s);

/// A coordinate typed by the user: an integer with an optional unit suffix
/// `i` (internal) or `l` (lambda); otherwise in the current snap unit.
[[nodiscard]] diag::Outcome<Coord> parse_coord(const Session& s, std::string_view text);
/// A coordinate printed in the current snap unit.
[[nodiscard]] cmd::Value coord_value(const Session& s, Coord value);

[[nodiscard]] diag::Outcome<std::int64_t> parse_int(std::string_view text);

/// cmdScaleCoord(w, text, relative, is_x, scale): the distance `text` in
/// internal units times `scale` (8 for label sizes and offsets), rounded half
/// away from zero. A word that is not a number prints Magic's "Coordinate
/// value cannot be parsed:  assuming 0" and yields 0.
/// (Magic ref: commands/CmdSubrs.c:123-300)
[[nodiscard]] std::int32_t scaled_coord(const Session& s, std::string_view text, int scale, bool is_x,
                                        bool relative = true);

/// Magic's StrIsInt (optional sign, then only digits), StrIsNumeric (strtod
/// reads a number at the start) and atoi (saturated at the int range).
[[nodiscard]] bool str_is_int(std::string_view text) noexcept;
[[nodiscard]] bool str_is_numeric(std::string_view text) noexcept;
[[nodiscard]] std::int32_t c_atoi(std::string_view text) noexcept;

/// The size a label gets when a font is given without one: DBLambda[1]
/// eighths (Q-MISC-22); with `magic-compat` OFF, 8 x that (one lambda).
[[nodiscard]] std::int32_t default_font_size(const Session& s) noexcept;

/// DBWLabelChanged: the label's area (its rendered text box for an outline
/// font) changed in `def`.
void label_changed(const Session& s, const CellDef& def, const Label& label);

/// DBEraseLabelsByContent(def, rect, -1, text): removes every label of
/// `def` with exactly `rect` and `text`, of any type. Returns how many.
std::size_t erase_labels_by_content(CellDef& def, const Rect& rect, std::string_view text);

/// `setlabel` (commands_setlabel.cpp).
[[nodiscard]] cmd::CommandResult setlabel_command(cmd::CommandContext& ctx, const cmd::Invocation& inv);

/// DBFontLabelSetBBox (font-labels "Font label geometry"): Label::text_bbox
/// of an outline-font label, nullopt for the default font or an unloaded
/// index; the rotation of an outline-font label is normalised into [0, 360)
/// (stored back, as Magic does).
void update_font_label(const Session& s, Label& label);

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

/// Magic's Lookup: exact match, else a unique prefix; the index, -1 when
/// ambiguous, -2 when nothing matches.
[[nodiscard]] int lookup(std::string_view word, std::initializer_list<std::string_view> choices);
[[nodiscard]] int lookup_in(std::string_view word, std::span<const std::string_view> choices);

/// Joins argument tokens from `first` with single spaces.
[[nodiscard]] std::string join_args(const std::vector<std::string>& args, std::size_t first);

bool add(cmd::CommandRegistry& registry, std::string name, std::string synopsis, std::string help,
         cmd::Handler handler);

// Command groups (commands.cpp, commands_labels.cpp, commands_cells.cpp).
bool register_paint_commands(cmd::CommandRegistry& registry);
bool register_label_commands(cmd::CommandRegistry& registry);
bool register_cell_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::db::cmdutil
