// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `areacap`, `perimc`, `overlap`, `sidewall`, `sideoverlap` (capability
// extract-tech-section "Area and perimeter capacitance rules", "Overlap
// capacitance rules", "Sidewall coupling rules and halving", "Sideoverlap
// rules"; openspec/changes/m7-extraction/design.md D7, tasks.md 4.4).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c ExtTechLine
// (AREAC :2267-2276, OVERC :3122-3210, SIDEOVERLAP :3211-3317, SIDEWALL
// :3318-3361, PERIMC :3377-3390), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: EXTTECH.

#include <optional>
#include <string>

#include "cap_tables.hpp"
#include "rule_context.hpp"

namespace hefesto::extract::detail {

void set_fringe(CapTables& tables, tech::TileType s, tech::TileType t, double cap) {
  // `(float) capVal * FRINGE_MULT`, stored as float.
  const auto mult = static_cast<float>(static_cast<double>(static_cast<float>(cap)) * kFringeMult);
  tables.fringe_mult[{s, t}] = mult;
  tables.fringe_mult[{t, s}] = mult;
}

void set_overlap(CapTables& tables, const OverlapEntry& entry) {
  const TypePair key{entry.s, entry.t};
  tables.overlap[key] = entry.cap;
  set_fringe(tables, entry.s, entry.t, entry.cap);
  tables.overlap_planes |= tech::plane_bit(entry.p1);
  if (entry.p2) {
    tables.overlap_other_planes[entry.s] |= tech::plane_bit(*entry.p2);
  }
  tables.overlap_types[entry.p1].set(entry.s);
  tables.overlap_other_types[entry.s].set(entry.t);
}

double overlap_value(const CapTables& tables, tech::TileType s, tech::TileType t) {
  const auto it = tables.overlap.find({s, t});
  return it == tables.overlap.end() ? 0.0 : it->second;
}

void prepend_rule(std::map<TypePair, std::vector<EdgeRule>>& lists, tech::TileType s, tech::TileType t,
                  const EdgeRule& rule) {
  std::vector<EdgeRule>& list = lists[{s, t}];
  list.insert(list.begin(), rule);
}

void add_side_type(CapTables& tables, tech::PlaneId plane, tech::TileType s, const tech::TypeMask& edges) {
  tables.side_planes |= tech::plane_bit(plane);
  tables.side_types[plane].set(s);
  tables.side_edges[s] |= edges;
}

void areacap_rule(RuleContext& ctx, const Tokens& t) {
  const double cap = ctx.cap_value(t[2]);
  CapTables& tables = ctx.style.caps.tables;
  for (tech::TileType ty = tech::kFirstUserType; ty < ctx.type_end(); ++ty) {
    if (ctx.types1.test(ty)) {
      tables.area[ty] = cap;
      set_fringe(tables, ty, tech::kSpace, cap);
    }
  }
}

void perimc_rule(RuleContext& ctx, const Tokens& t) {
  const tech::TypeMask types2 = ctx.noisy_types(t[2]);
  const double cap = ctx.cap_value(t[3]);
  if (cap == 0.0) {
    return;
  }
  CapTables& tables = ctx.style.caps.tables;
  for (tech::TileType s = tech::kFirstUserType; s < ctx.type_end(); ++s) {
    if (!ctx.types1.test(s)) {
      continue;
    }
    for (tech::TileType o = 0; o < ctx.type_end(); ++o) {
      if (types2.test(o)) {
        tables.perimeter[{s, o}] = cap;
        tables.perimeter_mask[s].set(o);
      }
    }
  }
}

namespace {

struct OverlapLine {
  tech::TypeMask types2;
  tech::TypeMask shield;
  double cap = 0.0;
  bool has_shield = false;
};

// The shield planes of one overlap pair; false when a shield type sits on
// the plane of s or t (reported).
bool overlap_shield_planes(const RuleContext& ctx, const OverlapLine& line, tech::PlaneId p1, tech::PlaneId p2,
                           tech::PlaneMask& planes) {
  bool ok = true;
  for (tech::TileType r = tech::kFirstUserType; r < ctx.type_end(); ++r) {
    if (!line.shield.test(r) || ctx.tech.is_contact(r)) {
      continue;
    }
    const tech::PlaneId p3 = ctx.plane_of(r);
    if (p3 == p1 || p3 == p2) {
      ctx.report(kCodeBadCapRule,
                 "Shielding type (" + std::string{ctx.tech.type_name(r)} +
                     ") must be on a different plane from shielded types.",
                 Print::tech_error);
      ok = false;
      continue;
    }
    planes |= tech::plane_bit(p3);
  }
  return ok;
}

// Why the pair (s, t) gets no overlap capacitance, or nullopt.
std::optional<std::string> overlap_pair_error(const RuleContext& ctx, tech::TileType s, tech::TileType t) {
  const std::string sn{ctx.tech.type_name(s)};
  const std::string tn{ctx.tech.type_name(t)};
  if (s == t) {
    return "Can't have overlap capacitance between tiles of the same type (" + sn + ")";
  }
  if (ctx.plane_of(s) == ctx.plane_of(t)) {
    return "Can't have overlap capacitance between tiles on the same plane (" + sn + ", " + tn + ")";
  }
  if (overlap_value(ctx.style.caps.tables, s, t) > 0.0) {
    return "Only one of \"overlap " + sn + " " + tn + "\" or \"overlap " + tn + " " + sn + "\" allowed";
  }
  return std::nullopt;
}

void overlap_pair(RuleContext& ctx, const OverlapLine& line, tech::TileType s, tech::TileType t) {
  if (const std::optional<std::string> error = overlap_pair_error(ctx, s, t)) {
    ctx.report(kCodeBadCapRule, *error, Print::tech_error);
    return;
  }
  CapTables& tables = ctx.style.caps.tables;
  const tech::PlaneId p1 = ctx.plane_of(s);
  const tech::PlaneId p2 = ctx.plane_of(t);
  set_overlap(tables, OverlapEntry{s, t, line.cap, p1, p2});
  if (!line.has_shield) {
    return;
  }
  tech::PlaneMask planes = 0;
  (void)overlap_shield_planes(ctx, line, p1, p2, planes);
  tables.overlap_shield_planes[{s, t}] = planes;
  tables.overlap_shield_types[{s, t}] = line.shield;
}

}  // namespace

// Contacts are skipped on both sides (their caps come from residues);
// a rejected pair leaves the pairs before it in place, as Magic does.
void overlap_rule(RuleContext& ctx, const Tokens& t) {
  OverlapLine line;
  line.types2 = ctx.noisy_types(t[2]);
  line.cap = ctx.cap_value(t[3]);
  line.has_shield = t.size() > 4;
  if (line.has_shield) {
    line.shield = ctx.noisy_types(t[4]);
  }
  for (tech::TileType s = tech::kFirstUserType; s < ctx.type_end(); ++s) {
    if (!ctx.types1.test(s) || ctx.tech.is_contact(s)) {
      continue;
    }
    for (tech::TileType o = tech::kFirstUserType; o < ctx.type_end(); ++o) {
      if (line.types2.test(o) && !ctx.tech.is_contact(o)) {
        overlap_pair(ctx, line, s, o);
      }
    }
  }
}

void sidewall_rule(RuleContext& ctx, const Tokens& t) {
  const tech::TypeMask types2 = ctx.noisy_types(t[2]);
  EdgeRule rule;
  rule.near_types = ctx.noisy_types(t[3]);
  rule.far_types = ctx.noisy_types(t[4]);
  if (ctx.types1.test(tech::kSpace)) {
    ctx.report(kCodeBadCapRule, "Can't have space on inside of edge [ignored]", Print::tech_error);
  }
  rule.cap = 0.5 * ctx.cap_value(t[5]);
  // `%lg` of the offset; Magic reads an uninitialised value when it does
  // not convert (fixed in both modes: 0).
  rule.offset = t.size() == 7 ? static_cast<float>(scan_double(t[6]).value_or(0.0)) : 0.0F;
  CapTables& tables = ctx.style.caps.tables;
  for (tech::TileType s = tech::kFirstUserType; s < ctx.type_end(); ++s) {
    if (!ctx.types1.test(s)) {
      continue;
    }
    add_side_type(tables, ctx.plane_of(s), s, types2);
    for (tech::TileType o = 0; o < ctx.type_end(); ++o) {
      if (types2.test(o)) {
        tables.side_couple_other_edges[{s, o}] |= rule.far_types;
        prepend_rule(tables.side_couple, s, o, rule);
      }
    }
  }
}

namespace {

struct SideOverlapLine {
  tech::TypeMask types2;
  tech::TypeMask ov;
  tech::TypeMask shield;
  tech::PlaneMask pov = 0;
  double cap = 0.0;
};

// The target planes of an overlap list that contains space: the `/plane`
// suffix. Magic dereferences past a missing `/` and crashes; fixed in both
// modes: the error, and the line is ignored (false).
bool space_target_plane(const RuleContext& ctx, const std::string& text, tech::PlaneMask& pov) {
  const std::size_t slash = text.find('/');
  if (slash == std::string::npos) {
    ctx.report(kCodeBadCapRule, "Must specify plane for sideoverlap to space", Print::tech_error);
    return false;
  }
  const tech::Lookup<tech::PlaneId> plane = ctx.tech.find_plane(text.substr(slash + 1));
  if (!plane.ok()) {
    ctx.report(kCodeBadCapRule, "Unknown overlap plane " + text, Print::tech_error);
    return true;
  }
  pov = tech::plane_bit(*plane.id);
  return true;
}

void report_diffplane(const RuleContext& ctx) {
  ctx.report(kCodeBadCapRule,
             "Overlapped types in \"sideoverlap\" rule must be on a\n\tdifferent plane from intypes and outtypes.",
             Print::tech_error);
}

tech::PlaneMask sideoverlap_shields(const RuleContext& ctx, const tech::TypeMask& shield, tech::PlaneId p1,
                                    tech::PlaneId p2) {
  tech::PlaneMask planes = 0;
  for (tech::TileType r = tech::kFirstUserType; r < ctx.type_end(); ++r) {
    if (!shield.test(r) || ctx.tech.is_contact(r)) {
      continue;
    }
    const tech::PlaneId p3 = ctx.plane_of(r);
    if (p3 == p1 || p3 == p2) {
      ctx.report(kCodeBadCapRule,
                 "Shielding type (" + std::string{ctx.tech.type_name(r)} +
                     ") must be on a different plane from shielded types.",
                 Print::tech_error);
      continue;
    }
    planes |= tech::plane_bit(p3);
  }
  return planes;
}

// One edge (s|o); false when o is on an overlapped plane (Magic's goto
// diffplane, which ends the whole line).
bool sideoverlap_edge(RuleContext& ctx, const SideOverlapLine& line, tech::TileType s, tech::TileType o) {
  const tech::PlaneId p1 = ctx.plane_of(s);
  const tech::PlaneId p2 = ctx.plane_of(o);
  if (o != tech::kSpace && (line.pov & tech::plane_bit(p2)) != 0) {
    return false;
  }
  CapTables& tables = ctx.style.caps.tables;
  tables.side_overlap_other_types[{s, o}] |= line.ov;
  tables.side_overlap_other_planes[{s, o}] |= line.pov;
  prepend_rule(tables.side_overlap, s, o, EdgeRule{line.cap, 0.0F, line.ov, line.shield, line.pov});
  const tech::PlaneMask shields = sideoverlap_shields(ctx, line.shield, p1, p2);
  for (tech::TileType v = tech::kFirstUserType; v < ctx.type_end(); ++v) {
    if (line.ov.test(v)) {
      tables.side_overlap_shield_planes[{s, v}] |= shields;
    }
  }
  return true;
}

// The inside type s; false on diffplane.
bool sideoverlap_inside(RuleContext& ctx, const SideOverlapLine& line, tech::TileType s) {
  if ((line.pov & tech::plane_bit(ctx.plane_of(s))) != 0) {
    return false;
  }
  add_side_type(ctx.style.caps.tables, ctx.plane_of(s), s, line.types2);
  for (tech::TileType o = 0; o < ctx.type_end(); ++o) {
    if (!line.types2.test(o) || ctx.tech.is_contact(o)) {
      continue;
    }
    if (!sideoverlap_edge(ctx, line, s, o)) {
      return false;
    }
  }
  return true;
}

}  // namespace

void sideoverlap_rule(RuleContext& ctx, const Tokens& t) {
  SideOverlapLine line;
  line.types2 = ctx.noisy_types(t[2]);
  line.ov = ctx.noisy_types(t[3]);
  line.pov = name_mask_planes(ctx.tech, t[3], line.ov);
  line.cap = ctx.cap_value(t[4]);
  if (t.size() == 6) {
    line.shield = ctx.noisy_types(t[5]);
  }
  if (ctx.types1.test(tech::kSpace)) {
    ctx.report(kCodeBadCapRule, "Can't have space on inside of edge [ignored]", Print::tech_error);
  }
  if (line.ov.test(tech::kSpace) && !space_target_plane(ctx, t[3], line.pov)) {
    return;
  }
  for (tech::TileType s = tech::kFirstUserType; s < ctx.type_end(); ++s) {
    if (!ctx.types1.test(s) || ctx.tech.is_contact(s)) {
      continue;
    }
    if (!sideoverlap_inside(ctx, line, s)) {
      report_diffplane(ctx);
      return;
    }
  }
}

}  // namespace hefesto::extract::detail
