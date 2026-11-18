// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m6-lef-def-oasis/design.md D3): a command
// option or function declared by the M6 scaffold before its workstream
// implements it fails with CORE-NOTIMPL-006 ("not implemented yet: <what>"),
// so command completion, help, scripts and golden inputs see the final names
// from the start. Each workstream replaces its stubs; INTEGRATION removes
// this header and retires the code at M6 exit.
//
// Owner: INTEGRATION.

#pragma once

#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::cmd {

/// The CORE-NOTIMPL-006 diagnostic for `what` (e.g. "lef read").
[[nodiscard]] diag::Diagnostic not_implemented_diagnostic(std::string_view what);

/// A failed result carrying not_implemented_diagnostic(what).
[[nodiscard]] CommandResult not_implemented(std::string_view what);

/// A failed status carrying not_implemented_diagnostic(what).
[[nodiscard]] diag::Status not_implemented_status(std::string_view what);

}  // namespace hefesto::cmd
