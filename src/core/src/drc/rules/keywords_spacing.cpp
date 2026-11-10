// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The spacing keywords of the drc section: `spacing` (touching_ok,
// touching_illegal with the coincidence rule, manhattan_dist, surround_ok,
// corner_ok), `widespacing` (runlength, trigger pairs), the DERIVED flags
// and multiplane rules (Q-DRC-3: Magic compiles only the first plane pair
// with magic-compat ON, every pair OFF, and a lint warning in both modes)
// (drc-rule-language "spacing compilation", "widespacing compilation").
//
// Ported from Magic VLSI 8.3.694 drc/DRCtech.c (drcSpacing, drcMaskSpacing,
// drcSpacing3, drcIsDerivedType, drcDerivedFlags), Copyright (C) 1985,
// 1990 Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: RULES (openspec/changes/m3-drc/design.md D2).

#include <string>
#include <utility>
#include <vector>

#include "drc/rules/compiler.hpp"
#include "drc/rules/text_util.hpp"

namespace hefesto::drc::rules_detail {

using tech::PlaneMask;
using tech::TileType;
using tech::TypeMask;

/// The arguments of drcMaskSpacing after the adjacency is decoded.
struct StyleCompiler::SpacingArgs {
  TypeMask set1;
  TypeMask set2;
  std::int32_t wwidth = 0;
  std::int32_t distance = 0;
  std::int32_t runlength = -1;
  WhyIndex why = 0;
  bool widerule = false;
  // decoded adjacency
  bool need_reverse = false;
  bool need_trigger = false;
  bool surround_ok = false;
  RuleFlags manhattan = 0;
  TypeMask set_r;          // setR
  TypeMask set_r_reverse;  // setRreverse
  PlaneMask pmask1 = 0;
  PlaneMask pmask2 = 0;
  std::string_view adjacency;
  bool multiplane = false;
  // Membership of each type (fast tests in the pair loop) and the
  // DERIVED_TARGET flag of each side, which does not depend on the pair.
  std::vector<char> in1;
  std::vector<char> in2;
  std::vector<char> in_r;
  std::vector<char> in_r_reverse;
  RuleFlags target1 = 0;
  RuleFlags target2 = 0;
};

// --- DERIVED flags -------------------------------------------------------------

bool StyleCompiler::is_derived_type(TileType type, const TypeMask& set, int plane) const {
  if (plane < 0 || plane >= plane_count()) {
    return false;
  }
  const TileType residue = residues_[type * tech::kMaxPlanes + static_cast<std::size_t>(plane)];
  return residue != tech::kSpace && !set.test(residue);  // kSpace for a non-contact
}

RuleFlags StyleCompiler::derived_flags(TileType edge_type, const TypeMask& edge_set, int edge_plane,
                                       const TypeMask& target_set, int target_plane) const {
  RuleFlags f = 0;
  if (is_derived_type(edge_type, edge_set, edge_plane)) {
    f |= flags::kDerivedEdge;
  }
  for (TileType t = tech::kFirstUserType; t < type_count(); ++t) {
    if (target_set.test(t) && on_plane(t, target_plane) && is_derived_type(t, target_set, target_plane)) {
      f |= flags::kDerivedTarget;
      break;
    }
  }
  return f;
}

// --- drcMaskSpacing ------------------------------------------------------------

void StyleCompiler::spacing_set1_side(const SpacingArgs& a, TileType i, TileType j, PlaneMask pmask1,
                                      PlaneMask pmask2) {
  if (a.in1[i] == 0 || a.in_r[j] == 0) {
    return;
  }
  const PlaneMask pset = same_plane(i, j) & pmask1;
  if (pset == 0) {
    return;
  }
  const int plane = lowest(pset);
  const int plane2 = lowest(pmask2);
  const TypeMask tmp1 = plane_types(plane2) & ~a.set2;
  const TypeMask tmp2 = plane_types(plane) & a.set_r;
  const RuleFlags bc = a.need_reverse ? flags::kBothCorners : 0;
  const bool run = a.runlength > 0;
  const std::int32_t wide_cdist = run ? a.runlength : a.distance;
  const RuleFlags run_flag = run ? flags::kRunLength : 0;
  const RuleFlags wide_trigger = flags::kMaxWidth | flags::kTrigger | flags::kBends;
  const RuleFlags derived =
      a.manhattan != 0 ? 0 : (is_derived_type(i, a.set1, plane) ? flags::kDerivedEdge : 0) | a.target1;
  if (a.widerule) {
    lists().insert(i, j, a.distance,
                   {make(a.wwidth, a.set1, a.set1, a.why, wide_cdist, flags::kReverse | wide_trigger, plane2, plane),
                    make(a.distance, tmp1, tmp2, a.why, wide_cdist, run_flag | flags::kForward | bc, plane2, plane)});
    lists().insert(j, i, a.distance,
                   {make(a.wwidth, a.set1, a.set1, a.why, wide_cdist, flags::kForward | wide_trigger, plane2, plane),
                    make(a.distance, tmp1, tmp2, a.why, wide_cdist,
                         run_flag | flags::kReverse | flags::kBothCorners, plane2, plane)});
    return;
  }
  if (a.need_trigger) {
    lists().insert(i, j, a.distance,
                   {make(1, a.set2, tmp2, a.why, 1, flags::kForward | flags::kTrigger, plane2, plane),
                    make(a.distance, tmp1, tmp2, a.why, a.wwidth, flags::kForward | flags::kBothCorners | bc, plane2,
                         plane)});
    lists().insert(j, i, a.distance,
                   {make(1, a.set2, tmp2, a.why, 1, flags::kReverse | flags::kTrigger, plane2, plane),
                    make(a.distance, tmp1, tmp2, a.why, a.wwidth, flags::kReverse | flags::kBothCorners, plane2,
                         plane)});
    return;
  }
  lists().insert(i, j, a.distance,
                 {make(a.distance, tmp1, tmp2, a.why, a.wwidth, flags::kForward | a.manhattan | derived | bc, plane2,
                       plane)});
  if (a.need_reverse) {
    lists().insert(j, i, a.distance,
                   {make(a.distance, tmp1, tmp2, a.why, a.wwidth,
                         flags::kReverse | flags::kBothCorners | a.manhattan | derived, plane2, plane)});
  }
}

void StyleCompiler::spacing_set2_side(const SpacingArgs& a, TileType i, TileType j, PlaneMask pmask1,
                                      PlaneMask pmask2) {
  if (a.surround_ok || a.in2[i] == 0 || a.in_r_reverse[j] == 0) {
    return;
  }
  const PlaneMask pset = same_plane(i, j) & pmask2;
  if (pset == 0) {
    return;
  }
  const int plane = lowest(pset);
  const int plane2 = lowest(pmask1);
  const TypeMask tmp1 = plane_types(plane2) & ~a.set1;
  const TypeMask tmp2 = plane_types(plane) & a.set_r_reverse;
  const RuleFlags bc = a.need_reverse ? flags::kBothCorners : 0;
  if (a.need_trigger) {  // unreachable: only surround_ok needs a trigger (kept as in Magic)
    lists().insert(i, j, a.distance,
                   {make(1, a.set1, tmp2, a.why, 1, flags::kForward | flags::kTrigger, plane2, plane),
                    make(a.distance, tmp1, tmp2, a.why, a.distance, flags::kForward | flags::kBothCorners | bc,
                         plane2, plane)});
    lists().insert(j, i, a.distance,
                   {make(1, a.set1, tmp2, a.why, 1, flags::kReverse | flags::kTrigger, plane2, plane),
                    make(a.distance, tmp1, tmp2, a.why, a.distance, flags::kReverse | flags::kBothCorners, plane2,
                         plane)});
    return;
  }
  const RuleFlags derived =
      a.manhattan != 0 ? 0 : (is_derived_type(i, a.set2, plane) ? flags::kDerivedEdge : 0) | a.target2;
  lists().insert(i, j, a.distance,
                 {make(a.distance, tmp1, tmp2, a.why, a.distance, flags::kForward | a.manhattan | derived | bc,
                       plane2, plane)});
  if (a.need_reverse) {
    lists().insert(j, i, a.distance,
                   {make(a.distance, tmp1, tmp2, a.why, a.distance,
                         flags::kReverse | flags::kBothCorners | a.manhattan | derived, plane2, plane)});
  }
}

void StyleCompiler::spacing_coincidence(const SpacingArgs& a, TileType i, TileType j, PlaneMask pmask1,
                                        PlaneMask pmask2) {
  if (a.in_r[i] == 0 || a.in1[j] == 0) {
    return;
  }
  const PlaneMask pset = same_plane(i, j) & pmask1;
  if (pset == 0) {
    return;
  }
  const int plane = lowest(pset);
  const int plane2 = lowest(pmask2);
  // Only cross-plane checks: skip when the non-space member is on the check plane.
  if (on_plane(i == tech::kSpace ? j : i, plane2)) {
    return;
  }
  const TypeMask tmp1 = plane_types(plane2) & ~a.set2;
  lists().append(i, j, make(1, tmp1, TypeMask{}, a.why, a.distance, flags::kForward | a.manhattan, plane2, plane));
}

namespace {

enum class Adjacency : std::uint8_t { bad, touching_ok, touching_illegal, manhattan, surround_ok };

Adjacency adjacency_of(std::string_view word) {
  if (word == "surround_ok") {
    return Adjacency::surround_ok;
  }
  if (word == "touching_ok") {
    return Adjacency::touching_ok;
  }
  if (word == "touching_illegal") {
    return Adjacency::touching_illegal;
  }
  return word == "manhattan_dist" ? Adjacency::manhattan : Adjacency::bad;
}

}  // namespace

bool StyleCompiler::decode_adjacency(SpacingArgs& a) {
  a.need_reverse = a.widerule;
  bool touching_ok = true;
  switch (adjacency_of(a.adjacency)) {
    case Adjacency::surround_ok:
      if (a.multiplane) {
        tech_error("\"surround_ok\" requires surrounding types to be in the same plane.");
        return false;
      }
      if ((a.pmask1 & a.pmask2) != 0) {
        tech_error("\"surround_ok\" used when spacing rule types are in the same plane.  Did you mean "
                   "\"touching_ok\"?");
        break;  // treated as touching_ok
      }
      if (a.widerule) {
        tech_error("Widespacing checks cannot use \"surround_ok\".");
        return false;
      }
      a.need_trigger = a.need_reverse = a.surround_ok = true;
      touching_ok = false;
      break;
    case Adjacency::touching_ok:
      if (a.multiplane || (a.pmask1 & a.pmask2) == 0) {
        tech_error("Spacing check with \"touching_ok\" must have all types in one plane.  Possibly you want "
                   "\"surround_ok\"?");
        return false;
      }
      break;
    case Adjacency::manhattan: a.manhattan = flags::kManhattan; [[fallthrough]];
    case Adjacency::touching_illegal: touching_ok = false; break;
    case Adjacency::bad:
      tech_error("Badly formed drc spacing line:  need \"touching_ok\", \"touching_illegal\", or \"surround_ok\".");
      return false;
  }
  if (touching_ok) {
    // Restrict to the coincident planes; check against neither set.
    a.pmask1 &= a.pmask2;
    a.pmask2 = a.pmask1;
    a.set_r = a.set_r_reverse = ~a.set1 & ~a.set2;
    a.need_reverse = a.need_reverse || !(a.set1 == a.set2);
  } else {
    a.set_r = ~a.set1;
    a.set_r_reverse = ~a.set2;
  }
  return true;
}

void StyleCompiler::prepare_pair_tests(SpacingArgs& a) const {
  const auto flags_of = [&](const TypeMask& m) {
    std::vector<char> out(type_count(), 0);
    for (TileType t = 0; t < type_count(); ++t) {
      out[t] = m.test(t) ? 1 : 0;
    }
    return out;
  };
  a.in1 = flags_of(a.set1);
  a.in2 = flags_of(a.set2);
  a.in_r = flags_of(a.set_r);
  a.in_r_reverse = flags_of(a.set_r_reverse);
  a.target1 = derived_flags(tech::kSpace, a.set1, 0, a.set2, lowest(a.pmask2));
  a.target2 = derived_flags(tech::kSpace, a.set2, 0, a.set1, lowest(a.pmask1));
}

int StyleCompiler::mask_spacing(const SpacingArgs& in) {
  SpacingArgs a = in;
  if (!decode_adjacency(a)) {
    return 0;
  }
  prepare_pair_tests(a);
  const bool one_set = a.set1 == a.set2;
  for (TileType i = 0; i < type_count(); ++i) {
    for (TileType j = 0; j < type_count(); ++j) {
      if (i == j) {
        continue;
      }
      spacing_set1_side(a, i, j, a.pmask1, a.pmask2);
      if (one_set || a.widerule) {
        continue;  // the width of set1 is unknown at an edge of set2
      }
      spacing_set2_side(a, i, j, a.pmask1, a.pmask2);
      if (!a.need_trigger) {
        spacing_coincidence(a, i, j, a.pmask1, a.pmask2);
      }
    }
  }
  return std::max(a.wwidth, a.distance);
}

bool StyleCompiler::multiplane_ok(const TypeMask& set, PlaneMask pmask, std::string_view which) {
  for (int p = 0; p < plane_count(); ++p) {
    for (int q = 0; q < plane_count(); ++q) {
      const bool both = p != q && (pmask & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0 &&
                        (pmask & tech::plane_bit(static_cast<tech::PlaneId>(q))) != 0;
      if (both && !(plane_types(p) & plane_types(q) & set).empty()) {
        tech_error("Types in " + std::string{which} +
                   " list must either be in one plane or else types must not share planes.");
        return false;
      }
    }
  }
  return true;
}

int StyleCompiler::spacing3(Tokens t) {
  const std::int32_t distance = text::c_atoi(t[3]);
  const WhyIndex reason = why(t[6]);
  const Names n1 = names(t[1]);
  PlaneMask pmask = coincident(n1.types, n1.planes);
  const Names n2 = names(t[2]);
  pmask &= coincident(n2.types, n2.planes);
  const Names n3 = names(t[5]);
  pmask &= coincident(n3.types, n3.planes);
  if (pmask == 0) {
    tech_error("Spacing check with \"corner_ok\" must have all types in one plane.");
    return 0;
  }
  const TypeMask not2 = ~n2.types;
  const TypeMask corner = ~(n3.types | n1.types);
  const RuleFlags bc = flags::kBothCorners;
  for_pairs(pmask, n1.types, corner, [&](TileType i, TileType j, PlaneMask pset) {
    if (n1.types.test(i) && corner.test(j)) {
      const int plane = lowest(pset);
      lists().insert(i, j, distance, {make(distance, not2, corner, reason, distance, flags::kForward | bc, plane, plane)});
      lists().insert(j, i, distance, {make(distance, not2, corner, reason, distance, flags::kReverse | bc, plane, plane)});
    }
  });
  return distance;
}

namespace {

std::string plane_pair_text(const tech::Technology& tech, int p, int q) {
  return "(" + std::string{tech.plane_name(static_cast<tech::PlaneId>(p))} + ", " +
         std::string{tech.plane_name(static_cast<tech::PlaneId>(q))} + ")";
}

}  // namespace

bool StyleCompiler::spacing_args(Tokens t, SpacingArgs& a) {
  a.widerule = text::starts_with(t[0], "wide");
  std::string_view layers2;
  if (a.widerule) {
    a.wwidth = text::c_atoi(t[2]);
    const bool run = t.size() == 8;
    a.runlength = run ? text::c_atoi(t[3]) : -1;
    const std::size_t k = run ? 4 : 3;
    layers2 = t[k];
    a.distance = text::c_atoi(t[k + 1]);
    a.adjacency = t[k + 2];
    a.why = why(t[k + 3]);
  } else {
    layers2 = t[2];
    a.distance = text::c_atoi(t[3]);
    a.adjacency = t[4];
    a.wwidth = a.distance;
    a.why = why(t[5]);
    if (t.size() >= 7) {
      tech_error("Unknown argument in spacing line.");
      return false;
    }
  }
  // Either list may hold independent types on different planes.
  const Names n1 = names(t[1]);
  a.set1 = n1.types;
  a.pmask1 = coincident(n1.types, n1.planes);
  if (a.pmask1 == 0 && n1.planes != 0) {
    a.pmask1 = n1.planes;
    a.multiplane = true;
    if (!multiplane_ok(a.set1, a.pmask1, "first")) {
      return false;
    }
  }
  const Names n2 = names(layers2);
  a.set2 = n2.types;
  a.pmask2 = coincident(n2.types, n2.planes);
  if (a.pmask2 == 0 && n2.planes != 0) {
    a.pmask2 = n2.planes;
    a.multiplane = true;
    return multiplane_ok(a.set2, a.pmask2, "second");
  }
  return true;
}

int StyleCompiler::multiplane_spacing(SpacingArgs a) {
  // Q-DRC-3: Magic returns after the first (plane1, plane2) combination.
  std::vector<std::pair<int, int>> combos;
  for (int p = 0; p < plane_count(); ++p) {
    for (int q = 0; q < plane_count(); ++q) {
      if ((a.pmask1 & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0 &&
          (a.pmask2 & tech::plane_bit(static_cast<tech::PlaneId>(q))) != 0) {
        combos.emplace_back(p, q);
      }
    }
  }
  if (combos.size() > 1) {
    std::string dropped;
    for (std::size_t k = 1; k < combos.size(); ++k) {
      dropped += (k == 1 ? "" : ", ") + plane_pair_text(tech(), combos[k].first, combos[k].second);
    }
    lint("DRC-LINT-001", std::string{"Multiplane spacing rule: Magic compiles only the plane pair "} +
                             plane_pair_text(tech(), combos[0].first, combos[0].second) + " and drops " + dropped +
                             (compat_.magic_compat ? " (magic-compat on: dropped here too)." : " (compiled here)."));
  }
  int result = 0;
  const TypeMask set1 = a.set1;
  const TypeMask set2 = a.set2;
  for (const auto& [p, q] : combos) {
    a.set1 = set1 & plane_types(p);
    a.set2 = set2 & plane_types(q);
    a.pmask1 = tech::plane_bit(static_cast<tech::PlaneId>(p));
    a.pmask2 = tech::plane_bit(static_cast<tech::PlaneId>(q));
    result = std::max(result, mask_spacing(a));
    if (compat_.magic_compat || result == 0) {
      break;
    }
  }
  return result;
}

int StyleCompiler::kw_spacing(Tokens t) {
  if (t.size() == 7 && t[4] == "corner_ok") {
    return spacing3(t);
  }
  SpacingArgs a;
  if (!spacing_args(t, a)) {
    return 0;
  }
  return a.multiplane ? multiplane_spacing(a) : mask_spacing(a);
}

}  // namespace hefesto::drc::rules_detail
