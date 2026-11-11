// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared by the shell tests: every line the shell issues is Tcl evaluated in
// the project's interpreter (shell/gui_script.hpp; m4-scripting tasks.md
// 13.2), which app::run_startup opens and binds in the desktop app.

#pragma once

#include <utility>

#include "hefesto/script/binding/bind.hpp"
#include "hefesto/script/project.hpp"
#include "hefesto/shell/core_command_sink.hpp"

namespace hefesto::shell::test {

/// Opens `core`'s project interpreter and binds every command registered so
/// far (call after the shell's own commands are registered), as
/// app::run_startup does.
inline script::Interpreter& bind_interpreter(CoreSession& core, script::SandboxPolicy policy = {}) {
  script::Interpreter& interp = script::open_project_interpreter(core.editor(), std::move(policy));
  (void)script::bind_registry(interp, core.registry(), core.dispatcher());
  return interp;
}

}  // namespace hefesto::shell::test
