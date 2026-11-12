// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Widget interception for PDK menu callbacks (openspec/changes/m4-scripting/
// design.md D4/D7; capability scripting-pdk-integration "Wrapper widget
// operations in PDK menu callbacks"; Q-SCRIPT-34, Q-SCRIPT-42, D-32).
// WidgetInterceptor defines Tcl commands named by the window frame paths
// the binding announces (`.layoutN.titlebar...`): `cget -height` errors,
// `...drc.toolmenu add` / `...file.toolmenu insert` edit the MenuModel,
// `tk_getOpenFile` asks platform::FilePicker; every other statement that
// addresses a Tk widget path (through `unknown`) or a Tk command (`winfo`,
// `wm`, `bind`, `pack`, ...) is skipped with one SCRIPT-PDK-004 diagnostic
// naming the script file, line and statement, and evaluation continues.
//
// Magic's File menu (tcltk/wrapper.tcl:1226-1244) is the frame of
// reference of `insert <index>`: the first insert into the File menu seeds
// the model with Magic's entries (MenuItem::native), so the PDK items keep
// the position they have in Magic. A menu only appended to (Drc) holds the
// appended items, which follow the native entries.
//
// Owner: PDKINT (design D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_picker.hpp"
#include "hefesto/script/events.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::script {

/// One native menu entry produced by an intercepted widget call.
struct MenuItem {
  enum class Kind : std::uint8_t { command, separator };
  Kind kind = Kind::command;
  std::string label;
  std::string command;  // the Tcl script the item runs (issued as a logged command)
  /// An entry of Magic's own wrapper menu (rendered from the shell's
  /// gui::WrapperMenuModel); false for the items PDK scripts added.
  bool native = false;

  friend bool operator==(const MenuItem&, const MenuItem&) = default;
};

/// The menus PDK scripts added or changed, keyed by menu name ("File",
/// "Drc", "Devices", ...), each in item order.
class MenuModel {
 public:
  void append(std::string_view menu, MenuItem item);
  /// Inserts at `index` (clamped to the end), as Tk's `insert`.
  void insert(std::string_view menu, std::size_t index, MenuItem item);
  [[nodiscard]] const std::vector<MenuItem>& items(std::string_view menu) const;
  /// Menu names in first-use order.
  [[nodiscard]] const std::vector<std::string>& menus() const noexcept { return order_; }
  void clear() noexcept;

 private:
  std::vector<MenuItem>& menu(std::string_view name);
  std::vector<std::string> order_;
  std::vector<std::vector<MenuItem>> items_;  // parallel to order_
};

class WidgetInterceptor {
 public:
  WidgetInterceptor(MenuModel& menus, platform::FilePicker& picker, ScriptEvents& events);

  /// Defines the interception commands for the window frame `frame_path`
  /// (".layout1") in `interp`: `<frame>.titlebar` and the File and DRC
  /// menus of `<frame>` and `<frame>.pane.top`; the first call also defines
  /// `tk_getOpenFile`, the Tk command skippers and the `unknown` chain for
  /// other widget paths (an existing `unknown` is kept as the next handler).
  /// The interceptor must outlive `interp` (it is a member of the same
  /// editor::EditorState::script in a session).
  [[nodiscard]] cmd::CommandResult install(Interpreter& interp, std::string_view frame_path);

  /// Where SCRIPT-PDK-004 diagnostics go besides ScriptEvents (nullptr: none).
  void set_sink(diag::DiagnosticSink* sink) noexcept { sink_ = sink; }

  [[nodiscard]] MenuModel& menus() noexcept { return menus_; }
  [[nodiscard]] platform::FilePicker& picker() noexcept { return picker_; }
  [[nodiscard]] ScriptEvents& events() noexcept { return events_; }

 private:
  void define_common(Interpreter& interp);
  cmd::CommandResult menu_command(std::string_view menu, const cmd::Invocation& inv);
  cmd::CommandResult open_file(const cmd::Invocation& inv);
  void skipped(const cmd::Invocation& inv);

  MenuModel& menus_;
  platform::FilePicker& picker_;
  ScriptEvents& events_;
  diag::DiagnosticSink* sink_ = nullptr;
  bool common_defined_ = false;
};

/// Magic's wrapper File menu (tcltk/wrapper.tcl:1226-1244), as native items.
[[nodiscard]] std::vector<MenuItem> magic_file_menu();

/// The Tk commands whose statements are skipped (Magic's own `label` and
/// `grid` are engine commands and are not listed).
[[nodiscard]] std::span<const std::string_view> skipped_tk_commands();

}  // namespace hefesto::script
