// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Introspection of a compiled DRC style (see query.hpp): the `drc
// printrules` text, the `drc rulestats` histogram, the `tech drc` queries
// and the default width / spacing / surround values other areas read.
//
// Ported from Magic VLSI 8.3.694 drc/DRCprint.c (DRCPrintRulesTable,
// maskToPrint, drcGetName), drc/DRCtech.c (DRCTechRuleStats,
// DRCGetDefaultLayerWidth, DRCGetDefaultLayerSpacing,
// DRCGetDefaultLayerSurround, DRCGetDirectionalLayerSurround,
// DRCGetDefaultWideLayerSpacing) and commands/CmdTZ.c (CmdTech "drc"),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#include "hefesto/drc/rules/query.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "drc/rules/text_util.hpp"
#include "hefesto/diag/codes.hpp"
#include "hefesto/drc/rules/details.hpp"
#include "hefesto/tech/technology.hpp"
#include "tech/build_state.hpp"

namespace hefesto::drc {

namespace {

using tech::TileType;

// --- DRCPrintRulesTable -----------------------------------------------------------

/// drcGetName for every type: `printed` is what is printed ("space" for
/// space), `truncated` the first 8 characters of the short name, which
/// maskToPrint counts against its buffer.
struct TypeNames {
  std::vector<std::string> printed;
  std::vector<std::string> truncated;

  TypeNames(const tech::Technology& technology, std::size_t types) {
    for (std::size_t t = 0; t < types; ++t) {
      truncated.push_back(tech::type_short_name(technology.data(), static_cast<TileType>(t)).substr(0, 8));
      printed.push_back(t == tech::kSpace ? std::string{"space"} : truncated.back());
    }
  }
};

/// maskToPrint, including its 511-character buffer accounting.
std::string mask_to_print(const TypeNames& names, const tech::TypeMask& mask) {
  if (mask.empty()) {
    return "<none>";
  }
  std::string chain;
  long bufsize = 511;
  bool got_some = false;
  const auto strncat = [&](const std::string& src, long n) {
    const std::size_t count = n < 0 ? src.size() : std::min(src.size(), static_cast<std::size_t>(n));
    chain.append(src, 0, count);
  };
  for (std::size_t i = 0; i < names.printed.size(); ++i) {
    if (!mask.test(static_cast<TileType>(i))) {
      continue;
    }
    if (got_some) {
      strncat(",", bufsize--);
    } else {
      got_some = true;
    }
    strncat(names.printed[i], bufsize);
    bufsize -= static_cast<long>(names.truncated[i].size());
    if (bufsize <= 1) {
      if (chain.size() > 510) {
        chain.resize(511);
        chain[510] = '|';
      } else if (chain.size() == 510) {
        chain.push_back('|');
      }
      break;
    }
  }
  return chain;
}

std::string padded(const std::string& s, std::size_t width) {
  return s.size() >= width ? s : s + std::string(width - s.size(), ' ');
}

std::string plane_label(const tech::Technology& technology, tech::PlaneId plane) {
  return plane < technology.plane_count() ? std::string{technology.plane_name(plane)} : std::string{"(null)"};
}

struct FlagWord {
  RuleFlags flag;
  std::string_view word;
};

constexpr FlagWord kFlagWords[] = {
    {flags::kReverse, " reverse"},         {flags::kBothCorners, " both-corners"},
    {flags::kTrigger, " trigger"},         {flags::kArea, " area"},
    {flags::kMaxWidth, " maxwidth"},       {flags::kBends, " bends"},
    {flags::kRectSize, " rect-size"},      {flags::kAngles45, " angles_45"},
    {flags::kAngles90, " angles_90"},      {flags::kDerivedEdge, " derived-edge"},
    {flags::kDerivedTarget, " derived-target"},
};

void print_rule(const Rule& rule, const tech::Technology& technology, const TypeNames& names, std::ostream& out) {
  out << rule.dist << " x " << rule.cdist << "   " << mask_to_print(names, rule.ok) << " ("
      << plane_label(technology, rule.check_plane) << ")\n";
  out << "                           " << mask_to_print(names, rule.corner);
  if (rule.flags > 0) {
    out << "\n                          ";
  }
  for (const FlagWord& f : kFlagWords) {
    if ((rule.flags & f.flag) != 0) {
      out << f.word;
    }
  }
  out << "\n";
}

}  // namespace

namespace {

/// The rules of pair (i, j); true when there were any.
bool print_pair(const RuleSet& rules, const tech::Technology& technology, const TypeNames& names, std::size_t i,
                std::size_t j, std::ostream& out) {
  bool first = true;
  for (const Rule& rule : rules.rules(static_cast<TileType>(i), static_cast<TileType>(j))) {
    if (first) {
      out << padded(names.printed[i], 8) << ' ' << padded(names.printed[j], 8) << "  ";
      first = false;
    } else {
      out << "                   ";
    }
    print_rule(rule, technology, names, out);
  }
  return !first;
}

/// The overlaps that are illegal between subcells (plane 0 of the DRC paint table).
void print_overlaps(const RuleSet& rules, const TypeNames& names, std::ostream& out) {
  const std::size_t types = names.printed.size();
  for (std::size_t i = 0; i < types; ++i) {
    for (std::size_t j = 0; j < types; ++j) {
      const bool error_type = i == tech::kErrorS || j == tech::kErrorS;
      if (!error_type && rules.paint_result(0, static_cast<TileType>(j), static_cast<TileType>(i)) == tech::kErrorS) {
        out << "Tile type " << names.printed[i] << " can't overlap type " << names.printed[j] << ".\n";
      }
    }
  }
}

}  // namespace

void print_rules(const RuleSet& rules, const tech::Technology& technology, std::ostream& out) {
  const std::size_t types = std::min(rules.type_count(), technology.type_count());
  const TypeNames names(technology, types);
  for (std::size_t i = 0; i < types; ++i) {
    bool got_any = false;
    for (std::size_t j = 0; j < types; ++j) {
      got_any = print_pair(rules, technology, names, i, j, out) || got_any;
    }
    if (got_any) {
      out << "\n";
    }
  }
  print_overlaps(rules, names, out);
  if (!rules.exact_overlap_types().empty()) {
    out << "Types that must overlap exactly: " << mask_to_print(names, rules.exact_overlap_types()) << "\n";
  }
}

// --- DRCTechRuleStats -----------------------------------------------------------

void rule_stats(const RuleSet& rules, std::ostream& out) {
  const StyleDetails* details = rules.details();
  const std::uint32_t specified = details != nullptr ? details->rules_specified : 0;
  const std::uint32_t optimized = details != nullptr ? details->rules_optimized : 0;
  const auto left = static_cast<std::uint32_t>(rules.all_rules().size());
  out << "Total number of rules specifed in tech file: " << specified << "\n";
  out << "Edge rules optimized away: " << optimized << "\n";
  out << "Edge rules left in database: " << left << "\n";
  out << "Histogram of # edges vs. rules per edge:\n";
  for (std::size_t i = 0; i <= 10; ++i) {
    const std::uint32_t count =
        details != nullptr && i < details->rules_per_pair_histogram.size() ? details->rules_per_pair_histogram[i] : 0;
    char line[64];
    std::snprintf(line, sizeof line, "  %2zu rules/edge: %u.\n", i, count);
    out << line;
  }
  char last[64];
  std::snprintf(last, sizeof last, " >%2d rules/edge: %u.\n", 10, details != nullptr ? details->rules_per_pair_overflow : 0U);
  out << last;
}

// --- default values (DRCGetDefault*) ---------------------------------------------------

std::int32_t default_width(const RuleSet& rules, const tech::Technology& technology, TileType type) {
  const auto list = rules.rules(tech::kSpace, type);
  const int home = technology.data().home(type);
  std::int32_t width = 0;
  for (std::size_t k = 0; k < list.size(); ++k) {
    const Rule& rule = list[k];
    if (rule.has(flags::kTrigger)) {
      ++k;  // skip the triggered rule too
      continue;
    }
    if (rule.has(flags::kArea) || (rule.flags & (flags::kReverse | flags::kMaxWidth | flags::kSplitTile)) != 0) {
      continue;
    }
    if (rule.ok.test(type) && rule.ok == rule.corner && static_cast<int>(rule.check_plane) == home &&
        rule.dist == rule.cdist) {
      width = rule.dist;
    }
  }
  return width;
}

std::int32_t default_spacing(const RuleSet& rules, const tech::Technology& technology, TileType type1,
                             TileType type2) {
  const auto list = rules.rules(type1, tech::kSpace);
  std::int32_t spacing = 0;
  for (std::size_t k = 0; k < list.size(); ++k) {
    const Rule& rule = list[k];
    if (rule.has(flags::kTrigger)) {
      ++k;  // skip widespacing rules
      continue;
    }
    if (rule.has(flags::kArea) || rule.reverse() || rule.ok.test(type2)) {
      continue;
    }
    if ((technology.type_planes(type2) & tech::plane_bit(rule.check_plane)) != 0 && rule.dist == rule.cdist) {
      spacing = rule.dist;
    }
  }
  return spacing;
}

std::int32_t default_surround(const RuleSet& rules, const tech::Technology& technology, TileType type1,
                              TileType type2) {
  std::int32_t surround = 0;
  for (const Rule& rule : rules.rules(type1, tech::kSpace)) {
    if (!rule.reverse() && !rule.ok.test(tech::kSpace) &&
        (technology.type_planes(type2) & tech::plane_bit(rule.check_plane)) != 0 && rule.dist == rule.cdist) {
      surround = rule.dist;
    }
  }
  if (surround > 0) {
    return surround;
  }
  for (const Rule& rule : rules.rules(tech::kSpace, type1)) {
    if (!rule.reverse() && rule.ok.test(tech::kSpace) && !rule.ok.test(type1) && rule.corner.test(type2) &&
        rule.has(flags::kBothCorners) && rule.edge_plane == rule.check_plane && rule.dist == rule.cdist) {
      surround = rule.dist;
    }
  }
  return surround;
}

std::int32_t directional_surround(const RuleSet& rules, TileType type1, TileType type2) {
  std::int32_t surround = 0;
  for (const Rule& rule : rules.rules(type1, tech::kSpace)) {
    if (rule.has(flags::kTrigger) && !rule.ok.test(tech::kSpace) && rule.ok.test(type2) &&
        rule.check_plane == rule.edge_plane && rule.cdist == 0) {
      surround = rule.dist;
    }
  }
  return surround;
}

std::int32_t default_wide_spacing(const RuleSet& rules, const tech::Technology& technology, TileType type,
                                  std::int32_t width) {
  std::int32_t spacing = 0;
  bool wide = false;
  for (const Rule& rule : rules.rules(type, tech::kSpace)) {
    if (rule.has(flags::kTrigger)) {
      wide = true;
      if (width > 0 && rule.dist > width) {
        return spacing;
      }
    }
    if (wide && !rule.reverse() && !rule.ok.test(type) &&
        (technology.type_planes(type) & tech::plane_bit(rule.check_plane)) != 0 && rule.dist == rule.cdist) {
      spacing = rule.dist;
    }
    if (!rule.has(flags::kTrigger)) {
      wide = false;
    }
  }
  return spacing;
}

// --- tech drc --------------------------------------------------------------------

namespace {

/// DBTechNoisyNameType: the type, or the diagnostic with "No such layer".
diag::Outcome<TileType> query_layer(const tech::Technology& technology, const std::string& name) {
  const int type = tech::name_type(technology.data(), name);
  if (type >= 0) {
    return static_cast<TileType>(type);
  }
  const std::string what = type == tech::kAmbiguous ? "Ambiguous layer (type) name \"" : "Unrecognized layer (type) name \"";
  return diag::make("TECH-CMD-006", what + name + "\"\nNo such layer " + name);
}

}  // namespace

diag::Outcome<cmd::Value> tech_drc_query(const RuleSet& rules, const tech::Technology& technology,
                                         std::span<const std::string> args) {
  // args: <rule> <layer> [<layer2>] (Magic's argv[2..])
  if (args.size() < 2) {
    return diag::make("TECH-CMD-001", "wrong number of arguments to command \"tech\"\n  Type \":tech help\" for help.");
  }
  const auto t1 = query_layer(technology, args[1]);
  if (!t1.ok()) {
    return t1.error();
  }
  const std::string& rule = args[0];
  const bool needs_two = text::starts_with(rule, "surr") || text::starts_with(rule, "direc");
  const bool spacing = text::starts_with(rule, "spac");
  if (!text::starts_with(rule, "width") && !spacing && !needs_two) {
    return cmd::Value{};  // an unknown rule keyword returns nothing
  }
  TileType t2 = t1.value();
  if (args.size() >= 3 && (spacing || needs_two)) {
    const auto second = query_layer(technology, args[2]);
    if (!second.ok()) {
      return second.error();
    }
    t2 = second.value();
  } else if (needs_two) {
    return diag::make("DRC-TECH-005", "Requires two layer types.");
  }
  if (text::starts_with(rule, "width")) {
    return cmd::Value{std::int64_t{default_width(rules, technology, t1.value())}};
  }
  if (spacing) {
    return cmd::Value{std::int64_t{default_spacing(rules, technology, t1.value(), t2)}};
  }
  if (text::starts_with(rule, "surr")) {
    return cmd::Value{std::int64_t{default_surround(rules, technology, t1.value(), t2)}};
  }
  return cmd::Value{std::int64_t{directional_surround(rules, t1.value(), t2)}};
}

}  // namespace hefesto::drc
