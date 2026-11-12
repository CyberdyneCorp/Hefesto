// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SPICE netlist seeding (openspec/changes/m4-scripting/design.md D9;
// capability pcell-toolkit "SPICE netlist to layout seeding", "Seeding a
// subcircuit cell"): magic::netlist_to_layout <file> <library>.
//
// Owner: PCELL (design D2).

#pragma once

#include <string_view>

#include "hefesto/cmd/command.hpp"
#include "hefesto/platform/file_system.hpp"

namespace hefesto::cmd {
class CommandContext;
}

namespace hefesto::pcell {

/// magic::netlist_to_layout netfile library (toolkit.tcl:448-639), in the
/// running command's session.
[[nodiscard]] cmd::CommandResult netlist_to_layout(cmd::CommandContext& ctx, const platform::Path& netlist,
                                                   std::string_view library);

}  // namespace hefesto::pcell
