// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Place device and the PCell parameter dialog (board
// docs/design/mockups/M4-PCell-Dialog.dc.html; openspec/changes/
// m4-scripting/design.md D9/D13; capabilities hefesto-app-ux "Functional
// parity with Magic's GUI managers" scenario "PCell dialog", pcell-toolkit
// "Parameter dialog structure", "Toolkit menu hooks").
//
// Place device is a rail button that opens a popover of the PDK's
// device-menu items (pcell::ToolkitMenus, grouped by menu, with a filter).
// The dialog renders pcell::DialogModel (editor::EditorState::script.
// gencell.dialog), which the device's `_dialog` procedure filled through
// the builder API. Every action is a Tcl script run in the project's
// interpreter: a value edit sets `magic::<pname>_val` and runs the widget's
// handlers (the bounds check and add_dependency's update_dialog); Create
// issues `magic::gencell_create <type> <lib> {k v ...}` (or `magic::gencell
// <lib>::<type> <name> k v ...` for a named instance) and Apply/Okay
// `magic::gencell <lib>::<type> <instance> k v ...`, so placing and changing a device are
// logged, replayable commands.
//
// Toolkit-free and unit-tested.
//
// Owner: SHELL (design D2); the DialogModel is PCELL's.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/pcell/framework/dialog.hpp"
#include "hefesto/pcell/framework/menus.hpp"
#include "hefesto/script/types.hpp"
#include "hefesto/shell/view_models.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::shell {

// --- Place device -----------------------------------------------------------------

struct PlaceDeviceEntry {
  std::string label;   // the PDK's item text ("nmos (MOSFET)"); a group: the menu title
  std::string script;  // what choosing it runs; empty for a group heading
  bool heading = false;
  bool draws = false;  // a `<ns>::*_draw` item: paints at the box, no dialog
  bool recent = false;

  friend bool operator==(const PlaceDeviceEntry&, const PlaceDeviceEntry&) = default;
};

struct PlaceDeviceView {
  bool enabled = false;
  std::string tooltip;  // "Place device: ..." or "No PDK device scripts"
  std::string title;    // "Place device · sky130A"
  std::vector<PlaceDeviceEntry> entries;

  friend bool operator==(const PlaceDeviceView&, const PlaceDeviceView&) = default;
};

/// The popover: every toolkit menu as a heading and its command items whose
/// label contains `filter` (case-insensitive), then the last device used
/// (`recent`, a script chosen before) as "Recent: <label>". Disabled with
/// "No PDK device scripts" when there is no toolkit menu or the scripts
/// were declined.
[[nodiscard]] PlaceDeviceView place_device_view(const pcell::ToolkitMenus& menus, std::string_view pdk_name,
                                                bool scripts_declined, std::string_view filter = {},
                                                std::string_view recent = {});

// --- parameter dialog ---------------------------------------------------------------

struct PCellRow {
  pcell::WidgetKind kind = pcell::WidgetKind::entry;
  std::string parameter;
  std::string label;
  std::string value;
  std::vector<std::string> choices;
  std::optional<std::size_t> choice;  // the selected choice (select lists)
  bool checked = false;               // checkboxes
  /// The value differs from what the user typed: _check corrected it (a
  /// warning outline, and `typed` below the field).
  bool corrected = false;
  std::string typed;
  /// A dependency callback changed this value (polite announcement).
  bool updated = false;
  std::string color;  // message rows: the PDK's colour name

  friend bool operator==(const PCellRow&, const PCellRow&) = default;
};

enum class PCellButtonRole : std::uint8_t { create, create_and_close, apply, okay, reset, close };

struct PCellButton {
  std::string label;
  PCellButtonRole role = PCellButtonRole::close;
  bool primary = false;  // the default (Return): Create and Close, Okay
  bool closes = false;

  friend bool operator==(const PCellButton&, const PCellButton&) = default;
};

struct PCellDialogView {
  std::string mode;  // "New device:" / "Edit device:"
  std::string device;
  std::string library;
  std::string cell_name;
  std::string instance;
  StatusChip generator;  // "● native generator" / "◆ Tcl generator"
  std::vector<PCellRow> rows;
  std::string banner;  // "▲ <_check messages>"; empty: none
  std::vector<PCellButton> buttons;
  /// What a screen reader is told after the last edit ("Value updated to 2.94k").
  std::string announcement;

  friend bool operator==(const PCellDialogView&, const PCellDialogView&) = default;
};

/// What the shell remembers between edits: what the user last typed per
/// parameter, and the values before that edit (to find dependency updates).
struct PCellEditState {
  std::map<std::string, std::string, std::less<>> typed;
  std::map<std::string, std::string, std::less<>> before;
  std::string edited;  // the parameter of the last edit

  /// Called before an edit of `parameter` to `value` is run.
  void begin_edit(const pcell::DialogModel& model, std::string_view parameter, std::string_view value);
  /// The user started typing into `parameter` again: what was last typed
  /// into it is forgotten, so a correction shown on it (warning outline,
  /// "you typed") goes. True when something was forgotten.
  bool begin_typing(std::string_view parameter);
  /// The dialog was rebuilt for another device or instance.
  void clear() noexcept;
};

/// Tcl's boolean truth of a checkbox value ("1", "true", "yes", "on").
[[nodiscard]] bool tcl_true(std::string_view value) noexcept;

/// The view of `model`; `native` tells whether a native generator of this
/// exact PDK version draws the device.
[[nodiscard]] PCellDialogView pcell_dialog_view(const pcell::DialogModel& model, bool native,
                                                const PCellEditState& edits = {});

/// The script a value edit runs: `set ::magic::<pname>_val <value>` and the
/// widget's handlers, in order (Return / focus-out of an entry, a click on
/// a checkbox or choice). A checkbox takes "1"/"0" (Tk's on and off values),
/// a select-index widget its 0-based index.
[[nodiscard]] std::string pcell_value_script(const pcell::DialogWidget& widget, std::string_view value);

/// The command that creates or changes the device with every parameter of
/// the dialog, Tcl-quoted: `magic::gencell <lib>::<type> <instance> k v ...`
/// (Magic's create-with-name and change forms). A new device with the
/// "(default)" instance name is `magic::gencell_create <type> <lib> {k v
/// ...}`, because Magic's `gencell <lib>::<type> {} k v ...` opens the
/// dialog instead of creating (toolkit.tcl `magic::gencell`).
[[nodiscard]] std::string pcell_gencell_command(const pcell::DialogModel& model);

/// The script of a button (empty for Close): Create runs the create
/// command and reopens the dialog on the new instance (`magic::gencell_dialog
/// [<create command>] <type> <lib> {}`), Create and Close / Apply / Okay
/// run the gencell command, Reset runs the model's Reset script.
[[nodiscard]] std::string pcell_button_script(const pcell::DialogModel& model, PCellButtonRole role);

/// The PDK version of the loaded technology: the identity of the
/// `libs.tech/magic` directory its file was read from (nullopt otherwise).
[[nodiscard]] std::optional<script::PdkIdentity> technology_pdk(const tech::Technology& technology);

/// True when a native generator is registered for exactly `pdk` and `device`.
[[nodiscard]] bool native_generator_available(const std::optional<script::PdkIdentity>& pdk, std::string_view device);

}  // namespace hefesto::shell
