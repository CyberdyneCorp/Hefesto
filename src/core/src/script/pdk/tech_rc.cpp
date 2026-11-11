// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `<tech>.magicrc` step of technology auto-discovery (design D7;
// scripting-pdk-integration "Technology auto-discovery from layout files";
// Q-SCRIPT-10), after consent (hefesto-security "Consent before implicit
// script execution"). Magic 8.3.694 (fc5daa9e) database/DBio.c:681-697
// evaluates the file with Tcl_EvalFile and ignores its result; so does this.

#include "hefesto/script/pdk/tech_rc.hpp"

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/script/pdk/trust.hpp"
#include "hefesto/script/project.hpp"

namespace hefesto::script {

cmd::CommandResult run_tech_magicrc(editor::EditorState& editor, const PdkIdentity& pdk,
                                    const platform::Path& rc_file, ConsentHost* consent) {
  if (editor.script.interp == nullptr) {
    return cmd::CommandResult::failure(diag::make(
        "SCRIPT-PDK-008", "The technology startup script \"" + rc_file.string() + "\" cannot run: no interpreter."));
  }
  auto decision = request_consent(editor.script.pdk_trust, editor.script.events, consent, pdk, rc_file, true);
  if (!decision.run) {
    cmd::CommandResult declined;
    declined.status = cmd::Status::failed;
    declined.diagnostics = std::move(decision.diagnostics);
    return declined;
  }
  // DBio.c: the result of Tcl_EvalFile is not checked; the file counts as run.
  static_cast<void>(editor.script.interp->source(rc_file));
  cmd::CommandResult out = cmd::CommandResult::success();
  out.diagnostics = std::move(decision.diagnostics);
  return out;
}

}  // namespace hefesto::script
