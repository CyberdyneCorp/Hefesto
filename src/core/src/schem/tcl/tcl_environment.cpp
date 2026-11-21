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

TclEnvironment::TclEnvironment(editor::EditorState& editor, script::SandboxPolicy policy, platform::FileSystem& fs,
                               ProcedureHooks hooks)
    : editor_(editor),
      policy_(std::make_unique<script::SandboxPolicy>(std::move(policy))),
      fs_(fs),
      files_(std::make_unique<script::SandboxedFileSystem>(fs_, *policy_)),
      interp_(script::create_project_interpreter(editor_, *policy_)) {
  hooks.files = files_.get();
  install_xschem_procedures(*interp_, std::move(hooks));
}

TclEnvironment::~TclEnvironment() {
  interp_.reset();  // before the file system it may still refer to
}

XschemrcResult TclEnvironment::load_xschemrc(const XschemrcInput& input) {
  return evaluate_xschemrc(*interp_, editor_.script.progress, fs_, input);
}

}  // namespace hefesto::schem
