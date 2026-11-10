// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The rule-list optimizations of drcTechFinalStyle (drc-rule-language
// "Illegal-edge rule pruning", "Covered rule elimination") and the
// distance scaling of drcScaleDown / drcScaleUp ("Scale factor and
// distance units", Q-DRC-1).
//
//   - Illegal-edge pruning keeps, per edge plane, the first rule an edge
//     always violates (its TRIGGER flag cleared) and drops the other rules
//     of that pair on the same edge plane. Replicated in both modes.
//   - Covered-rule elimination removes a rule X when a later rule Y covers
//     it; with magic-compat ON Magic's coded condition is replicated (a
//     BOTHCORNERS rule may be removed for a single-corner one, and the edge
//     plane and the MANHATTAN, RUNLENGTH and DERIVED flags are not
//     compared); OFF requires Y to have BOTHCORNERS whenever X has it and
//     compares those fields (Q-DRC-5).
//   - drcScaleDown keeps Magic's one-byte remainders (drcc_mod, drcc_cmod),
//     including their wrap for factors above 256 (Q-DRC-1).
//
// Ported from Magic VLSI 8.3.694 drc/DRCtech.c (drcTechFinalStyle,
// drcScaleDown, drcScaleUp), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#include <algorithm>
#include <cstdint>

#include "drc/rules/compiler.hpp"
#include "drc/rules/text_util.hpp"

namespace hefesto::drc::rules_detail {

using tech::TileType;

// --- illegal-edge pruning ---------------------------------------------------------

namespace {

/// Whether the edge (i, j) itself violates `rule`.
bool always_violated(const Rule& rule, TileType i, TileType j, const tech::Technology& tech) {
  if ((rule.flags & (flags::kNonStandard | flags::kOutside)) != 0) {
    return false;
  }
  const TileType checked = rule.reverse() ? i : j;  // the type on the checked side
  if (checked == tech::kSpace || rule.ok.test(checked)) {
    return false;
  }
  // Rules whose OK types are on another plane count only when the checked
  // type is also on the check plane.
  if (rule.check_plane != rule.edge_plane) {
    return (tech.type_planes(checked) & tech::plane_bit(rule.check_plane)) != 0;
  }
  return true;
}

}  // namespace

std::size_t prune_illegal_edges(std::vector<Rule>& rules, TileType i, TileType j, const tech::Technology& tech) {
  std::size_t removed = 0;
  std::size_t k = 0;
  while (k < rules.size()) {
    if (!always_violated(rules[k], i, j, tech)) {
      ++k;
      continue;
    }
    Rule keep = rules[k];
    keep.flags &= ~flags::kTrigger;  // the kept rule can't be a trigger
    std::vector<Rule> kept;
    std::size_t keep_index = 0;
    for (std::size_t n = 0; n < rules.size(); ++n) {
      if (n == k) {
        keep_index = kept.size();
        kept.push_back(keep);
      } else if (rules[n].edge_plane != keep.edge_plane) {
        kept.push_back(rules[n]);
      } else {
        ++removed;
      }
    }
    rules = std::move(kept);
    k = keep_index + 1;
  }
  return removed;
}

// --- covered-rule elimination --------------------------------------------------------

namespace {

bool same_flag(const Rule& a, const Rule& b, RuleFlags f) {
  return ((a.flags & f) != 0) == ((b.flags & f) != 0);
}

/// Whether `next` (Y) covers `rule` (X) (drcTechFinalStyle, Q-DRC-5).
bool covers(const Rule& next, const Rule& rule, bool magic_compat) {
  if (!rule.ok.contains(next.ok) || !(rule.corner == next.corner) || rule.dist > next.dist ||
      rule.cdist > next.cdist || rule.check_plane != next.check_plane || rule.exception != next.exception ||
      !same_flag(rule, next, flags::kReverse) || next.nonstandard()) {
    return false;
  }
  const bool x_bc = rule.has(flags::kBothCorners);
  const bool y_bc = next.has(flags::kBothCorners);
  if (magic_compat ? (y_bc && !x_bc) : (x_bc && !y_bc)) {
    return false;
  }
  if (rule.dist == next.dist && !same_flag(rule, next, flags::kOutside)) {
    return false;
  }
  if (!same_flag(rule, next, flags::kSplitTile)) {
    return false;
  }
  if (magic_compat) {
    return true;
  }
  return rule.edge_plane == next.edge_plane && same_flag(rule, next, flags::kManhattan) &&
         same_flag(rule, next, flags::kRunLength) && same_flag(rule, next, flags::kDerivedEdge) &&
         same_flag(rule, next, flags::kDerivedTarget);
}

/// The first later rule covering rules[x], skipping triggered pairs.
bool covered_later(const std::vector<Rule>& rules, std::size_t x, bool magic_compat) {
  std::size_t n = x + 1;
  while (n < rules.size()) {
    if (rules[n].has(flags::kTrigger)) {
      n += 2;  // a triggered rule never covers another
      continue;
    }
    if (covers(rules[n], rules[x], magic_compat)) {
      return true;
    }
    ++n;
  }
  return false;
}

}  // namespace

std::size_t eliminate_covered(std::vector<Rule>& rules, bool magic_compat) {
  std::size_t removed = 0;
  std::size_t k = 0;
  while (k < rules.size()) {
    // Optimize on the triggered rule, not on its trigger.
    const bool triggered = rules[k].has(flags::kTrigger) && k + 1 < rules.size();
    const std::size_t x = triggered ? k + 1 : k;
    if (rules[x].nonstandard() || !covered_later(rules, x, magic_compat)) {
      k = x + 1;
      continue;
    }
    // Remove the rule, with its trigger when it is triggered.
    rules.erase(rules.begin() + static_cast<std::ptrdiff_t>(k), rules.begin() + static_cast<std::ptrdiff_t>(x + 1));
    removed += 1;  // drcRulesOptimized counts one per removal
  }
  return removed;
}

// --- drcScaleDown / drcScaleUp --------------------------------------------------------

void scale_down(std::vector<Rule>& rules, std::int32_t factor, bool magic_compat) {
  if (factor <= 1) {
    return;
  }
  for (Rule& rule : rules) {
    if (rule.dist > 0) {
      const std::int32_t dist = rule.dist;
      rule.dist /= factor;
      rule.dist_mod = static_cast<std::uint8_t>(dist % factor);
      // Magic tests the stored byte: a remainder that wraps to 0 loses the
      // ceiling (Q-DRC-1, magic-compat ON); OFF tests the true remainder.
      const bool remainder = magic_compat ? rule.dist_mod != 0 : dist % factor != 0;
      if (remainder && !rule.has(flags::kMaxWidth)) {
        ++rule.dist;
      }
    }
    if (rule.cdist > 0) {
      const bool area = rule.has(flags::kArea);
      const std::int64_t locscale = area ? std::int64_t{factor} * factor : factor;
      const std::int64_t dist = rule.cdist;
      rule.cdist = static_cast<std::int32_t>(dist / locscale);
      const std::int64_t cmod = dist % locscale;
      if (cmod != 0) {
        ++rule.cdist;
        rule.cdist_mod = static_cast<std::uint8_t>(area ? cmod / factor : cmod);
      }
    }
  }
}

void scale_up(std::vector<Rule>& rules, std::int32_t factor) {
  if (factor <= 1) {
    return;
  }
  for (Rule& rule : rules) {
    if (rule.dist > 0) {
      if (rule.dist_mod != 0 && !rule.has(flags::kMaxWidth)) {
        --rule.dist;
      }
      rule.dist = text::clamp_int(std::int64_t{rule.dist} * factor + rule.dist_mod);
      rule.dist_mod = 0;
    }
    if (rule.cdist > 0) {
      if (rule.cdist_mod != 0) {
        --rule.cdist;
      }
      std::int64_t cdist = std::int64_t{rule.cdist} * factor;
      if (rule.has(flags::kArea)) {
        cdist = cdist * factor + std::int64_t{rule.cdist_mod} * factor;
      } else {
        cdist += rule.cdist_mod;
      }
      rule.cdist = text::clamp_int(cdist);
      rule.cdist_mod = 0;
    }
  }
}

}  // namespace hefesto::drc::rules_detail
