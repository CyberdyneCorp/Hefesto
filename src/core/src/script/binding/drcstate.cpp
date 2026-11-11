// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include "hefesto/script/binding/drcstate.hpp"

#include <memory>

#include "hefesto/editor/editor_state.hpp"

namespace hefesto::script {

DrcStateBridge::DrcStateBridge(Interpreter& interp, drc::DrcEvents& events) : interp_(interp), events_(events) {
  events_.add(*this);
}

DrcStateBridge::~DrcStateBridge() {
  events_.remove(*this);
}

void DrcStateBridge::on_state(drc::CheckerState state) {
  // Magic schedules `after idle magic::drcstate busy|idle` (drc/DRCcontin.c:
  // 499,563); Hefesto has no Tcl event loop of its own, so the event (which
  // already arrives between commands, on the command thread) calls the
  // procedure directly (design D6). The result is not used and an error is
  // not reported.
  static_cast<void>(interp_.evaluate(state == drc::CheckerState::busy ? "magic::drcstate busy"
                                                                      : "magic::drcstate idle"));
}

void install_drc_state(Interpreter& interp, editor::EditorState& editor) {
  // The bridge lives in a hidden command of the interpreter, so it goes
  // away with the interpreter (one per project, Q-SCRIPT-41).
  auto bridge = std::make_shared<DrcStateBridge>(interp, editor.drc.events);
  interp.define_command("::hefesto::_drc_state_bridge", [bridge](const cmd::Invocation&) {
    return cmd::CommandResult::success(
        cmd::Value{std::string{bridge ? "installed" : ""}});
  });
}

}  // namespace hefesto::script
