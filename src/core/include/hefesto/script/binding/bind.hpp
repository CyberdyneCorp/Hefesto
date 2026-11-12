// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The command binding (openspec/changes/m4-scripting/design.md D4/D6;
// capability scripting-api "Command registration in the magic namespace",
// "Global import of magic commands", "Name collisions with Tcl built-ins",
// "Unknown-command fallback for renamed built-ins", "Command status and
// error signalling"): every registry command becomes the Tcl object command
// ::magic::<name>, which builds a cmd::Invocation (origin `script`) and
// calls cmd::Dispatcher::execute, then is imported globally with Magic's
// collision rules (`tcl_<name>` renames, the unknown handler, `magic::`
// precedence). Dispatch failures through the namespace are quiet; command
// failures return ok unless strict mode or CommandResult::raises.
//
// Owner: BINDING (design D2).

#pragma once

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::script {

/// Registers every command of `registry` in `interp` (see above), executing
/// through `dispatcher`, whose editor session is the interpreter's project.
[[nodiscard]] cmd::CommandResult bind_registry(Interpreter& interp, const cmd::CommandRegistry& registry,
                                               cmd::Dispatcher& dispatcher);

}  // namespace hefesto::script
