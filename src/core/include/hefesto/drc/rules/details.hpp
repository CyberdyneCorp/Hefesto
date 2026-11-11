// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc::StyleDetails: the RULES-private parts of a compiled DRC style, held
// by RuleSet::details() (openspec/changes/m3-drc/design.md D4/D5): what
// `drc printrules`, `drc rulestats`, `tech drc`, rescaling (Q-DRC-1) and the
// M5 mask-layer checks need but the check kernel does not.
//
// RULES owns this file and may add members freely; no other workstream
// reads them directly (COMMANDS goes through the query functions of
// query.hpp).
//
// (Magic ref: drc/drc.h DRCStyle; drc/DRCcif.c drcCifRules;
// drc/DRCtech.c drcRulesSpecified, drcRulesOptimized, DRCTechRuleStats)
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hefesto/tech/types.hpp"

namespace hefesto::drc {

/// One compiled mask-layer rule (`cifwidth`, `cifspacing`, `cifarea`,
/// `cifmaxwidth`), kept unchecked until M5 (design D13). Distances are in
/// the CIF units of the bound cifoutput style, as written.
struct MaskLayerRule {
  std::string keyword;        // "cifwidth", "cifspacing", "cifarea", "cifmaxwidth"
  std::string layer;          // mask layer in the bound cifoutput style
  std::string layer2;         // second layer (cifspacing), else empty
  std::int32_t distance = 0;  // width, spacing, area or maximum width
  std::int32_t value2 = 0;    // cifarea horizon, else 0
  std::uint32_t why = 0;      // WhyIndex
  bool touching_ok = false;   // cifspacing touching_ok (else touching_illegal)
  std::uint32_t flags = 0;    // cifmaxwidth bends: flags::kBends, kMaxWidthBoth or 0
  std::uint8_t exception = 0xff;  // the exception tag in effect

  friend bool operator==(const MaskLayerRule&, const MaskLayerRule&) = default;
};

/// One `no_overlap` statement (drc-rule-language "Overlap rules and the DRC
/// paint table").
struct OverlapRule {
  tech::TypeMask first;
  tech::TypeMask second;

  friend bool operator==(const OverlapRule&, const OverlapRule&) = default;
};

struct StyleDetails {
  std::string cif_style;  // the `cifstyle` binding, empty when none
  /// Whether the binding names the current cifoutput style (drcCifStyle !=
  /// NULL): only then are mask-layer rules compiled.
  bool cif_bound = false;
  std::vector<MaskLayerRule> mask_layer_rules;
  std::vector<OverlapRule> no_overlap;
  /// Planes of the technology the DRC paint table covers.
  std::size_t plane_count = 0;
  /// drcRulesSpecified, drcRulesOptimized.
  std::uint32_t rules_specified = 0;
  std::uint32_t rules_optimized = 0;
  /// `drc rulestats` histogram: number of same-plane edge pairs with
  /// k rules (k = 0..10), pairs with more, and all rules left.
  std::vector<std::uint32_t> rules_per_pair_histogram;
  std::uint32_t rules_per_pair_overflow = 0;
  std::uint32_t edge_rules = 0;
  /// A rule line returned a negative distance (e.g. `width m1 -2 ...`):
  /// Magic's DRCTechAddRule then fails the line, and TechLoad reports "The
  /// following sections of <file> contained errors: drc" and fails the
  /// load. Hefesto reads the technology before the rules, so the flag is
  /// recorded here and `tech load` fails on LoadedRules::ok() (style.hpp).
  bool line_failed = false;
};

}  // namespace hefesto::drc
