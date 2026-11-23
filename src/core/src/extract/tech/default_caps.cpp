// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `default*` capacitance shorthands (capability extract-tech-section
// "Default capacitance shorthands"; Q-EXTRACT-1; openspec/changes/
// m7-extraction/design.md D7, tasks.md 4.4) and DBTechNameMask's plane mask.
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c
// ExtTechSimpleAreaCap, ExtTechSimplePerimCap, ExtTechSimpleSidewallCap,
// ExtTechSimpleOverlapCap, ExtTechSimpleSideOverlapCap (:1194-1872) and
// database/DBtechname.c DBTechNameMask0, Copyright (C) 1985, 1990 Regents
// of the University of California (see THIRD_PARTY_NOTICES.md).
//
// An unknown plane name makes Magic index its tables with -1 or -2; here
// the line stops after the message (both modes).
//
// Owner: EXTTECH.

#include <optional>
#include <string>

#include "cap_tables.hpp"
#include "rule_context.hpp"

namespace hefesto::extract::detail {

namespace {

// pnum of a plane (exts_planeOrder as read so far).
std::int32_t order_of(const RuleContext& ctx, tech::PlaneId plane) {
  return plane < ctx.plane_order.size() ? ctx.plane_order[plane] : -1;
}

bool require_plane_order(const RuleContext& ctx, std::string_view what) {
  if (ctx.plane_order_seen) {
    return true;
  }
  ctx.report(kCodeNoPlaneOrder, "Cannot parse " + std::string{what} + " line without plane ordering!",
             Print::tech_error);
  return false;
}

// The shield types and planes strictly between two plane orders.
struct Shields {
  tech::TypeMask types;
  tech::PlaneMask planes = 0;
};

void add_shield_plane(const RuleContext& ctx, Shields& shields, tech::PlaneId plane) {
  shields.types |= ctx.tech.plane_types(plane);
  shields.planes |= tech::plane_bit(plane);
}

// The area / perimeter form: planes between pnum2 and pnum1 shield; every
// plane at or below pnum2 restricts the substrate types (Magic ANDs them
// all in turn).
Shields shields_below(const RuleContext& ctx, std::int32_t pnum1, std::int32_t pnum2, tech::TypeMask& subtypes) {
  Shields shields;
  for (auto p = static_cast<std::size_t>(tech::kFirstUserPlane); p < ctx.tech.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    const std::int32_t pnum3 = order_of(ctx, plane);
    if (pnum3 > pnum2 && pnum3 < pnum1) {
      add_shield_plane(ctx, shields, plane);
    } else if (pnum3 <= pnum2) {
      subtypes &= ctx.tech.plane_types(plane);
    }
  }
  shields.types.clear_type(tech::kSpace);
  subtypes.clear_type(tech::kSpace);
  return shields;
}

// The substrate line's defaults of a 3-argument area / perimeter line.
void substrate_defaults(const RuleContext& ctx, tech::TypeMask& subtypes, Shields& shields, bool keep_space) {
  const SubstrateDecl& sub = ctx.style.substrate;
  subtypes |= sub.types;
  subtypes &= ~sub.shield_types;
  subtypes.clear_type(tech::kSpace);
  if (!sub.default_type) {
    return;
  }
  shields.planes |= tech::plane_bit(*sub.plane);
  subtypes.clear_type(*sub.default_type);
  if (keep_space) {
    subtypes.set(tech::kSpace);
  }
  for (tech::TileType t = tech::kFirstUserType; t < ctx.type_end(); ++t) {
    if (ctx.plane_of(t) == *sub.plane && !sub.types.test(t)) {
      shields.types.set(t);
    }
  }
}

// The second plane of an area / perimeter line: the substrate plane for
// the 3-argument form; nullopt for none (also for an unknown name).
std::optional<tech::PlaneId> second_plane(const RuleContext& ctx, const Tokens& t) {
  if (t.size() == 4) {
    return ctx.style.substrate.plane;
  }
  return ctx.noisy_plane(t[t.size() - 2]);
}

struct AreaLine {
  tech::TypeMask types;
  tech::TypeMask subtypes;
  tech::PlaneId plane1 = 0;
  std::optional<tech::PlaneId> plane2;
  double cap = 0.0;
};

void record_default_overlaps(RuleContext& ctx, const AreaLine& line, const Shields& shields) {
  CapTables& tables = ctx.style.caps.tables;
  for (tech::TileType s = tech::kFirstUserType; s < ctx.type_end(); ++s) {
    if (!line.types.test(s) || ctx.tech.is_contact(s)) {
      continue;
    }
    for (tech::TileType t = tech::kFirstUserType; t < ctx.type_end(); ++t) {
      if (!line.subtypes.test(t) || s == t || overlap_value(tables, s, t) > 0.0) {
        continue;
      }
      set_overlap(tables, OverlapEntry{s, t, line.cap, line.plane1, line.plane2});
      tables.overlap_shield_planes[{s, t}] = shields.planes;
      tables.overlap_shield_types[{s, t}] = shields.types;
    }
  }
}

}  // namespace

void default_areacap_rule(RuleContext& ctx, const Tokens& t) {
  if (!require_plane_order(ctx, "area cap")) {
    return;
  }
  AreaLine line;
  line.types = ctx.noisy_types(t[1]);
  const std::optional<tech::PlaneId> plane1 = ctx.noisy_plane(t[2]);
  if (!plane1) {
    return;
  }
  line.plane1 = *plane1;
  line.types &= ctx.tech.plane_types(line.plane1);
  line.cap = ctx.cap_value(t.back());
  line.plane2 = second_plane(ctx, t);
  if (t.size() > 5) {
    line.subtypes = ctx.noisy_types(t[t.size() - 3]);
  }
  CapTables& tables = ctx.style.caps.tables;
  for (tech::TileType ty = tech::kFirstUserType; ty < ctx.type_end(); ++ty) {
    if (line.types.test(ty)) {
      tables.area[ty] = line.cap;
      set_fringe(tables, ty, tech::kSpace, line.cap);
    }
  }
  if ((!line.plane2 && !ctx.style.substrate.plane) || line.plane2 == line.plane1) {
    return;
  }
  Shields shields;
  if (line.plane2) {
    shields = shields_below(ctx, order_of(ctx, line.plane1), order_of(ctx, *line.plane2), line.subtypes);
  }
  if (ctx.style.substrate.plane && t.size() == 4) {
    substrate_defaults(ctx, line.subtypes, shields, false);
  }
  record_default_overlaps(ctx, line, shields);
}

namespace {

void record_default_perimeter(RuleContext& ctx, const AreaLine& line, const Shields& shields) {
  CapTables& tables = ctx.style.caps.tables;
  const tech::TypeMask nottypes = tech::TypeMask::of(tech::kSpace);
  const tech::PlaneMask pmask = line.plane2 ? tech::plane_bit(*line.plane2) : 0;
  for (tech::TileType s = tech::kFirstUserType; s < ctx.type_end(); ++s) {
    if (ctx.tech.is_contact(s) || !line.types.test(s)) {
      continue;
    }
    add_side_type(tables, line.plane1, s, nottypes);
    // nottypes is space alone, which is never a contact.
    tables.side_overlap_other_types[{s, tech::kSpace}] |= line.subtypes;
    tables.side_overlap_other_planes[{s, tech::kSpace}] |= pmask;
    prepend_rule(tables.side_overlap, s, tech::kSpace, EdgeRule{line.cap, 0.0F, line.subtypes, shields.types, pmask});
    for (tech::TileType r = tech::kFirstUserType; r < ctx.type_end(); ++r) {
      if (line.subtypes.test(r)) {
        tables.side_overlap_shield_planes[{s, r}] |= shields.planes;
      }
    }
  }
}

}  // namespace

void default_perimeter_rule(RuleContext& ctx, const Tokens& t) {
  if (!require_plane_order(ctx, "perimeter cap")) {
    return;
  }
  AreaLine line;
  line.types = ctx.noisy_types(t[1]);
  const std::optional<tech::PlaneId> plane1 = ctx.noisy_plane(t[2]);
  if (!plane1) {
    return;
  }
  line.plane1 = *plane1;
  line.types &= ctx.tech.plane_types(line.plane1);
  line.cap = ctx.cap_value(t.back());
  line.plane2 = second_plane(ctx, t);
  if (t.size() > 5) {
    line.subtypes = ctx.noisy_types(t[t.size() - 3]);
  } else if (ctx.style.substrate.plane) {
    line.subtypes = ctx.style.substrate.types;
  } else {
    line.subtypes = tech::TypeMask::of(tech::kSpace);
  }
  CapTables& tables = ctx.style.caps.tables;
  for (tech::TileType s = tech::kFirstUserType; s < ctx.type_end(); ++s) {
    if (line.types.test(s)) {
      tables.perimeter[{s, tech::kSpace}] = line.cap;
      tables.perimeter_mask[s].set(tech::kSpace);
    }
  }
  if ((!line.plane2 && !ctx.style.substrate.plane) || line.plane2 == line.plane1) {
    return;
  }
  Shields shields;
  if (line.plane2) {
    shields = shields_below(ctx, order_of(ctx, line.plane1), order_of(ctx, *line.plane2), line.subtypes);
  }
  if (ctx.style.substrate.plane && t.size() == 4) {
    substrate_defaults(ctx, line.subtypes, shields, true);
  }
  record_default_perimeter(ctx, line, shields);
}

// `sidewall TYPES space space TYPES C [OFFSET]` restricted to the plane;
// Magic does not require a plane order for it.
void default_sidewall_rule(RuleContext& ctx, const Tokens& t) {
  tech::TypeMask types1 = ctx.noisy_types(t[1]);
  const std::optional<tech::PlaneId> plane = ctx.noisy_plane(t[2]);
  if (!plane) {
    return;
  }
  EdgeRule rule;
  rule.cap = 0.5 * ctx.cap_value(t[3]);
  rule.offset = t.size() == 5 ? static_cast<float>(scan_double(t[4]).value_or(0.0)) : 0.0F;
  const tech::TypeMask on_plane = ctx.tech.plane_types(*plane);
  const tech::TypeMask types2 = tech::TypeMask::of(tech::kSpace) & on_plane;
  types1 &= on_plane;
  if (types1.test(tech::kSpace)) {
    ctx.report(kCodeBadCapRule, "Can't have space on inside of edge [ignored]", Print::tech_error);
  }
  rule.near_types = types2;
  rule.far_types = types1;
  CapTables& tables = ctx.style.caps.tables;
  for (tech::TileType s = tech::kFirstUserType; s < ctx.type_end(); ++s) {
    if (!types1.test(s)) {
      continue;
    }
    add_side_type(tables, ctx.plane_of(s), s, types2);
    if (types2.test(tech::kSpace)) {
      tables.side_couple_other_edges[{s, tech::kSpace}] |= types1;
      prepend_rule(tables.side_couple, s, tech::kSpace, rule);
    }
  }
}

namespace {

// The planes strictly between pnum2 and pnum1 (pnum2 < pnum3 < pnum1).
Shields shields_between(const RuleContext& ctx, std::int32_t low, std::int32_t high) {
  Shields shields;
  for (auto p = static_cast<std::size_t>(tech::kFirstUserPlane); p < ctx.tech.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    const std::int32_t pnum3 = order_of(ctx, plane);
    if (pnum3 > low && pnum3 < high) {
      add_shield_plane(ctx, shields, plane);
    }
  }
  shields.types.clear_type(tech::kSpace);
  return shields;
}

struct TwoPlaneLine {
  tech::TypeMask types1;
  tech::TypeMask types2;
  tech::PlaneId plane1 = 0;
  tech::PlaneId plane2 = 0;
  double cap = 0.0;
};

// types plane othertypes otherplane cap, each list restricted to its plane.
std::optional<TwoPlaneLine> two_plane_line(const RuleContext& ctx, const Tokens& t) {
  TwoPlaneLine line;
  line.types1 = ctx.noisy_types(t[1]);
  const std::optional<tech::PlaneId> plane1 = ctx.noisy_plane(t[2]);
  line.types2 = ctx.noisy_types(t[3]);
  const std::optional<tech::PlaneId> plane2 = ctx.noisy_plane(t[4]);
  if (!plane1 || !plane2) {
    return std::nullopt;
  }
  line.plane1 = *plane1;
  line.plane2 = *plane2;
  line.types1 &= ctx.tech.plane_types(line.plane1);
  line.types2 &= ctx.tech.plane_types(line.plane2);
  line.cap = ctx.cap_value(t[5]);
  return line;
}

}  // namespace

void default_overlap_rule(RuleContext& ctx, const Tokens& t) {
  if (!require_plane_order(ctx, "overlap cap")) {
    return;
  }
  const std::optional<TwoPlaneLine> line = two_plane_line(ctx, t);
  if (!line) {
    return;
  }
  const Shields shields = shields_between(ctx, order_of(ctx, line->plane2), order_of(ctx, line->plane1));
  CapTables& tables = ctx.style.caps.tables;
  for (tech::TileType s = tech::kFirstUserType; s < ctx.type_end(); ++s) {
    if (!line->types1.test(s) || ctx.tech.is_contact(s)) {
      continue;
    }
    for (tech::TileType o = tech::kFirstUserType; o < ctx.type_end(); ++o) {
      const bool skip = !line->types2.test(o) || ctx.tech.is_contact(o) || s == o || line->plane1 == line->plane2 ||
                        overlap_value(tables, s, o) > 0.0;
      if (skip) {
        continue;
      }
      set_overlap(tables, OverlapEntry{s, o, line->cap, line->plane1, line->plane2});
      tables.overlap_shield_planes[{s, o}] = shields.planes;
      tables.overlap_shield_types[{s, o}] = shields.types;
    }
  }
}

namespace {

void record_default_sideoverlap(RuleContext& ctx, const TwoPlaneLine& line, const Shields& shields) {
  CapTables& tables = ctx.style.caps.tables;
  const tech::TypeMask nottypes = tech::TypeMask::of(tech::kSpace);
  const tech::PlaneMask pmask = tech::plane_bit(line.plane2);
  for (tech::TileType s = tech::kFirstUserType; s < ctx.type_end(); ++s) {
    if (ctx.tech.is_contact(s) || !line.types1.test(s)) {
      continue;
    }
    add_side_type(tables, line.plane1, s, nottypes);
    tables.side_overlap_other_types[{s, tech::kSpace}] |= line.types2;
    tables.side_overlap_other_planes[{s, tech::kSpace}] |= pmask;
    prepend_rule(tables.side_overlap, s, tech::kSpace, EdgeRule{line.cap, 0.0F, line.types2, shields.types, pmask});
    for (tech::TileType r = tech::kFirstUserType; r < ctx.type_end(); ++r) {
      if (line.types2.test(r)) {
        tables.side_overlap_shield_planes[{s, r}] |= shields.planes;
      }
    }
  }
}

}  // namespace

// Q-EXTRACT-1: the shield direction comes from the plane indices with
// magic-compat ON (Magic), from the plane order with it OFF.
void default_sideoverlap_rule(RuleContext& ctx, const Tokens& t) {
  if (!require_plane_order(ctx, "side overlap cap")) {
    return;
  }
  const std::optional<TwoPlaneLine> line = two_plane_line(ctx, t);
  if (!line) {
    return;
  }
  const std::int32_t pnum1 = order_of(ctx, line->plane1);
  const std::int32_t pnum2 = order_of(ctx, line->plane2);
  if (pnum1 == pnum2) {
    ctx.report(kCodeBadCapRule, "Cannot have fringing capacitance between types on the same plane",
               Print::tech_error);
    return;
  }
  const bool forward = ctx.magic_compat() ? line->plane1 < line->plane2 : pnum1 < pnum2;
  const Shields shields = forward ? shields_between(ctx, pnum1, pnum2) : shields_between(ctx, pnum2, pnum1);
  if (line->types1.test(tech::kSpace) || line->types2.test(tech::kSpace)) {
    ctx.report(kCodeBadCapRule, "Overlap types can't contain space [ignored]", Print::tech_error);
    return;
  }
  record_default_sideoverlap(ctx, *line, shields);
}

namespace {

// The comma-separated elements of a type list, parentheses kept whole.
std::vector<std::string_view> list_elements(std::string_view text) {
  std::vector<std::string_view> out;
  int depth = 0;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= text.size(); ++i) {
    const char c = i < text.size() ? text[i] : ',';
    depth += c == '(' ? 1 : (c == ')' ? -1 : 0);
    if (c == ',' && depth <= 0) {
      if (i > start) {
        out.push_back(text.substr(start, i - start));
      }
      start = i + 1;
    }
  }
  return out;
}

tech::PlaneMask user_type_planes(const tech::Technology& tech, const tech::TypeMask& types) {
  tech::PlaneMask planes = 0;
  for (auto t = static_cast<std::size_t>(tech::kFirstUserType); t < tech.user_type_end(); ++t) {
    if (types.test(static_cast<tech::TileType>(t))) {
      planes |= tech.type_planes(static_cast<tech::TileType>(t));
    }
  }
  return planes;
}

}  // namespace

tech::PlaneMask name_mask_planes(const tech::Technology& tech, std::string_view text, const tech::TypeMask& types) {
  tech::PlaneMask planes = 0;
  for (const std::string_view element : list_elements(text)) {
    const std::size_t close = element.rfind(')');
    const std::size_t slash = element.find('/', close == std::string_view::npos ? 0 : close);
    if (slash == std::string_view::npos) {
      auto parsed = tech.parse_type_list(element);
      planes |= parsed.ok() ? user_type_planes(tech, parsed.value()) : 0;
      continue;
    }
    const tech::Lookup<tech::PlaneId> plane = tech.find_plane(element.substr(slash + 1));
    if (plane.ok() && *plane.id > 0) {
      planes = tech::plane_bit(*plane.id);  // `=`, as DBTechNameMask0 does
    }
  }
  const bool space_only = types.empty() || types == tech::TypeMask::of(tech::kSpace);
  if (space_only && planes == 0) {
    planes = ~tech::plane_bit(0);  // DBTypePlaneMaskTbl[TT_SPACE]: every plane but the router
  }
  return planes;
}

}  // namespace hefesto::extract::detail
