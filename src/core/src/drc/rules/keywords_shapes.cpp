// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The edge and surround keywords of the drc section (drc-rule-language
// "edge and edge4way compilation", "surround compilation"), with the
// Q-DRC-18 lint for cross-plane `exact_width`.
//
// Ported from Magic VLSI 8.3.694 drc/DRCtech.c (drcEdge, drcSurround),
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#include <algorithm>
#include <cstdlib>
#include <string>

#include "drc/rules/compiler.hpp"
#include "drc/rules/text_util.hpp"

namespace hefesto::drc::rules_detail {

using tech::PlaneMask;
using tech::TileType;
using tech::TypeMask;

// --- edge / edge4way ----------------------------------------------------------

namespace {

PlaneMask plane_mask_of(int plane) {
  return plane >= 0 && plane < 64 ? tech::plane_bit(static_cast<tech::PlaneId>(plane)) : 0;
}

}  // namespace

void StyleCompiler::edge_pair_rules(TileType i, TileType j, PlaneMask pset, PlaneMask ok_planes, const Rule& proto,
                                    bool fourway) {
  int plane = 0;
  int check = 0;
  if ((pset & ok_planes) != 0) {
    plane = lowest(pset & ok_planes);
    check = plane;
  } else {
    plane = lowest(pset);
    check = lowest(ok_planes);
  }
  const RuleFlags base = proto.flags;
  lists().insert(i, j, proto.dist,
                 {make(proto.dist, proto.ok, proto.corner, proto.why, proto.cdist,
                       flags::kForward | base | (fourway ? flags::kBothCorners : 0), check, plane)});
  if (fourway) {
    lists().insert(j, i, proto.dist,
                   {make(proto.dist, proto.ok, proto.corner, proto.why, proto.cdist,
                         flags::kReverse | base | flags::kBothCorners, check, plane)});
  }
}

std::optional<PlaneMask> StyleCompiler::edge_ok_planes(const Names& ok, const std::string& ok_types, PlaneMask edge_planes,
                                                       int plane_arg) {
  // plane_arg: the optional `plane` argument's plane, -1 when it does not
  // resolve, -2 when there is none.
  PlaneMask ok_planes = coincident(ok.types, ok.planes);
  if (ok_planes == 0 || ok_planes == tech().type_planes(tech::kSpace)) {
    // OK = space (or "0") takes the edge plane, or the `plane` argument.
    if (ok.types == TypeMask::of(tech::kSpace)) {
      if (plane_arg != -2) {
        ok_planes = plane_mask_of(plane_arg);
      } else {
        tech_error("OK types \"" + ok_types + "\" in more than one plane.\n\tAssuming same plane (" +
                   std::string{tech().plane_name(static_cast<tech::PlaneId>(lowest(edge_planes)))} + ") as edge.");
        ok_planes = edge_planes;
      }
    } else if (ok_types == "0") {
      ok_planes = edge_planes;
    } else {
      tech_error("All OK types must lie in one plane.");
      return std::nullopt;
    }
  }
  if (plane_arg != -2 && plane_mask_of(plane_arg) != ok_planes) {
    tech_error("Ignoring bad plane argument.");
  }
  return ok_planes;
}

int StyleCompiler::kw_edge(Tokens t) {
  const std::int32_t distance = text::c_atoi(t[3]);
  const std::string& ok_types = t[4];
  const std::int32_t cdist = text::c_atoi(t[6]);
  std::size_t argc = t.size();
  RuleFlags extra = 0;
  WhyIndex reason = 0;
  int plane_arg = -1;
  if (argc > 7 && t[7] == "manhattan_dist") {
    extra = flags::kManhattan;
    reason = why(arg(t, 8));
    if (argc == 10) {
      plane_arg = std::max(plane_named(t[9]), -1);
    }
    --argc;
  } else {
    reason = why(t[7]);
    if (argc == 9) {
      plane_arg = std::max(plane_named(t[8]), -1);
    }
  }
  const bool fourway = t[0] == "edge4way";  // exact comparison, as in Magic
  const Names n1 = names(t[1]);
  PlaneMask pmask1 = coincident(n1.types, n1.planes);
  const Names n2 = names(t[2]);
  pmask1 &= coincident(n2.types, n2.planes);
  if (pmask1 == 0) {
    tech_error("All edges in edge rule must lie in shared planes.");
    return 0;
  }
  if (n1.types.intersects(n2.types)) {
    tech_error("Warning:  types1 and types2 have nonempty intersection.  DRC does not check edges with the same "
               "type on both sides.");
  }
  const Names corner = names(t[5]);
  if ((coincident(corner.types, corner.planes) & pmask1) == 0) {
    tech_error("Corner types aren't in same plane as edges.");
    return 0;
  }
  const Names ok = names(ok_types);
  const auto ok_planes = edge_ok_planes(ok, ok_types, pmask1, argc == 9 ? plane_arg : -2);
  if (!ok_planes) {
    return 0;
  }
  Rule proto;
  proto.dist = distance;
  proto.cdist = cdist;
  proto.ok = ok.types;
  proto.corner = corner.types;
  proto.why = reason;
  proto.flags = extra;
  for_pairs(pmask1, n1.types, n2.types, [&](TileType i, TileType j, PlaneMask pset) {
    if (n1.types.test(i) && n2.types.test(j)) {
      edge_pair_rules(i, j, pset, *ok_planes, proto, fourway);
    }
  });
  return std::max(distance, cdist);
}

// --- surround --------------------------------------------------------------

struct StyleCompiler::SurroundArgs {
  std::int32_t distance = 0;
  std::int32_t distance2 = 0;
  WhyIndex why = 0;
  TypeMask inner;   // setM: layers1
  TypeMask outer;   // set2: layers2
  PlaneMask inner_planes = 0;  // pmask
  PlaneMask outer_planes = 0;  // pmask2
};

void StyleCompiler::surround_directional(const SurroundArgs& a) {
  const TypeMask both = a.inner | a.outer;  // setR
  const TypeMask not_inner = ~a.inner;
  const TypeMask none;
  for_pairs(a.inner_planes, a.inner, not_inner, [&](TileType i, TileType j, PlaneMask pset) {
    if (!a.inner.test(i) || !not_inner.test(j)) {
      return;
    }
    const int plane1 = lowest(pset);
    const int plane2 = lowest(a.outer_planes);
    lists().insert(i, j, a.distance,
                   {make(a.distance2, a.outer, none, a.why, 0, flags::kForward | flags::kTrigger, plane2, plane1),
                    make(a.distance, both, TypeMask::all(), a.why, a.distance,
                         flags::kReverse | flags::kBothCorners, plane2, plane1)});
    lists().insert(j, i, a.distance,
                   {make(a.distance2, a.outer, none, a.why, 0, flags::kReverse | flags::kTrigger, plane2, plane1),
                    make(a.distance, both, TypeMask::all(), a.why, a.distance,
                         flags::kForward | flags::kBothCorners, plane2, plane1)});
  });
}

void StyleCompiler::surround_base(const SurroundArgs& a) {
  const TypeMask not_inner = ~a.inner;  // invM
  const TypeMask not_outer = ~a.outer;  // set1
  const RuleFlags f = flags::kBothCorners | flags::kDerivedTarget;
  for_pairs(a.outer_planes, not_outer, a.outer, [&](TileType i, TileType j, PlaneMask pset) {
    if (!not_outer.test(i) || !a.outer.test(j)) {
      return;
    }
    const int plane1 = lowest(a.inner_planes);
    const int plane2 = lowest(pset);
    lists().insert(i, j, a.distance,
                   {make(a.distance, not_inner, a.outer, a.why, a.distance, flags::kForward | f, plane1, plane2)});
    lists().insert(j, i, a.distance,
                   {make(a.distance, not_inner, a.outer, a.why, a.distance, flags::kReverse | f, plane1, plane2)});
  });
}

void StyleCompiler::surround_exact(const SurroundArgs& a) {
  const TypeMask not_outer = ~a.outer;
  const RuleFlags f = flags::kBothCorners | flags::kOutside;
  for_pairs(a.inner_planes, a.inner, a.outer, [&](TileType i, TileType j, PlaneMask pset) {
    if (!a.inner.test(i) || !a.outer.test(j)) {
      return;
    }
    const int plane1 = lowest(pset);
    lists().insert(i, j, a.distance,
                   {make(a.distance, not_outer, a.outer, a.why, a.distance, flags::kForward | f, plane1, plane1)});
    lists().insert(j, i, a.distance,
                   {make(a.distance, not_outer, a.outer, a.why, a.distance, flags::kReverse | f, plane1, plane1)});
  });
  // Q-DRC-18: pairs with no common plane get no maximum-surround rule.
  for (TileType i = tech::kFirstUserType; i < type_count(); ++i) {
    for (TileType j = tech::kFirstUserType; j < type_count(); ++j) {
      if (i != j && a.inner.test(i) && a.outer.test(j) && (same_plane(i, j) & a.inner_planes) == 0) {
        lint("DRC-LINT-002", "\"surround ... exact_width\": no maximum-surround check between \"" +
                                 std::string{tech().type_name(i)} + "\" and \"" + std::string{tech().type_name(j)} +
                                 "\", which share no plane (only absence is checked, as in Magic).");
        return;
      }
    }
  }
}

void StyleCompiler::surround_presence(const SurroundArgs& a) {
  const RuleFlags bc = flags::kBothCorners;
  if ((a.inner_planes & a.outer_planes) != 0) {
    const TypeMask outside = ~(a.inner | a.outer);
    const TypeMask none;
    for_pairs(a.inner_planes & a.outer_planes, a.inner, outside, [&](TileType i, TileType j, PlaneMask pset) {
      const int plane1 = lowest(pset);
      if (a.inner.test(i) && outside.test(j)) {
        lists().insert(i, j, a.distance,
                       {make(a.distance, none, outside, a.why, a.distance, flags::kForward | bc, plane1, plane1)});
        lists().insert(j, i, a.distance,
                       {make(a.distance, none, outside, a.why, a.distance, flags::kReverse | bc, plane1, plane1)});
      }
    });
    return;
  }
  const TypeMask not_inner = ~a.inner;
  for_pairs(a.inner_planes, a.inner, not_inner, [&](TileType i, TileType j, PlaneMask pset) {
    if (!a.inner.test(i) || !not_inner.test(j)) {
      return;
    }
    const int plane1 = lowest(pset);
    const int plane2 = lowest(a.outer_planes);
    lists().insert(i, j, a.distance,
                   {make(a.distance, a.outer, not_inner, a.why, a.distance, flags::kForward | bc, plane2, plane1)});
    lists().insert(j, i, a.distance,
                   {make(a.distance, a.outer, not_inner, a.why, a.distance, flags::kReverse | bc, plane2, plane1)});
  });
}

int StyleCompiler::kw_surround(Tokens t) {
  SurroundArgs a;
  a.distance = text::c_atoi(t[3]);
  a.why = why(t[5]);  // registered first in every form, as in Magic
  std::string_view presence;
  if (t.size() == 7) {
    char* end = nullptr;
    const long d2 = std::strtol(t[4].c_str(), &end, 10);
    if (end != nullptr && *end != '\0') {
      tech_error("Incorrect arguments in directional \"surround\" rule");
      return 0;
    }
    a.distance2 = text::clamp_int(d2);
    presence = t[5];
    a.why = why(t[6]);
  } else {
    a.distance2 = a.distance;
    presence = t[4];
    a.why = why(t[5]);
  }
  const Names inner = names(t[1]);
  a.inner = inner.types;
  a.inner_planes = coincident(inner.types, inner.planes);
  if (a.inner_planes == 0) {
    tech_error("Inside types in \"surround\" must be on the same plane");
    return 0;
  }
  const Names outer = names(t[2]);
  a.outer = outer.types;
  a.outer_planes = coincident(outer.types, outer.planes);
  if (a.outer_planes == 0) {
    tech_error("Outside types in \"surround\" must be on the same plane");
    return 0;
  }
  const bool exact = text::starts_with(presence, "exact_");
  const bool directional = !exact && text::starts_with(presence, "directional");
  if (directional) {
    surround_directional(a);
  } else {
    surround_base(a);
  }
  if (exact) {
    surround_exact(a);
  }
  if (exact || presence == "absence_illegal") {
    surround_presence(a);
  }
  return a.distance;
}

}  // namespace hefesto::drc::rules_detail
