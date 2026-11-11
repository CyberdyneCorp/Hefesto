// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The desktop menu bar (board docs/design/mockups/M4-Menu-Bar.dc.html;
// openspec/changes/m4-scripting/design.md D13; capabilities ui-gui-managers
// "Wrapper menus", pcell-toolkit "Toolkit menu hooks",
// scripting-pdk-integration "Wrapper widget operations in PDK menu
// callbacks"). One model, two renderings: Raven's MenuBar is the native
// NSMenu bar on macOS and a 30 px bar in the window on Linux.
//
// The menus are Magic's wrapper menus (gui::WrapperMenuModel) with the
// items PDK scripts inserted through intercepted widget calls
// (script::MenuModel: Import SPICE in File, DRC Routing in Drc), then,
// after a divider, the PDK's toolkit menus (pcell::ToolkitMenus: Devices 1,
// Devices 2) in creation order. Every item carries the Tcl script it runs
// in the project's interpreter, so each choice is a logged command.
//
// Toolkit-free and unit-tested.
//
// Owner: SHELL (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/gui/managers.hpp"
#include "hefesto/pcell/framework/menus.hpp"
#include "hefesto/script/pdk/widgets.hpp"
#include "hefesto/script/types.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::shell {

struct MenuBarItem {
  enum class Kind : std::uint8_t { command, check, separator };
  Kind kind = Kind::command;
  std::string label;        // Magic's label, double spaces collapsed
  std::string script;       // what choosing it runs (Tcl, in the project's interpreter)
  std::string accelerator;  // the key macro whose text equals the command ("d", "^L"); empty: none
  std::string variable;     // check items: the option variable (Opts(...), Winopts(...))
  bool checked = false;
  bool enabled = true;
  bool pdk = false;         // added by a PDK script (the "PDK" chip)
  std::string tooltip;      // disabled items: the milestone ("Read GDS: arrives in M5")

  /// The status-bar text and accessible description: "<label> · <command>".
  [[nodiscard]] std::string description() const;

  friend bool operator==(const MenuBarItem&, const MenuBarItem&) = default;
};

struct MenuBarMenu {
  std::string title;
  std::vector<MenuBarItem> items;
  bool pdk = false;    // a toolkit menu (after the divider, ember tint)
  std::string source;  // toolkit menus: the first row ("Devices 1 · pdk1")

  friend bool operator==(const MenuBarMenu&, const MenuBarMenu&) = default;
};

struct MenuBarView {
  std::vector<MenuBarMenu> menus;
  /// Index of the first toolkit menu (menus.size() when there is none).
  std::size_t divider = 0;

  friend bool operator==(const MenuBarView&, const MenuBarView&) = default;
};

/// What the bar shows besides the models.
struct MenuBarInputs {
  /// The window frame (".layout1"): toolkit menus of other frames are not shown.
  std::string frame = ".layout1";
  /// The current value of an option variable (check items); nullopt: unset (off).
  std::function<std::optional<bool>(std::string_view variable)> option;
  /// The project's PDK scripts were declined: no PDK items or menus.
  bool scripts_declined = false;
};

/// Magic's labels with runs of spaces collapsed ("Mirror   Up/Down" ->
/// "Mirror Up/Down").
[[nodiscard]] std::string collapse_spaces(std::string_view label);

/// The milestone that enables a wrapper command, for the items present but
/// disabled in M4 (Read/Write GDS: M5, Netlist Window: M12, 3D Display:
/// M10); nullopt for the others.
[[nodiscard]] std::optional<std::string_view> later_milestone(std::string_view command);

/// The bar for one layout window.
[[nodiscard]] MenuBarView menu_bar_view(const gui::WrapperMenuModel& wrapper, const script::MenuModel& pdk_menus,
                                        const pcell::ToolkitMenus& toolkit, const MenuBarInputs& inputs);

/// The bar of the session's layout window, the wrapper frame `.layout1`
/// (gui::adopt_wrapper): Magic's wrapper menus, the edits the PDK made
/// through the frame's WidgetInterceptor (ProjectInterpreter::menu_model),
/// the toolkit menus of every frame (the desktop has one layout window) and
/// the check items' values from the project interpreter.
[[nodiscard]] MenuBarView session_menu_bar(const editor::EditorState& editor, bool scripts_declined);

/// The script choosing `item` runs: a command item its command; a check
/// item first sets its variable to the toggled value (Tk's checkbutton
/// -variable), then runs its command.
[[nodiscard]] std::string menu_item_script(const MenuBarItem& item);

/// A signature of the view (rebuild the platform menus only when it changes).
[[nodiscard]] std::size_t menu_bar_signature(const MenuBarView& view);

/// One Diagnostics row per statement WidgetInterceptor skipped (board tile
/// 11): "▲ SCRIPT-PDK-004 skipped widget statement <file>:<line>", the
/// statement on the next line and, when it ran in a procedure, "in
/// <procedure> · evaluation continued" (without the leading `::`).
[[nodiscard]] std::string skipped_statement_row(const script::SkippedStatement& statement);

/// The status-bar message after a window opened with skipped statements:
/// "▲ 2 menu statements of mymenus.tcl were skipped"; empty for none.
[[nodiscard]] std::string skipped_statements_message(const std::vector<script::SkippedStatement>& statements);

}  // namespace hefesto::shell
