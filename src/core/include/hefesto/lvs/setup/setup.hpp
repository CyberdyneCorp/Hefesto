// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Setup-file evaluation (openspec/changes/m9-lvs-simulation design.md D7,
// "Area contracts" LVSSETUP; capability lvs-setup-file). A setup file is
// Tcl evaluated in a fresh sandboxed interpreter (X-3) per run, with the
// `netgen::` command set and the setup commands; the result is the state
// the comparison reads.
//
// Owner: LVSSETUP.

#pragma once

#include <string>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/lvs/cancel.hpp"
#include "hefesto/lvs/net/circuit.hpp"
#include "hefesto/lvs/read/source.hpp"
#include "hefesto/script/tcl/sandbox.hpp"

namespace hefesto::lvs {

/// What the setup commands recorded. The scaffold keeps each command's
/// words in evaluation order; LVSSETUP replaces the lists with typed
/// entries.
struct SetupState {
  std::vector<std::vector<std::string>> permutes;
  std::vector<std::vector<std::string>> properties;
  std::vector<std::vector<std::string>> equates;
  std::vector<std::vector<std::string>> ignores;
  std::vector<std::vector<std::string>> models;
  std::vector<std::vector<std::string>> globals;
  std::vector<std::vector<std::string>> flatten;
};

struct SetupContext {
  Circuit* first = nullptr;   // circuit 1 (required)
  Circuit* second = nullptr;  // circuit 2 (required)
  /// The sandbox of the fresh interpreter: a read-only root over the PDK
  /// store and the document folder, and the step budget (design D7).
  script::SandboxPolicy sandbox;
  CancelToken cancel;
};

[[nodiscard]] diag::Outcome<SetupState> evaluate_setup(const Source& setup, SetupContext& context);

}  // namespace hefesto::lvs
