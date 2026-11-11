// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// pcell::DialogModel: the platform-neutral parameter dialog
// (openspec/changes/m4-scripting/design.md D4/D9; capability pcell-toolkit
// "Parameter dialog structure", "Dialog builder API", "Dialog dependencies
// and validation", "Selection-driven dialog refresh and key binding"). The
// builder commands (`magic::add_entry`, `add_checkbox`, `add_selectlist`,
// `add_selectindex`, `add_message`, `add_dependency`) fill it; SHELL
// renders it on the desktop (and the iPad shell in M14).
//
// Commands only: each widget lists the Tcl scripts its edits run (Magic's
// <Return>/<FocusOut> bindings and -command options) and each button its
// script, so every dialog action is a logged, replayable command run
// through the project's interpreter. The value of a parameter widget is
// the script variable `magic::<pname>_val` (Tk's -textvariable); `value`
// mirrors it after every framework call.
//
// Owner: PCELL (design D2); the model is a shared interface read by SHELL.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/pcell/framework/parameters.hpp"

namespace hefesto::pcell {

enum class WidgetKind : std::uint8_t {
  entry,        // add_entry:       <pname>_ent
  checkbox,     // add_checkbox:    <pname>_chk
  selectlist,   // add_selectlist:  <pname>_sel, value = the chosen item
  selectindex,  // add_selectindex: <pname>_sel, value = the 0-based index
  message,      // add_message:     <pname>_txt, read-only
};

/// One row of the parameter area, in grid order.
struct DialogWidget {
  WidgetKind kind = WidgetKind::entry;
  std::string parameter;             // pname
  std::string label;                 // text in column 0
  std::string value;                 // magic::<pname>_val
  std::vector<std::string> choices;  // selectlist / selectindex items
  std::string color;                 // message colour (default "blue")
  /// Scripts run, in order, when the user changes the value: on Return and
  /// focus-out for an entry, on change for a checkbox or choice.
  std::vector<std::string> handlers;

  friend bool operator==(const DialogWidget&, const DialogWidget&) = default;
};

/// add_dependency: `callback` runs (through magic::update_dialog) when one
/// of `parameters` changes; empty `parameters` = every parameter of the
/// device's _defaults (Q-SCRIPT-25).
struct DialogDependency {
  std::string callback;
  std::string device;   // type
  std::string library;  // lib
  std::vector<std::string> parameters;

  friend bool operator==(const DialogDependency&, const DialogDependency&) = default;
};

/// A button of the button row: `script` runs in the interpreter, then the
/// dialog closes when `closes` (Magic's `destroy .params`).
struct DialogButton {
  std::string text;    // Create, Create and Close, Apply, Okay, Reset, Close
  std::string script;  // empty for Close
  bool closes = false;

  friend bool operator==(const DialogButton&, const DialogButton&) = default;
};

enum class DialogMode : std::uint8_t { new_device, edit_device };

struct DialogModel {
  DialogMode mode = DialogMode::new_device;
  std::string device;     // device type
  std::string library;    // Tcl namespace of the generator (e.g. "sky130")
  std::string cell_name;  // "(default)" for a new device
  std::string instance;   // the editable instance-name entry; "(default)" for a new device
  std::vector<DialogWidget> widgets;
  std::vector<DialogDependency> dependencies;
  std::vector<DialogButton> buttons;
  /// The last validation messages of `_check` (shown by the shell).
  std::vector<std::string> messages;
  /// Incremented by every `raise .params` (Tk's raise; also the last step
  /// of magic::gencell_dialog and the ^P macro): the shell brings the
  /// dialog to the front and focuses it whenever the count changes.
  std::uint32_t raise_count = 0;

  /// The values of every entry, checkbox and select widget in grid order
  /// (magic::gencell_getparams reads the script variables instead).
  [[nodiscard]] ParameterDict parameters() const;
  /// The widget of parameter `pname` with a value (not a message), or nullptr.
  [[nodiscard]] DialogWidget* find(std::string_view pname);

  friend bool operator==(const DialogModel&, const DialogModel&) = default;
};

/// "Edit device:" / "New device:".
[[nodiscard]] std::string_view mode_label(DialogMode mode) noexcept;

}  // namespace hefesto::pcell
