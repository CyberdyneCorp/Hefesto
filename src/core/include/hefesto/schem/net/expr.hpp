// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `expr(...)` evaluation in format strings (openspec/changes/m8-schematic
// design.md D9, "Area contracts" NETLIST; capability schematic-netlisting
// "expr evaluation", Q-SCHEM-12).
//
// Owner: NETLIST.

#pragma once

#include <string>
#include <string_view>

namespace hefesto::schem {

/// The text that replaces `expr(<expression>)`, given the expression
/// between the parentheses. `xschem_compat` selects the Q-SCHEM-12 number
/// lexing. Identical on every platform (D20).
[[nodiscard]] std::string evaluate_expr(std::string_view expression, bool xschem_compat);

}  // namespace hefesto::schem
