// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The wrapper layout window (openspec/changes/m4-scripting/design.md D8/D12;
// capability ui-gui-managers "Wrapper window construction", "Wrapper hook
// points for scripts", "Wrapper menus", "Wrapper tag callbacks and caption
// formats", "Wrapper option variables", "Window close and key overrides",
// "Per-window command entry"; Q-SCRIPT-13, Q-SCRIPT-36).
//
// Native port of wrapper.tcl without widgets: open_wrapper() creates the
// layout window `F.pane.top.magic` and runs the script hooks;
// wrapper_menus() is the menu model SHELL renders (every item a logged
// command). As in Magic, the wrapper's state is Tcl state of the project's
// interpreter (`lwindow`, `owindow`, `Opts(...)`, `Winopts(...)`), so PDK
// scripts read and set it as they do in Magic; the wrapper therefore needs
// the project interpreter (editor::EditorState::script.interp).
//
// (Magic ref: tcltk/wrapper.tcl:233-268,533-729,1050-1584, tcltk/tools.tcl:267-295)
//
// Owner: GUIMGR (design D2).

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/gui/managers.hpp"

namespace hefesto::cmd {
class CommandContext;
}

namespace hefesto::script {
class Interpreter;
}

namespace hefesto::gui {

/// "Wrapper option variables": the `Opts(...)` defaults, in Magic's order.
[[nodiscard]] const std::vector<std::pair<std::string, std::string>>& wrapper_option_defaults();

/// Sets every default of wrapper_option_defaults() that is not set yet.
void init_wrapper_options(script::Interpreter& interp);

/// The layout window of frame `F`: `F.pane.top.magic`.
[[nodiscard]] std::string wrapper_window_name(std::string_view frame);

/// `magic::openwrapper ?cell? ?frame?`: the layout window path
/// (`F.pane.top.magic`). Fails with GUI-WRAP-001 without a project
/// interpreter or when the frame exists.
[[nodiscard]] cmd::CommandResult open_wrapper(cmd::CommandContext& ctx, std::string_view cell,
                                              std::string_view frame);

/// `magic::closewrapper F` ("Window close and key overrides").
[[nodiscard]] cmd::CommandResult close_wrapper(cmd::CommandContext& ctx, std::string_view frame);

/// Makes the session's existing layout window the wrapper frame `frame`
/// (".layout1") instead of opening a new one: the front end that draws its
/// own window chrome (the desktop app) calls it once its startup files ran.
/// The window is named `frame.pane.top.magic`, `lwindow` counts it, the
/// PDK's widget statements for the frame are intercepted (the menu model,
/// the file picker; script::WidgetInterceptor) and the wrapper's script
/// hooks run (`Opts(callback)`, `Winopts(frame,callback)`), as openwrapper
/// does. Fails when the session has no interpreter or no layout window.
[[nodiscard]] cmd::CommandResult adopt_wrapper(cmd::CommandContext& ctx, std::string_view frame);

/// "Wrapper menus" of frame `frame` (Q-SCRIPT-36: Rotate 90 degree runs
/// `magic::clockwise 90`). `maze_helper` adds Options → Maze Router.
[[nodiscard]] WrapperMenuModel wrapper_menus(std::string_view frame, bool maze_helper = false);

/// The accelerator key text of a key macro name (`Control_`→`^`, `XK_`
/// removed, `less`→`<`, `more`→`>`, `comma`→`,`, `question`→`?`).
[[nodiscard]] std::string accelerator_key_name(std::string_view key);

/// `Opts(autobuttontext)`: each command item's accelerator becomes `(<key>)`
/// for the key macro whose text equals the command after stripping
/// `magic::` and surrounding blanks (the last such key in `macro list
/// -reverse` order), else empty. `reverse` is that list's (text, key) pairs.
void apply_accelerators(WrapperMenuModel& menus, const std::vector<std::pair<std::string, std::string>>& reverse);

/// The menu model of `frame` with the accelerators of the session's key
/// macros (`macro list -reverse`) when `Opts(autobuttontext)` is on.
[[nodiscard]] WrapperMenuModel wrapper_menu_model(cmd::CommandContext& ctx, std::string_view frame);

/// The caption `Loaded: <root> Editing: <edit> Tool: <tool>    Technology: <tech>`
/// from `windowcaption`'s value (element 0 root; element 1 `EDITING` then
/// element 2 is the edit cell, else the whole caption).
[[nodiscard]] std::string wrapper_caption(const std::vector<std::string>& window_caption, std::string_view tool,
                                          std::string_view technology);

/// The box position label in µm: `box (%+g %+g) to (%+g %+g) microns`, or
/// `no box`.
[[nodiscard]] std::string box_position_label(const std::optional<std::array<double, 4>>& box_microns);

/// The pointer label: `(%+g %+g) %+g %+g microns` (pointer and offset from
/// the box lower-left) or `(%+g %+g) microns` without a box.
[[nodiscard]] std::string pointer_position_label(double x, double y,
                                                 const std::optional<std::array<double, 2>>& box_ll);

/// The DRC button: `DRC=<count>` and its indicator colour.
struct DrcButton {
  enum class Colour : unsigned char { green, red, yellow };
  std::string text;
  Colour colour = Colour::green;
};
[[nodiscard]] DrcButton drc_button(bool busy, long long total_count);

/// "Per-window command entry": the prompt `F>` and the command a line typed
/// in frame F's console runs (`F.pane.top.magic <line>`).
[[nodiscard]] std::string command_entry_prompt(std::string_view frame);
[[nodiscard]] std::string command_entry_line(std::string_view frame, std::string_view line);

}  // namespace hefesto::gui
