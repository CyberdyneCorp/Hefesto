// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// M7 scaffold stubs (openspec/changes/m7-extraction/design.md D4): a command
// option or function declared by the M7 scaffold before its workstream
// implements it fails with CORE-NOTIMPL-008 ("not implemented yet: <what>"),
// so completion, help, scripts and golden inputs see the final names from
// the start. Shared by the extract, extflat, netlist, extresist and antenna
// areas. Each workstream replaces its stubs; INTEGRATION removes this header
// and retires the code at M7 exit (tasks.md, R6).
//
// Owner: INTEGRATION.

#pragma once

#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::cmd {

/// The CORE-NOTIMPL-008 diagnostic for `what` (e.g. "extract all").
[[nodiscard]] diag::Diagnostic m7_not_implemented_diagnostic(std::string_view what);

/// A failed result carrying m7_not_implemented_diagnostic(what).
[[nodiscard]] CommandResult m7_not_implemented(std::string_view what);

/// A failed status carrying m7_not_implemented_diagnostic(what).
[[nodiscard]] diag::Status m7_not_implemented_status(std::string_view what);

}  // namespace hefesto::cmd
