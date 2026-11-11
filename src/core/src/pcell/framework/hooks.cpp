// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// toolkit.tcl's top-level hooks and Tk's `raise` for `.params` (Magic ref:
// tcltk/toolkit.tcl:107-117,1769-1770; Tk 8.6 generic/tkCmds.c
// Tk_RaiseObjCmd). See hooks.hpp.

#include "hefesto/pcell/framework/hooks.hpp"

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/interp.hpp"
#include "pcell/framework/toolkit.hpp"

namespace hefesto::pcell {

namespace {

constexpr std::string_view kParams = ".params";

// TkGetWindowFromObj: only `.params` exists, and only while the dialog is open.
bool window_exists(const FrameworkState& state, std::string_view path) {
  return path == kParams && state.dialog.has_value();
}

}  // namespace

cmd::CommandResult raise_window(FrameworkState& state, const std::vector<std::string>& args) {
  if (args.empty() || args.size() > 2) {
    return detail::wrong_args("raise window ?aboveThis?");
  }
  for (const std::string& path : args) {
    if (!window_exists(state, path)) {
      return detail::script_error("bad window path name \"" + path + "\"");
    }
  }
  ++state.dialog->raise_count;
  return cmd::CommandResult::success();
}

cmd::CommandResult install_toolkit(script::Interpreter& interp, editor::EditorState& editor) {
  FrameworkState& state = editor.script.gencell;
  interp.define_command("::raise", [&state](const cmd::Invocation& inv) { return raise_window(state, inv.args); });
  return interp.evaluate(R"(magic::tag add select "magic::gencell_update %1")", cmd::Origin::internal);
}

}  // namespace hefesto::pcell
