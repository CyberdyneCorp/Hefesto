// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private parts of the `drc` command shared by its translation units:
// the parsed request (Magic's argc/argv after the list word), printing,
// the box window and the option handlers.
//   command.cpp          table, lookup, dispatch, help, registration
//   control_options.cpp  on off status catchup check euclidean style
//                        printrules rulestats statistics *halo *stepsize
//                        *flatcheck *showint
//   report_options.cpp   count find why ignore
//
// Owner: COMMANDS (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "db/command_util.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/db/geometry.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::drc::detail {

/// Diagnostic codes of the drc command's console text.
inline constexpr std::string_view kReport = "DRC-CMD-001";   // TxPrintf
inline constexpr std::string_view kWarning = "DRC-CMD-002";  // TxError

/// One `drc` call. `words` is Magic's argv[1..] after the list word
/// (words[0] is the option word), so Magic's argc is words.size() + 1.
struct Request {
  cmd::CommandContext& ctx;
  db::cmdutil::Session s;
  editor::EditorState& editor;
  std::vector<std::string> words;
  bool list = false;     // `drc list ...` or `drc listall ...` (dolist)
  bool listall = false;  // `drc listall ...` (doforall)
  std::size_t option = 0;

  [[nodiscard]] std::size_t argc() const noexcept { return words.size() + 1; }
  /// Magic's argv[i] for 1 <= i < argc().
  [[nodiscard]] const std::string& argv(std::size_t i) const { return words[i - 1]; }
  [[nodiscard]] bool magic_compat() const { return editor.drc.compat_settings().magic_compat; }
};

void print(Request& q, std::string text);
/// print() of a TxPrintf without a line end (Magic's console continues
/// the line with the next output).
void print_unterminated(Request& q, std::string text);
void warn(Request& q, std::string text);
/// Emits a failed status's diagnostic as Magic's TxError text: the command
/// still succeeds.
void report_failure(Request& q, const diag::Diagnostic& error);

/// The wrong-arguments text of the request's option (`badusage`).
[[nodiscard]] cmd::CommandResult bad_usage(Request& q);

/// A root cell and an area in its coordinates.
struct RootArea {
  db::CellDef* root = nullptr;
  db::Rect area;
};

/// ToolGetBoxWindow: the box and the root of a window showing the box's
/// root cell; nullopt without a box or such a window.
[[nodiscard]] std::optional<RootArea> box_window(const editor::EditorState& editor);
/// CmdDrc's `w ? w->w_surfaceArea : box window` (count, find): the current
/// window's root and visible area, else box_window().
[[nodiscard]] std::optional<RootArea> command_area(const editor::EditorState& editor);
/// ToolMoveBox(TOOL_BL) then ToolMoveCorner(TOOL_TR): the box becomes
/// `area` in `root` (two box events when the box stays in `root`).
void move_box(editor::EditorState& editor, db::CellDef* root, const db::Rect& area);

using OptionHandler = cmd::CommandResult (*)(Request&);

// control_options.cpp
cmd::CommandResult drc_flatcheck(Request& q);
cmd::CommandResult drc_halo(Request& q);
cmd::CommandResult drc_showint(Request& q);
cmd::CommandResult drc_stepsize(Request& q);
cmd::CommandResult drc_catchup(Request& q);
cmd::CommandResult drc_check(Request& q);
cmd::CommandResult drc_euclidean(Request& q);
cmd::CommandResult drc_off(Request& q);
cmd::CommandResult drc_on(Request& q);
cmd::CommandResult drc_status(Request& q);
cmd::CommandResult drc_style(Request& q);
cmd::CommandResult drc_printrules(Request& q);
cmd::CommandResult drc_rulestats(Request& q);
cmd::CommandResult drc_statistics(Request& q);

// report_options.cpp
cmd::CommandResult drc_count(Request& q);
cmd::CommandResult drc_find(Request& q);
cmd::CommandResult drc_ignore(Request& q);
cmd::CommandResult drc_why(Request& q);

// command.cpp
cmd::CommandResult drc_help(Request& q);

}  // namespace hefesto::drc::detail
