// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `xschemrc` files evaluated into a given sandboxed interpreter (capability
// schematic-symbols-and-libraries "xschemrc as data"; Q-SCHEM-23,
// Q-SCHEM-25, Q-SCHEM-32). SearchPath::from_xschemrc uses a fresh
// interpreter; the `tcleval` environment (schem/tcl/tcl_environment.hpp)
// evaluates the files into its own, so the procedures and variables they
// define stay available to `tcleval` values.
//
// Owner: SYMLIB.

#pragma once

#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/script/interp.hpp"
#include "hefesto/script/progress.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace hefesto::schem {

/// The sandbox an `xschemrc` runs in: read-only access to the rc file's
/// folder, the document folder, PDK_ROOT and XSCHEM_SHAREDIR, nothing
/// writable, and the synthetic `env` of Q-SCHEM-23 (HOME and PWD: the
/// document folder unless `input.env` gives them).
[[nodiscard]] script::SandboxPolicy xschemrc_policy(const XschemrcInput& input);

/// Sets `input.variables`, then sources `input.rc_file` and the document
/// folder's `xschemrc` in `interp`, at most `input.time_limit` in total
/// (enforced through `progress`, the interpreter's progress hook, which is
/// restored afterwards); returns the search path and the global variables
/// the files set. `puts` is redefined to capture output and `unknown`
/// accepts every unknown command.
[[nodiscard]] XschemrcResult evaluate_xschemrc(script::Interpreter& interp, script::ProgressReporting& progress,
                                               platform::FileSystem& fs, const XschemrcInput& input);

}  // namespace hefesto::schem
