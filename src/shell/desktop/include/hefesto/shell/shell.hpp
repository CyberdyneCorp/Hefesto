// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Entry point of the desktop shell. No Raven type may appear in any header
// under include/ (capability hefesto-architecture, "App shells per
// platform"; product decision P-11): Raven is confined to src/raven_ui.cpp.

#pragma once

namespace hefesto::shell {

/// Runs the desktop application; returns the process exit code
/// (0 ok, 1 error, 2 bad arguments, 3 Raven could not start: no display or
/// no Vulkan driver).
[[nodiscard]] int run(int argc, char** argv);

}  // namespace hefesto::shell
