// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Window-directed commands (openspec/changes/m4-scripting/design.md D6;
// capability scripting-api "Window-directed commands"): each window path
// the shell announces (`.layout1`, ...) becomes a Tcl command that runs its
// arguments as a command sent to that window.
//
// Owner: BINDING (design D2).

#pragma once

#include <span>
#include <string>

#include "hefesto/cmd/command.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/script/interp.hpp"

namespace hefesto::script {

/// Registers one command per window path of `window_paths` in `interp`.
[[nodiscard]] cmd::CommandResult register_window_commands(Interpreter& interp, cmd::Dispatcher& dispatcher,
                                                          std::span<const std::string> window_paths);

}  // namespace hefesto::script
