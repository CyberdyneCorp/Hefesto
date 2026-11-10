// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The default key and button bindings (capability ui-tools-and-input,
// requirement "Default key bindings"; ui-gui-managers "Tool modes"; design.md
// D7): the "new macro" set of Magic's system startup file for its Tcl build
// (magic/proto.magicrc.in with MAGIC_WRAPPER and XLIB, without readline),
// then the tool setup of tools.tcl (magic::enable_tools: `>`/`<`, space and
// Shift-space, the wiring/nettool/pick tables copied from the box table and
// their button overrides, `+`/`-` auto-increment). Installed at startup as
// the system startup file would be, so no Tcl is needed.
//
// Hefesto-native forms of Tcl-only texts: `magic::tool` is `tool`,
// `magic::pushstack [cellname list self]` is `pushstack` (which selects the
// cell under the box itself), and the tools.tcl procs
// `keepselect`, `startselect`, `cancelselect` are native commands taking the
// window as `%W` (substituted by the front end, see ui::substitute_window);
// the wiring tool's buttons are the wiring area's (`trackwire`).
//
// Owner: DISPATCH (openspec/changes/m2-interactive-editing/design.md D2).

#pragma once

#include <span>
#include <string>
#include <string_view>

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::ui {

/// One line of the startup macro set: `macro [table] key text` (or
/// `imacro`), or `macro copy NAME` when `copy_to` is set, or a switch of the
/// current tool (`tool NAME`) when `tool` is set.
struct StartupBinding {
  std::string_view table;  // empty: the current tool's table
  std::string_view key;
  std::string_view text;
  bool interactive = false;
  std::string_view copy_to;  // `macro copy`
  std::string_view tool;     // `tool NAME`
  /// The wiring tool's button bindings (wiring::tool_bindings(), the
  /// wiring area's `trackwire` and `wire` texts) into the `wiring` table.
  bool wiring_bindings = false;
};

/// The startup lines in the order they run.
[[nodiscard]] std::span<const StartupBinding> startup_bindings() noexcept;

/// Registers the built-in tools (box current) and installs the startup
/// bindings. Leaves the box tool current.
void install_defaults(editor::EditorState& editor);
/// install_defaults() when no tool is registered yet (the first command that
/// needs tools or macros in a session that did not install them).
void ensure_defaults(editor::EditorState& editor);

/// Replaces every `%W` in a macro text with the window's name (Tk's window
/// path substitution in the tools.tcl bindings).
[[nodiscard]] std::string substitute_window(std::string_view text, std::string_view window);

}  // namespace hefesto::ui
