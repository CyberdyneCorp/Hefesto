// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Internal to the CELL commands: a CellIoContext bound to one command's
// editor session, with automatic technology discovery bound to
// `load_technology` (cell-library-resolution "Automatic technology
// discovery") and Magic's keyword lookup.

#pragma once

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
  const tech::Technology* discover_technology(std::string_view name);
  void discover_libraries(const std::string& tech_dir);
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
/// becomes the edit cell unless it is read-only.
void show_in_window(editor::EditorState& ed, db::CellDef& def);

/// `getcell` and `flatten` (place_commands.cpp): read what they need, then
/// run the MODEL handlers.
bool register_place_commands(cmd::CommandRegistry& registry);

}  // namespace hefesto::cell::detail
