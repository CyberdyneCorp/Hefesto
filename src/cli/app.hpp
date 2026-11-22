// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// hefesto-cli application logic (capability hefesto-architecture, requirement
// "Headless command-line tool"; capability hefesto-diagnostics, requirement
// "Diagnostic presentation and logging").

#pragma once

#include <cstdio>
#include <span>
#include <string_view>

namespace hefesto::cli {

inline constexpr int kExitOk = 0;
inline constexpr int kExitFailure = 1;
inline constexpr int kExitUsage = 2;
/// `--netlist`: the netlist run reported a failure (xschem's netlist exit code;
/// schematic-command "Headless use").
inline constexpr int kExitNetlistFailed = 10;

/// Runs the CLI with argv[1..]; returns the process exit status.
/// Results go to `out`, diagnostics to `err`.
[[nodiscard]] int run(std::span<const std::string_view> args, std::FILE* out, std::FILE* err);

}  // namespace hefesto::cli
