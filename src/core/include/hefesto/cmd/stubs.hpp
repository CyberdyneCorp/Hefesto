// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Scaffold stubs (openspec/changes/m4-scripting/design.md D3): a command or
// function declared by the M4 scaffold before its workstream implements it
// fails with CORE-NOTIMPL-004 ("not implemented yet: <name>"), so command
// completion, help, scripts and golden inputs see the final names from the
// start. Each workstream replaces its stubs; INTEGRATION removes this header
// and retires the code at M4 exit (tasks.md 13.5).
//
// Owner: INTEGRATION.

#pragma once

#include <string>
#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/registry.hpp"

namespace hefesto::cmd {

/// The CORE-NOTIMPL-004 diagnostic for `what` (a command or function name).
[[nodiscard]] diag::Diagnostic not_implemented_diagnostic(std::string_view what);

/// A failed result carrying not_implemented_diagnostic(what).
[[nodiscard]] CommandResult not_implemented(std::string_view what);

/// A command whose handler always returns not_implemented(name).
[[nodiscard]] CommandSpec stub_command(std::string name, std::string synopsis, std::string help);

}  // namespace hefesto::cmd
