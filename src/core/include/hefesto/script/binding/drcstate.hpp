// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `magic::drcstate busy|idle` from the DRC engine's events
// (openspec/changes/m4-scripting/design.md D6; capability scripting-api
// "Generic Tcl helpers defined at startup": the procedure the background
// DRC code calls, a no-op until the GUI or a script redefines it; the M3
// non-goal of openspec/changes/m3-drc "magic::drcstate becomes a Tcl
// callback in M4"). Each open project has its own interpreter and its own
// drc::DrcEvents, so every interpreter hears its own project's checker.
//
// Owner: BINDING (design D2).

#pragma once

#include "hefesto/drc/events.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::script {

/// Forwards drc::DrcEvents busy/idle transitions to `magic::drcstate` in
/// one interpreter (DRCContinuous's TagCallback-free Tcl_EvalEx). A failure
/// of the procedure is ignored, as in Magic.
class DrcStateBridge final : public drc::DrcObserver {
 public:
  /// Registers with `events`; removed again by the destructor.
  DrcStateBridge(Interpreter& interp, drc::DrcEvents& events);
  ~DrcStateBridge() override;
  DrcStateBridge(const DrcStateBridge&) = delete;
  DrcStateBridge& operator=(const DrcStateBridge&) = delete;

  void on_state(drc::CheckerState state) override;

 private:
  Interpreter& interp_;
  drc::DrcEvents& events_;
};

/// Installs a DrcStateBridge from `editor.drc.events` to `interp`, owned by
/// the interpreter (it is deleted with the interpreter). The interpreter
/// must therefore be closed while `editor.drc` still exists
/// (script::close_project_interpreter before the EditorState is destroyed).
void install_drc_state(Interpreter& interp, editor::EditorState& editor);

}  // namespace hefesto::script
