// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// How a screen action runs (openspec/changes/m4-scripting/design.md D13
// "Commands only", tasks.md 13.2): every line the shell issues — menu
// items, Place device and the PCell dialog (Tcl written by Magic's wrapper
// or by PDK scripts: `sky130::importspice`, `set ::magic::w_val 1.0 ;
// magic::update_dialog ...`), the command bar, the manager buttons and
// `--run` (ScreenContext::run), key and button macros — is Tcl evaluated in
// the project's interpreter, as in Magic's Tcl build. Every engine command
// goes through the Dispatcher (logged) and through the binding, so command
// tags fire (`load` → `clearstack; cellmanager`). Without an interpreter
// (no project open) nothing runs: SCRIPT-TCL-001.
//
// Owner: SHELL (design D2).

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::cmd {
class Dispatcher;
}

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::shell {

/// Runs `script` with `origin`: `gui` for the shell's own actions, `script`
/// for text a PDK supplied (its menu items and device entries), so that the
/// sandbox's script rules apply to it (security finding S-01).
[[nodiscard]] cmd::CommandResult run_gui_script(editor::EditorState& editor, cmd::Dispatcher& dispatcher,
                                                std::string_view script, cmd::Origin origin = cmd::Origin::gui);

/// What a traced GUI script did.
struct GuiScriptRun {
  /// The interpreter's result: its value, or the Tcl error (SCRIPT-TCL-*).
  cmd::CommandResult result;
  /// Every diagnostic reported while it ran (the engine commands' and the
  /// sandbox's), in order.
  std::vector<diag::Diagnostic> reported;
  /// An engine command failed, also quietly (Magic's non-strict failures
  /// leave the Tcl result ok; the line still shows as failed).
  bool command_failed = false;

  [[nodiscard]] bool ok() const noexcept { return result.ok() && !command_failed; }
};

/// run_gui_script that also collects what the script's engine commands
/// reported and whether one failed. When the script fails, the diagnostics
/// of its result that no command reported (a Tcl parse error, an unknown
/// Tcl procedure, sandbox denials) are reported to the Dispatcher's sinks
/// too and added to `reported`, so they reach the Diagnostics panel like
/// an engine error. `quit` unwinds the evaluation (Magic ends the process
/// there); the run is then ok.
[[nodiscard]] GuiScriptRun run_gui_script_traced(editor::EditorState& editor, cmd::Dispatcher& dispatcher,
                                                 std::string_view script, cmd::Origin origin = cmd::Origin::gui);

/// While it exists, run_gui_script runs nothing and answers SCRIPT-TCL-004:
/// the window holds one while it draws a frame from a running script's
/// progress report, so a click, a key macro or a menu item chosen in that
/// frame cannot start a command inside the running script (boards
/// M4-Console note 8 and M4-Script-Consent tile 7: the layout takes no
/// input until the script ends; the Interrupt card and Ctrl-C do not run
/// commands). UI thread only.
class ScriptInputBlock {
 public:
  ScriptInputBlock() noexcept;
  ~ScriptInputBlock();
  ScriptInputBlock(const ScriptInputBlock&) = delete;
  ScriptInputBlock& operator=(const ScriptInputBlock&) = delete;
  [[nodiscard]] static bool active() noexcept;
};

/// The text a result shows in the Console: its value, else its first diagnostic.
[[nodiscard]] std::string gui_result_text(const cmd::CommandResult& result);

}  // namespace hefesto::shell
