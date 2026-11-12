// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the CELL commands: a CellIoContext bound to one command's
// editor session, with automatic technology discovery bound to
// `load_technology` (cell-library-resolution "Automatic technology
// discovery") and Magic's keyword lookup.

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "hefesto/cell/io_safety.hpp"
#include "hefesto/cell/mag_io.hpp"
#include "hefesto/cmd/all_commands.hpp"
#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/context.hpp"
#include "hefesto/editor/editor_state.hpp"

namespace hefesto::cell::detail {

/// Magic's Lookup(): case-insensitive unique prefix, exact match wins; -1
/// when no entry or several match.
[[nodiscard]] int lookup(std::string_view word, std::span<const std::string_view> table);

[[nodiscard]] cmd::CommandResult fail(std::string_view code, std::string text);

/// Lives for one command; `io` refers to the editor's members.
class IoSession {
 public:
  IoSession(cmd::CommandContext& cmd_ctx, editor::EditorState& editor);
  IoSession(const IoSession&) = delete;
  IoSession& operator=(const IoSession&) = delete;
  IoSession(IoSession&&) = delete;
  IoSession& operator=(IoSession&&) = delete;
  ~IoSession() = default;

  /// The safe point after a command: pending timestamp mismatches.
  void process_mismatches();
  void emit(std::string_view code, std::string text) const;

  editor::EditorState& ed;
  CellSession& session;
  cmd::CommandContext& command;
  CellIoContext io;

 private:
  /// Steps 1-7 of "Automatic technology discovery" for technology `name`.
  CellIoContext::TechnologySwitch discover_technology(std::string_view name);
  void discover_libraries(const std::string& tech_dir);
  /// Step 3: `<dir>/<tech>.magicrc` through the project's interpreter after
  /// consent (owner: PDKINT, m4-scripting design D2/D7).
  enum class RcStep : std::uint8_t { none, ran, declined };
  RcStep run_technology_rc(const platform::Path& dir, const std::string& tech_name);
  /// The declined path (hefesto-security "Consent before implicit script
  /// execution"): the cell is read read-only with the `minimum` technology.
  CellIoContext::TechnologySwitch declined_technology() const;
};

/// Runs `body(IoSession&)` with the command's editor session, then the
/// deferred timestamp mismatches.
/// The windows' cell reader (view expansion): reads `def` from its file in a
/// session built from `ctx`, ignoring the technology as subcell expansion does.
bool read_for_expansion(cmd::CommandContext& ctx, db::CellDef& def);

template <typename Body>
cmd::CommandResult with_session(cmd::CommandContext& ctx, Body&& body) {
  auto editor = cmd::require_editor(ctx);
  if (!editor) {
    return cmd::CommandResult::failure(editor.error());
  }
  IoSession s{ctx, *editor.value()};
  editor.value()->windows.set_cell_reader(&read_for_expansion);
  cmd::CommandResult result = body(s);
  s.process_mismatches();
  return result;
}

/// "Loading a cell into a window": the current window shows `def`, which
/// becomes the edit cell (and the edit root, identity transform; recorded
/// for undo) unless it is read-only, when there is no edit cell.
void show_in_window(editor::EditorState& ed, db::CellDef& def);

/// Loads cell `name` (nullopt: "(UNNAMED)") into the current window as its
/// root and the edit cell (`load`, `xload`, `down`; dbwind/DBWprocs.c
/// DBWloadWindow).
/// `fail_if_missing` (`load -fail`): a cell neither in memory nor in a
/// file is not created and the window is unchanged.
[[nodiscard]] cmd::CommandResult load_into_window(IoSession& s, const std::optional<std::string>& name,
                                                  const ReadOptions& options, bool fail_if_missing = false);

/// `xload`, `edit`, `down` (edit_commands.cpp).
bool register_edit_commands(cmd::CommandRegistry& registry);

/// `flush` (flush_command.cpp).
bool register_flush_command(cmd::CommandRegistry& registry);

/// cmdFlushCell: clears `def` and reads it again from its file (dropping
/// its recorded path first with `dereference`); false, with Magic's
/// message, when `def` is the parent of the edit instance.
bool flush_cell(IoSession& s, db::CellDef& def, bool dereference);

/// `findlabel`, `goto` (find_commands.cpp) and `element` (element_command.cpp).
bool register_find_commands(cmd::CommandRegistry& registry);
bool register_element_command(cmd::CommandRegistry& registry);

/// `getcell` and `flatten` (place_commands.cpp): read what they need, then
/// run the MODEL handlers.
bool register_place_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::cell::detail
