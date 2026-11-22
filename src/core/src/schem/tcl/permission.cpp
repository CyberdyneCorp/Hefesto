// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Launcher confirmation once per document (capability
// schematic-symbols-and-libraries "Script execution permission";
// Q-SCHEM-25).
//
// Owner: SYMLIB.

#include "hefesto/schem/tcl/permission.hpp"

#include <string>

#include "hefesto/diag/diagnostic.hpp"

namespace hefesto::schem {

bool LauncherPermission::allow(DocumentId document, std::string_view tclcommand, const ConfirmLauncher& ask) {
  if (confirmed(document)) return true;
  if (!ask || !ask(tclcommand)) return false;
  confirmed_.insert(document);
  return true;
}

cmd::CommandResult run_launcher(script::Interpreter& sandbox, LauncherPermission& permission, DocumentId document,
                                std::string_view tclcommand, const ConfirmLauncher& ask) {
  if (!permission.allow(document, tclcommand, ask)) {
    return cmd::CommandResult::failure(diag::make(
        "SCHEM-TCL-002", "Launcher command not run: it was not confirmed for this document: " + std::string{tclcommand}));
  }
  return sandbox.evaluate(tclcommand, cmd::Origin::script);
}

}  // namespace hefesto::schem
