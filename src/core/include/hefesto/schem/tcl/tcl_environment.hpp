// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The sandboxed interpreter `tcleval` values run in (openspec/changes/
// m8-schematic design.md D7; capability schematic-symbols-and-libraries
// "tcleval values", "xschemrc as data", "Script execution permission";
// Q-SCHEM-8, Q-SCHEM-25): the procedures xschem provides, then the active
// PDK's `xschemrc` evaluated into the same interpreter, so the procedures
// it defines (`symtype`, `cap_cmomi_C`, ...) and the variables it sets
// (`SKYWATER_MODELS`, `MODELS_NGSPICE`, ...) are there for `tcleval`.
//
// Owner: SYMLIB.

#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/tcl/procedures.hpp"
#include "hefesto/schem/tcl/tcl_bridge.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace hefesto::editor {
struct EditorState;
}

namespace hefesto::schem {

class TclEnvironment {
 public:
  /// Creates the interpreter of `editor`'s project restricted by `policy`
  /// (sandbox always, on every platform) and installs the xschem
  /// procedures. `write_data` writes through `fs` restricted to `policy`;
  /// `hooks.files` is ignored. `editor` and `fs` must outlive this object.
  TclEnvironment(editor::EditorState& editor, script::SandboxPolicy policy, platform::FileSystem& fs,
                 ProcedureHooks hooks = {});
  ~TclEnvironment();
  TclEnvironment(const TclEnvironment&) = delete;
  TclEnvironment& operator=(const TclEnvironment&) = delete;

  /// Evaluates the `xschemrc` files of `input` into this interpreter
  /// (evaluate_xschemrc: at most `input.time_limit`, SCHEM-LIB-010).
  [[nodiscard]] XschemrcResult load_xschemrc(const XschemrcInput& input);

  /// A bridge over this interpreter, valid while this object lives.
  [[nodiscard]] TclBridge bridge() const noexcept { return TclBridge{interp_.get()}; }

  [[nodiscard]] script::Interpreter& interpreter() noexcept { return *interp_; }
  [[nodiscard]] const script::SandboxPolicy& policy() const noexcept { return *policy_; }

 private:
  editor::EditorState& editor_;
  std::unique_ptr<script::SandboxPolicy> policy_;  // stable address: files_ refers to it
  platform::FileSystem& fs_;
  std::unique_ptr<platform::FileSystem> files_;    // fs_ restricted to policy_
  std::unique_ptr<script::Interpreter> interp_;
};

}  // namespace hefesto::schem
