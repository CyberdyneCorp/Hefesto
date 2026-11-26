// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The xschem-style default key bindings of the desktop schematic editor and
// their rebinding (capability schematic-editing "Desktop and touch
// interaction", Q-SCHEM-24 `Resolution: platform`: desktop bindings stay
// xschem-style by default and rebindable; hefesto-input-and-touch;
// openspec/changes/m8-schematic design D15).
//
// The binding set is the macro table `schematic` of the editor's
// ui::MacroTables (EditorState::macros; SchematicKeyMap::attach), so the
// key-binding sheet (key_bindings_sheet.hpp) edits it unchanged: its
// `macro schematic KEY "TEXT"` lines run through the core's `macro`
// command, whose `schematic` client selects this table (M8 round 6), and
// Reset table restores the defaults below. A map not attached to an editor
// keeps a table of its own (tests).
//
// A binding's text is either an editor action (one word of
// kSchematicActions: an interactive tool or a panel the editor opens) or a
// command line issued through the window's CommandSink (`schematic rotate`).
// The defaults are the keys the spec names: w wire, m move, c copy,
// R rotate, F flip, e descend, Ctrl+e ascend, q edit properties,
// k highlight, u undo, U redo, Insert place symbol, n netlist; Delete
// deletes the selection (Hefesto addition, reported).
//
// Raven-free and unit-tested in tests/unit/shell/schematic_bindings_test.cpp.
// Owner: SHELL (openspec/changes/m8-schematic design D3, D15; task 13.2).

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/ui/input.hpp"
#include "hefesto/ui/macros.hpp"

namespace hefesto::shell {

/// The binding table's name in the key-binding sheet.
inline constexpr std::string_view kSchematicTable = "schematic";

/// The editor actions a binding may name instead of a command line.
inline constexpr std::array<std::string_view, 6> kSchematicActions{"wire",       "move",  "copy",
                                                                   "properties", "place", "descend"};

struct DefaultBinding {
  std::string_view key;   // a macro key name ("w", "R", "Control_e", "Insert")
  std::string_view text;  // the action or command line
  std::string_view help;  // shown in the sheet
};

/// The xschem-style defaults, in the sheet's order.
[[nodiscard]] const std::vector<DefaultBinding>& schematic_default_bindings();

/// Defines the defaults in the `schematic` table of `tables` (replacing
/// what the table binds to those keys).
void install_schematic_defaults(::hefesto::ui::MacroTables& tables);

/// What a key press does in the schematic canvas.
struct BoundAction {
  std::string action;  // one of kSchematicActions, or empty
  std::string line;    // the command line to issue, or empty
};

class SchematicKeyMap {
 public:
  /// A table of its own, with the defaults.
  SchematicKeyMap();
  SchematicKeyMap(const SchematicKeyMap&) = delete;
  SchematicKeyMap& operator=(const SchematicKeyMap&) = delete;
  ~SchematicKeyMap() = default;

  /// Reads the bindings from `tables` (the editor's macro tables, which the
  /// core's `macro schematic ...` edits) from now on; adds each default
  /// whose key the `schematic` table does not bind yet (a binding a startup
  /// file made stays). `tables` must outlive the map.
  void attach(::hefesto::ui::MacroTables& tables);

  [[nodiscard]] const ::hefesto::ui::MacroTables& tables() const noexcept { return *tables_; }

  /// The binding of `key`; nullopt when the table does not bind it.
  [[nodiscard]] std::optional<BoundAction> lookup(const ::hefesto::ui::Key& key) const;
  /// The binding of a key event (`macos`: Command and Option are Meta).
  [[nodiscard]] std::optional<BoundAction> lookup(const ::hefesto::ui::KeyEvent& event, bool macos) const;

  /// The sheet's Reset table for `schematic`: the lines that restore the defaults.
  [[nodiscard]] std::vector<std::string> reset_commands() const;

 private:
  ::hefesto::ui::MacroTables own_;
  ::hefesto::ui::MacroTables* tables_ = &own_;
};

}  // namespace hefesto::shell
