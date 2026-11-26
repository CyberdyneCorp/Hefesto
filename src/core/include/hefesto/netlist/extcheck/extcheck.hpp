// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// extcheck, library part (capability netlist-extcheck;
// openspec/changes/m7-extraction/design.md D5, D15): reads and flattens a
// root cell's `.ext` tree and prints the device, node, capacitance and
// resistance counts in Magic's format, with the defaults of Q-NETLIST-36.
// The standalone `hefesto-extcheck` (src/cli/extcheck/, Open Question 6
// (a)) calls it with its argv.
//
// SIM implements run() (task 12.4); the scaffold's CORE-NOTIMPL-008 text
// is retired (task 19.5).
//
// Owner: INTEGRATION (this header); SIM (the implementation).

#pragma once

#include <ostream>
#include <span>
#include <string>

namespace hefesto::netlist::extcheck {

/// extcheck's main(): `args` without the program name; `program` is argv[0]
/// as printed by the usage text. Returns the exit status.
[[nodiscard]] int run(std::span<const std::string> args, std::ostream& out, std::ostream& err,
                      const std::string& program = "extcheck");

}  // namespace hefesto::netlist::extcheck
