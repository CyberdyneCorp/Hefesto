// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The sandboxed `tcleval` interpreter (capability
// schematic-symbols-and-libraries "tcleval values", "Script execution
// permission"; design D7).
//
// Owner: SYMLIB.

#include "hefesto/schem/tcl/tcl_environment.hpp"

#include <utility>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/schem/lib/xschemrc.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"

namespace hefesto::schem {

namespace {

// The policy of a schematic-script interpreter: `policy` without reads of
// stdin, which block out of reach of the time limits (S8-04).
script::SandboxPolicy without_stdin(script::SandboxPolicy policy) {
  policy.stdin_readable = false;
  return policy;
}

}  // namespace

TclEnvironment::TclEnvironment(editor::EditorState& editor, script::SandboxPolicy policy, platform::FileSystem& fs,
                               ProcedureHooks hooks)
    : editor_(editor),
      policy_(std::make_unique<script::SandboxPolicy>(without_stdin(std::move(policy)))),
      fs_(fs),
      files_(std::make_unique<script::SandboxedFileSystem>(fs_, *policy_)),
      interp_(script::create_project_interpreter(editor_, *policy_)) {
  run_.progress = &editor_.script.progress;  // the hook the time limits of the values go through
  hooks.files = files_.get();
  hooks.denied = [this](const diag::Diagnostic& denial) { report_tcleval(run_, denial); };
  install_xschem_procedures(*interp_, std::move(hooks));
}

TclEnvironment::~TclEnvironment() {
  interp_.reset();  // before the file system it may still refer to
}

XschemrcResult TclEnvironment::load_xschemrc(const XschemrcInput& input) {
  return evaluate_xschemrc(*interp_, editor_.script.progress, fs_, input);
}

}  // namespace hefesto::schem
