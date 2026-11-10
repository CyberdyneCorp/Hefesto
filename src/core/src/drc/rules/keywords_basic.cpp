// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The single-layer and extension keywords of the drc section: width, area,
// off_grid, maxwidth (Q-DRC-19 lint), angles (Q-DRC-4 fix), rect_only,
// rectangle, extend and overhang (drc-rule-language "width compilation",
// "area, maxwidth and off_grid compilation", "angles compilation",
// "rect_only and rectangle compilation", "extend compilation", "overhang
// compilation").
//
// Ported from Magic VLSI 8.3.694 drc/DRCtech.c (drcWidth, drcArea,
// drcOffGrid, drcMaxwidth, drcAngles, drcRectOnly, drcRectangle, drcExtend,
// drcOverhang), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#include <array>
#include <cstdlib>
#include <string>

#include "drc/rules/compiler.hpp"
#include "drc/rules/text_util.hpp"

namespace hefesto::drc::rules_detail {

using tech::PlaneMask;
using tech::TileType;
using tech::TypeMask;

int StyleCompiler::kw_width(Tokens t) {
  const std::int32_t distance = text::c_atoi(t[2]);
  RuleFlags extra = 0;
  WhyIndex reason = 0;
  if (text::starts_with(t[3], "angle")) {
    extra = flags::kSplitTile;
    reason = why(arg(t, 4));
  } else {
    reason = why(t[3]);
  }
  const Names set = names(t[1]);
  const PlaneMask pmask = coincident(set.types, set.planes);
  const TypeMask comp = ~set.types;
  if (pmask == 0) {
    tech_error("All layers for \"width\" must be on same plane");
    return 0;
  }
  // Forward rules at (~S, S) and the split-tile reverse rules at (S, ~S)
  // go to disjoint lists, so the two passes keep Magic's insertion order.
  for_pairs(pmask, comp, set.types, [&](TileType i, TileType j, PlaneMask pset) {
    const int plane = lowest(pset);
    lists().insert(i, j, distance,
                   {make(distance, set.types, set.types, reason, distance, flags::kForward | extra, plane, plane)});
  });
  // Width checks are symmetric; the reverse rule only applies to split tiles.
  for_pairs(pmask, set.types, comp, [&](TileType i, TileType j, PlaneMask pset) {
    const int plane = lowest(pset);
    lists().insert(i, j, distance,
                   {make(distance, set.types, set.types, reason, distance, flags::kReverse | flags::kSplitTile, plane,
                         plane)});
  });
  return distance;
}

int StyleCompiler::kw_area(Tokens t) {
  const std::int32_t area = text::c_atoi(t[2]);
  const std::int32_t horizon = text::c_atoi(t[3]);
  const WhyIndex reason = why(t[4]);
  const Names set = names(t[1]);
  const PlaneMask pmask = coincident(set.types, set.planes);
  const TypeMask comp = ~set.types;
  if (pmask == 0) {
    tech_error("All layers for \"area\" must be on same plane");
    return 0;
  }
  for_pairs(pmask, comp, set.types, [&](TileType i, TileType j, PlaneMask pset) {
    if (comp.test(i) && set.types.test(j)) {
      const int plane = lowest(pset);
      lists().insert(i, j, horizon,
                     {make(horizon, set.types, set.types, reason, area, flags::kArea | flags::kForward, plane, plane)});
    }
  });
  return horizon;
}

int StyleCompiler::kw_off_grid(Tokens t) {
  const std::int32_t pitch = text::c_atoi(t[2]);
  const WhyIndex reason = why(t[3]);
  const TypeMask set = names(t[1]).types;
  const TypeMask comp = ~set;
  // off_grid needs no single plane: every pair sharing any plane.
  for_pairs(~PlaneMask{0}, comp, set, [&](TileType i, TileType j, PlaneMask pset) {
    if (comp.test(i) && set.test(j)) {
      const int plane = lowest(pset);
      lists().insert(i, j, pitch, {make(pitch, set, set, reason, 0, flags::kOffGrid | flags::kForward, plane, plane)});
      lists().insert(j, i, pitch, {make(pitch, set, set, reason, 0, flags::kOffGrid | flags::kReverse, plane, plane)});
    }
  });
  return pitch;
}

namespace {

/// The maxwidth bends option; std::nullopt for an unknown word.
std::optional<RuleFlags> bend_option(std::string_view word) {
  if (word == "bend_illegal") {
    return RuleFlags{0};
  }
  if (word == "bend_ok") {
    return flags::kBends;
  }
  if (word == "both") {
    return flags::kMaxWidthBoth;
  }
  return std::nullopt;
}

}  // namespace

std::optional<StyleCompiler::Exclude> StyleCompiler::maxwidth_exclude(Tokens t, PlaneMask pmask, RuleFlags bend) {
  Exclude exclude;
  if (t.size() != 6) {
    return exclude;  // no exclude types: plane -1
  }
  const Names excl = names(t[4]);
  const PlaneMask pmask2 = coincident(excl.types, excl.planes);
  if (pmask2 == 0) {
    tech_error("All layers for \"maxwidth\" exclude types must be on same plane.");
    return std::nullopt;
  }
  exclude.types = excl.types;
  exclude.plane = plane_count();  // no technology plane: Magic's DBNumPlanes
  for (int p = tech::kFirstUserPlane; p < plane_count() && exclude.plane == plane_count(); ++p) {
    if ((pmask2 & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0) {
      exclude.plane = p;
    }
  }
  if (exclude.plane < 64 && (pmask & tech::plane_bit(static_cast<tech::PlaneId>(exclude.plane))) != 0) {
    tech_error("Warning:  Exclude types for \"maxwidth\" are on the same plane and so cannot be checked.");
  }
  if (bend != flags::kBends) {
    // Q-DRC-19: only bend_ok honors the exclude types (both modes).
    lint("DRC-LINT-003", "Exclude types of this \"maxwidth\" rule are ignored: only \"bend_ok\" checks them "
                         "(Magic ignores them with \"" + t[3] + "\").");
  }
  return exclude;
}

int StyleCompiler::kw_maxwidth(Tokens t) {
  const std::int32_t distance = text::c_atoi(t[2]);
  const Names set = names(t[1]);
  const PlaneMask pmask = coincident(set.types, set.planes);
  if (pmask == 0) {
    tech_error("All layers for \"maxwidth\" must be on same plane");
    return 0;
  }
  // The 4-token form: BENDS unless the distance is 0 (bends are irrelevant).
  std::optional<RuleFlags> bend = distance == 0 ? 0 : flags::kBends;
  if (t.size() > 4) {
    bend = bend_option(t[3]);
    if (!bend) {
      tech_error("unknown bend option " + t[3]);
      return 0;
    }
  }
  const WhyIndex reason = why(t.size() == 4 ? t[3] : (t.size() == 6 ? t[5] : t[4]));
  const auto exclude = maxwidth_exclude(t, pmask, *bend);
  if (!exclude) {
    return 0;
  }
  const TypeMask comp = ~set.types;
  const RuleFlags f = flags::kMaxWidth | *bend;
  for_pairs(pmask, comp, set.types, [&](TileType i, TileType j, PlaneMask pset) {
    const int plane = lowest(pset);
    const Rule rule = exclude->plane == -1
                          ? make(distance, set.types, set.types, reason, distance, f, plane, plane)
                          : make(distance, set.types, exclude->types, reason, distance, f, exclude->plane, plane);
    lists().insert(i, j, distance, {rule});
  });
  return distance;
}

// --- angles ----------------------------------------------------------------

void StyleCompiler::angles_45_only(const TypeMask& set, PlaneMask pmask, WhyIndex reason, RuleFlags angles) {
  const TypeMask comp = ~set;
  for_pairs(pmask, TypeMask::all(), TypeMask::all(), [&](TileType i, TileType j, PlaneMask pset) {
    const int plane = lowest(pset);
    const TypeMask all_on_plane = plane_types(plane);
    if (set.test(i) && comp.test(j)) {  // inside corners
      const RuleFlags f = angles | flags::kBothCorners;
      lists().insert(i, j, 1, {make(1, comp, all_on_plane, reason, 1, f | flags::kForward, plane, plane)});
      lists().insert(j, i, 1, {make(1, comp, all_on_plane, reason, 1, f | flags::kReverse, plane, plane)});
    }
    if (comp.test(i) && set.test(j)) {  // outside corners
      const TypeMask not_i = ~TypeMask::of(i);
      const RuleFlags f = angles | flags::kBothCorners;
      lists().insert(i, j, 1, {make(1, not_i, all_on_plane, reason, 1, f | flags::kForward, plane, plane)});
      lists().insert(j, i, 1, {make(1, not_i, all_on_plane, reason, 1, f | flags::kReverse, plane, plane)});
    }
  });
}

namespace {

/// The angle flags of an `angles` value (Q-DRC-4 fix: "45" followed by
/// exactly one separator and "only" means 45 degrees only; the token is
/// never read past its end); std::nullopt when not 45 or 90.
std::optional<RuleFlags> angles_value(const std::string& value) {
  char* end = nullptr;
  const long angle = std::strtol(value.c_str(), &end, 10);
  if (angle == 90) {
    return flags::kAngles90;
  }
  if (angle != 45) {
    return std::nullopt;
  }
  const bool only = end != nullptr && *end != '\0' && std::string_view{end + 1} == "only";
  return only ? flags::kAngles45 : flags::kAngles45 | flags::kAngles90;
}

}  // namespace

void StyleCompiler::angles_plane_rules(const TypeMask& set, WhyIndex reason, RuleFlags angles) {
  // Edges between a type of the set and any other type of its home plane.
  for (const TileType i : members(set)) {
    const int plane = data_.home(i);
    for (TileType j = 0; j < type_count(); ++j) {
      if (i == j || set.test(j) || (j != tech::kSpace && data_.home(j) != plane)) {
        continue;
      }
      lists().insert(j, i, 1, {make(1, set, set, reason, 1, angles, plane, plane)});
      lists().insert(i, j, 1, {make(1, set, set, reason, 1, angles | flags::kReverse, plane, plane)});
    }
  }
}

int StyleCompiler::kw_angles(Tokens t) {
  const WhyIndex reason = why(t[3]);
  const auto angles = angles_value(t[2]);
  if (!angles) {
    tech_error("angles must be 45 or 90");
    return 0;
  }
  const Names set = names(t[1]);
  if (*angles == flags::kAngles45) {
    const PlaneMask pmask = coincident(set.types, set.planes);
    if (pmask == 0) {
      tech_error("All types for \"angles\"  must be on the same plane.");
      return 0;
    }
    angles_45_only(set.types, pmask, reason, *angles);
  }
  angles_plane_rules(set.types, reason, *angles);
  return 1;
}

// --- rect_only and rectangle ---------------------------------------------------

int StyleCompiler::kw_rect_only(Tokens t) {
  const WhyIndex reason = why(t[2]);
  const Names set = names(t[1]);
  const PlaneMask pmask = coincident(set.types, set.planes);
  if (pmask == 0) {
    tech_error("All types for \"rect_only\"  must be on the same plane.");
    return 0;
  }
  const TypeMask comp = ~set.types;
  for_pairs(pmask, set.types, comp, [&](TileType i, TileType j, PlaneMask pset) {
    if (set.types.test(i) && comp.test(j)) {
      const int plane = lowest(pset);
      const TypeMask all_on_plane = plane_types(plane);
      lists().insert(i, j, 1,
                     {make(1, comp, all_on_plane, reason, 1, flags::kForward | flags::kBothCorners, plane, plane)});
      lists().insert(j, i, 1,
                     {make(1, comp, all_on_plane, reason, 1, flags::kReverse | flags::kBothCorners, plane, plane)});
    }
  });
  return 1;
}

int StyleCompiler::kw_rectangle(Tokens t) {
  const WhyIndex reason = why(t[4]);
  const Names set = names(t[1]);
  const PlaneMask pmask = coincident(set.types, set.planes);
  if (pmask == 0) {
    tech_error("Layers in rectangle rule must lie in a single plane.");
    return 0;
  }
  const TypeMask comp = ~set.types;
  const auto maxwidth = text::scan_int(t[2]);
  if (!maxwidth) {
    tech_error("bad maxwidth in rectangle rule");
    return 0;
  }
  static constexpr std::array<std::string_view, 3> kParity = {"any", "even", "odd"};
  const int parity = text::lookup(t[3], kParity);
  if (parity < 0) {
    tech_error("bad [even|odd|any] selection in rectangle rule");
    return 0;
  }
  const std::int32_t even = parity - 1;  // -1 any, 0 even, 1 odd
  for_pairs(pmask, set.types, comp, [&](TileType i, TileType j, PlaneMask pset) {
    if (!set.types.test(i) || !comp.test(j)) {
      return;
    }
    const int plane = lowest(pset);
    lists().insert(i, j, 1, {make(1, comp, TypeMask::all(), reason, 1, flags::kForward, plane, plane)});
    lists().insert(j, i, 1, {make(1, comp, TypeMask::all(), reason, 1, flags::kReverse, plane, plane)});
    if (*maxwidth > 0) {
      lists().insert_plain(j, i, *maxwidth,
                           make(*maxwidth, set.types, TypeMask{}, reason, even, flags::kRectSize, plane, plane));
    }
  });
  return *maxwidth;
}

// --- extend and overhang -----------------------------------------------------------

void StyleCompiler::insert_triggered(TileType i, TileType j, const TriggeredRule& r) {
  for (const RuleFlags dir : {flags::kForward, flags::kReverse}) {
    const TileType a = dir == flags::kForward ? i : j;
    const TileType b = dir == flags::kForward ? j : i;
    lists().insert(a, b, r.distance,
                   {make(1, r.trigger_ok, TypeMask{}, r.why, 0, dir | flags::kTrigger, r.check_plane, r.edge_plane),
                    make(r.distance, r.ok, r.corner, r.why, r.cdist, dir, r.check_plane, r.edge_plane)});
  }
}

PlaneMask StyleCompiler::one_plane(const Names& n, std::string_view which, std::string_view keyword) {
  const PlaneMask pmask = coincident(n.types, n.planes);
  if (pmask == 0) {
    tech_error("All layers in " + std::string{which} + " set for \"" + std::string{keyword} +
               "\" must be on the same plane");
  }
  return pmask;
}

namespace {

enum class ExtendOption : std::uint8_t { none, exact_width, exclusive };

/// The optional fifth argument of `extend` (Magic tests prefixes, so any
/// other word is the why text).
ExtendOption extend_option(std::string_view word) {
  if (text::starts_with(word, "exact_")) {
    return ExtendOption::exact_width;
  }
  return text::starts_with(word, "exclu") ? ExtendOption::exclusive : ExtendOption::none;
}

}  // namespace

int StyleCompiler::kw_extend(Tokens t) {
  const std::int32_t distance = text::c_atoi(t[3]);
  const ExtendOption option = extend_option(t[4]);
  const bool exact = option == ExtendOption::exact_width;
  const bool exclusive = option == ExtendOption::exclusive;
  const WhyIndex reason = why(arg(t, option == ExtendOption::none ? 4 : 5));
  const Names n1 = names(t[1]);
  const PlaneMask pmask1 = one_plane(n1, "first", "extend");
  if (pmask1 == 0) {
    return 0;
  }
  const Names n2 = names(t[2]);
  const PlaneMask pmask2 = one_plane(n2, "second", "extend");
  if (pmask2 == 0) {
    return 0;
  }
  const TypeMask set_m = exclusive ? n1.types : (n1.types | n2.types);
  const TypeMask set_n = ~n1.types;
  const TypeMask set_c = ~n2.types;
  const TypeMask none;
  const auto same_plane_rules = [&](TileType i, TileType j, int plane) {
    lists().insert(i, j, distance, {make(distance, set_m, none, reason, 0, flags::kForward, plane, plane)});
    lists().insert(j, i, distance, {make(distance, set_m, none, reason, 0, flags::kReverse, plane, plane)});
    if (exact) {
      const RuleFlags out = flags::kOutside;
      lists().insert(i, j, distance, {make(distance, set_n, none, reason, 0, flags::kForward | out, plane, plane)});
      lists().insert(j, i, distance, {make(distance, set_n, none, reason, 0, flags::kReverse | out, plane, plane)});
    }
  };
  const TriggeredRule multi{set_n, set_m, none, distance, 0, reason, lowest(pmask1), 0};
  for_pairs(pmask2, n2.types, TypeMask::all(), [&](TileType i, TileType j, PlaneMask pset) {
    if ((pset & pmask1) == 0) {
      if (n2.types.test(i) && set_c.test(j)) {  // multi-plane extend
        TriggeredRule r = multi;
        r.edge_plane = lowest(pset);
        insert_triggered(i, j, r);
      }
    } else if (n2.types.test(i) && n1.types.test(j)) {
      same_plane_rules(i, j, lowest(pset & pmask1));
    }
  });
  return distance;
}

int StyleCompiler::kw_overhang(Tokens t) {
  const std::int32_t distance = text::c_atoi(t[3]);
  const WhyIndex reason = why(t[4]);
  const Names n1 = names(t[2]);  // layers1: the inside types
  const PlaneMask pmask1 = one_plane(n1, "first", "overhang");
  if (pmask1 == 0) {
    return 0;
  }
  const TypeMask set_n = ~n1.types;
  const Names n2 = names(t[1]);  // layers2: the overhanging types
  const PlaneMask pmask2 = one_plane(n2, "second", "overhang");
  if (pmask2 == 0) {
    return 0;
  }
  const TypeMask set2_inv = ~n2.types;
  if (n1.types.intersects(n2.types)) {
    tech_error("Warning:  inside and outside types have nonempty intersection.  DRC does not check edges with the "
               "same type on both sides.");
  }
  const TypeMask set_m = n1.types | n2.types;
  TypeMask set2 = n2.types;
  set2.set(tech::kSpace);
  const RuleFlags bc = flags::kBothCorners;
  const TriggeredRule multi{set_n, n1.types, n1.types, distance, distance, reason, lowest(pmask1), 0};
  for_pairs(pmask2, n1.types | set2, TypeMask::all(), [&](TileType i, TileType j, PlaneMask pset) {
    const int plane = lowest(pset);
    if ((pset & pmask1) == 0) {
      if (set2.test(i) && set2_inv.test(j)) {  // multi-plane overhang
        TriggeredRule r = multi;
        r.edge_plane = plane;
        insert_triggered(i, j, r);
      }
    } else if (n1.types.test(i) && set2.test(j)) {
      lists().insert(i, j, distance, {make(distance, set_m, set_m, reason, distance, flags::kForward | bc, plane, plane)});
      lists().insert(j, i, distance, {make(distance, set_m, set_m, reason, distance, flags::kReverse | bc, plane, plane)});
    }
  });
  return distance;
}

}  // namespace hefesto::drc::rules_detail
