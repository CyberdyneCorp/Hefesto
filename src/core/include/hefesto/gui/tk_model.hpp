// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The headless widget model of hefesto-cli's wrapper mode (openspec/changes/
// m4-scripting/design.md D16: GUI cases replay through the view models and
// hefesto-cli, comparing issued commands and files, not pixels).
//
// `hefesto-cli --magic -d X11` has no display, yet the scripts of Magic's GUI
// cases address Tk widgets: `winfo children .layout1.pane.top`, a menu's
// `entrycget`, a button's `invoke`, `wm title .params`. install_tk_model()
// defines, in the project's interpreter, Tk's widget commands over an
// in-memory widget tree (paths, classes, options, menu entries, treeview
// items, bindings), with Tk's error texts (`bad window path name "P"`,
// `invalid command name "P"` for a widget never created). Nothing is drawn;
// a menu entry or button runs its `-command` like Tk does.
//
// The wrapper's own widgets are created from the view models: the layout
// window frame of wrapper.tcl's openwrapper with the menus of
// wrapper_menus(), the layer toolbar of build_toolbar().
//
// The desktop app never installs it: it renders the view models itself.
//
// (Magic ref: tcltk/wrapper.tcl:1052-1512, tcltk/toolbar.tcl:8-230;
// Tk 8.6 generic/tkCmds.c, tkMenu.c, ttk/ttkTreeview.c)
//
// Owner: GUIMGR (design D2).

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/gui/managers.hpp"
#include "hefesto/platform/file_picker.hpp"

namespace hefesto::cmd {
class CommandContext;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::pcell {
struct DialogModel;
}

namespace hefesto::script {
class Interpreter;
}

namespace hefesto::gui {

/// Defines the widget model's commands (`winfo`, `wm`, `bind`, `event`,
/// `destroy`, `focus`, `pack`, `place`, `image`, `font`, `tkwait`, the
/// widget creation commands, `ttk::*`) in `interp`; `label` and `grid`
/// create or place a widget when their first word is a window path and are
/// Magic's commands otherwise, as Magic's Tcl dispatcher resolves them.
/// Also adds wrapper.tcl's tags (gui_script_tags(GuiScript::wrapper)) with
/// their procedures over the model.
[[nodiscard]] cmd::CommandResult install_tk_model(script::Interpreter& interp);

/// True once install_tk_model() ran in `interp`.
[[nodiscard]] bool has_tk_model(const script::Interpreter& interp);

/// Creates the widgets of wrapper frame `frame` (`F`, `F.pane`,
/// `F.pane.top` with its scroll frames, zoom button, title bar, menu buttons
/// and menus from `menus`, the layout window `F.pane.top.magic` and the
/// toolbar frame), in openwrapper's order.
[[nodiscard]] cmd::CommandResult build_wrapper_widgets(script::Interpreter& interp, std::string_view frame,
                                                       const WrapperMenuModel& menus);

/// toolbar.tcl's maketoolbar on `layoutframe` (`F.pane.top`): the toolbar's
/// children are replaced by the canvas, its frame with one `f<layer>` row
/// (label `.l`, button `.b`, or `.p` when locked) per layer of `toolbar()`,
/// and the scroll bar. Fails as Magic does when `layoutframe` or its
/// toolbar does not exist (before `toolbar` is called); does nothing while
/// the frame is suspended.
[[nodiscard]] cmd::CommandResult build_toolbar_widgets(script::Interpreter& interp, std::string_view layoutframe,
                                                       const std::function<ToolbarModel()>& toolbar);

/// The manager windows of the model.
enum class ManagerWindow : unsigned char { cell, library, technology };

/// `cellmanager`, `libmanager` and `techmanager` with `option`: `create`
/// builds `.cellmgr`, `.libmgr` or `.techmgr` once (withdrawn, as Magic's
/// scripts do), and every option fills it from cell_manager_model(),
/// library_manager_model() (the `filtered` variable of its Filter button)
/// or tech_manager_model(); nothing happens while the window does not exist.
[[nodiscard]] cmd::CommandResult update_manager_widgets(cmd::CommandContext& ctx, script::Interpreter& interp,
                                                        ManagerWindow window, std::string_view option);

/// The engine's parameter dialog as the model's `.params` reads it
/// (`::hefesto::tk::params_model`): `{new|edit type library cell instance
/// {{kind pname label value} ...} {{text script closes} ...}}`; empty while
/// the dialog is closed (`dialog` null).
[[nodiscard]] std::string params_dialog_words(const pcell::DialogModel* dialog);

/// The file dialogs of the model: Tk's `tk_getOpenFile` / `tk_getSaveFile`
/// in `editor`'s interpreter with wrapper.tcl's arguments (`-filetypes
/// {{Magic {.mag {.mag}}} {"All files" {*}}}`, `-title`), so a script that
/// defines them answers the prompts; their default answers "" (cancelled).
class TkFilePicker final : public platform::FilePicker {
 public:
  explicit TkFilePicker(editor::EditorState& editor) : editor_(editor) {}
  [[nodiscard]] std::optional<platform::Path> pick(const platform::FilePickerRequest& request) override;

 private:
  editor::EditorState& editor_;
};

/// wrapper.tcl's `-filetypes` word for `types`: `{<desc> {<patterns> {<patterns>}}}`
/// per type, `{<desc> {*}}` for the catch-all, a description with a blank in
/// double quotes.
[[nodiscard]] std::string tk_file_types(const std::vector<std::pair<std::string, std::string>>& types);

/// The wrapper's own procedures that magic.tcl.in imports into the global
/// namespace in wrapper mode (wrapper.tcl, cellmgr.tcl, libmgr.tcl,
/// drcmgr.tcl, texthelper.tcl, toolbar.tcl are sourced before its
/// `pushnamespace magic`): every command of register_gui_commands().
[[nodiscard]] const std::vector<std::string>& wrapper_global_commands();

/// Imports wrapper_global_commands() that have no global command yet.
[[nodiscard]] cmd::CommandResult import_wrapper_commands(script::Interpreter& interp);

}  // namespace hefesto::gui
