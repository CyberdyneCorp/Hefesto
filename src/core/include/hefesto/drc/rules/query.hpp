// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Introspection of a compiled DRC style (openspec/changes/m3-drc/design.md
// D5, "Area contracts" RULES): the `drc printrules` and `drc rulestats`
// texts (formatted here, printed by COMMANDS), the `tech drc` queries
// (tech-command "tech drc", replacing TECH-CMD-005) and the default
// widths, spacings and surrounds the wiring tool and other areas read
// (DRCGetDefaultLayerWidth and friends; the wiring tool stops parsing the
// raw drc section).
//
// Values are in internal units of the rule set's grid; callers format them
// in display units (cmd::format_value).
//
// (Magic ref: drc/DRCprint.c DRCPrintRulesTable, maskToPrint;
// drc/DRCtech.c DRCTechRuleStats, DRCGetDefaultLayerWidth,
// DRCGetDefaultLayerSpacing, DRCGetDefaultWideLayerSpacing,
// DRCGetDefaultLayerSurround, DRCGetDirectionalLayerSurround;
// commands/CmdTZ.c CmdTech "drc")
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstdint>
#include <ostream>
#include <span>
#include <string>

#include "hefesto/cmd/value.hpp"
#include "hefesto/diag/outcome.hpp"
#include "hefesto/drc/types.hpp"

namespace hefesto::tech {
class Technology;
}

namespace hefesto::drc {

/// DRCPrintRulesTable: the full `drc printrules` text (rules per edge pair,
/// the illegal subcell overlaps, the exact-overlap types).
void print_rules(const RuleSet& rules, const tech::Technology& technology, std::ostream& out);

/// DRCTechRuleStats: the `drc rulestats` text.
void rule_stats(const RuleSet& rules, std::ostream& out);

/// `tech drc <rule> <layer> [<layer2>]`: `args` are the words after `tech
/// drc`. The value is an integer in internal units, or none for an unknown
/// rule keyword; an unknown layer fails with TECH-CMD-006 ("No such layer
/// <n>"), a missing second layer of `surr`/`direc` with DRC-TECH-005.
[[nodiscard]] diag::Outcome<cmd::Value> tech_drc_query(const RuleSet& rules, const tech::Technology& technology,
                                                       std::span<const std::string> args);

/// DRCGetDefaultLayerWidth: the minimum width of `type` (0 = no rule).
[[nodiscard]] std::int32_t default_width(const RuleSet& rules, const tech::Technology& technology,
                                         tech::TileType type);
/// DRCGetDefaultLayerSpacing(t1, t2).
[[nodiscard]] std::int32_t default_spacing(const RuleSet& rules, const tech::Technology& technology,
                                           tech::TileType type1, tech::TileType type2);
/// DRCGetDefaultWideLayerSpacing(type, twidth).
[[nodiscard]] std::int32_t default_wide_spacing(const RuleSet& rules, const tech::Technology& technology,
                                                tech::TileType type, std::int32_t width);
/// DRCGetDefaultLayerSurround(t1, t2).
[[nodiscard]] std::int32_t default_surround(const RuleSet& rules, const tech::Technology& technology,
                                            tech::TileType type1, tech::TileType type2);
/// DRCGetDirectionalLayerSurround(t1, t2).
[[nodiscard]] std::int32_t directional_surround(const RuleSet& rules, tech::TileType type1, tech::TileType type2);

}  // namespace hefesto::drc
