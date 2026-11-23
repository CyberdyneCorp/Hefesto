// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The parse half of extTechFinalStyle (capability extract-tech-section
// "Connectivity overrides" (resistive and device connectivity), "Contact
// residues and coupling table completion"; openspec/changes/m7-extraction/
// design.md D7, tasks.md 4.3) and the derived lists of a parsed style
// (CapRules' lists, the resistance classes' contact values, ExtGetDevInfo's
// table).
//
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtTech.c extTechFinalStyle
// (:3627-3800) and ExtGetDevInfo (:312-466), Copyright (C) 1985, 1990
// Regents of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: EXTTECH.

#include <algorithm>
#include <string>

#include "rule_context.hpp"

namespace hefesto::extract::detail {

namespace {

tech::TileType user_end(const tech::Technology& tech) {
  return static_cast<tech::TileType>(tech.user_type_end());
}

tech::TileType type_end(const tech::Technology& tech) {
  return static_cast<tech::TileType>(tech.type_count());
}

bool in_range(tech::TileType t, tech::TileType first, tech::TileType end) {
  return t >= first && t < end;
}

tech::PlaneId plane_of(const tech::Technology& tech, tech::TileType t) {
  return tech.home_plane(t).value_or(tech::PlaneId{0});
}

// exts_subsDevTypes: devices whose substrate types meet the substrate's,
// and substrate types off the substrate plane that are not shields.
void substrate_device_types(ExtStyle& s, const tech::Technology& tech) {
  for (tech::TileType r = tech::kFirstUserType; r < type_end(tech); ++r) {
    for (const DevicePattern* d : s.records_for(r)) {
      if (d->substrate_types.intersects(s.substrate.types)) {
        s.substrate_device_types.set(r);
      }
    }
  }
  if (!s.substrate.plane) {
    return;
  }
  for (tech::TileType r = tech::kFirstUserType; r < type_end(tech); ++r) {
    if (s.substrate.types.test(r) && !s.substrate.shield_types.test(r) &&
        plane_of(tech, r) != *s.substrate.plane) {
      s.substrate_device_types.set(r);
    }
  }
}

// exts_resistConn, exts_deviceConn of non-device types, and
// exts_typesResistChanged.
void resist_connectivity(ExtStyle& s, const tech::Technology& tech) {
  const tech::TileType end = type_end(tech);
  for (tech::TileType r = tech::kFirstUserType; r < end; ++r) {
    tech::TypeMask mask = s.node_conn[r];
    if (!s.device_types.test(r)) {
      s.device_conn[r] = {};
    }
    for (tech::TileType o = tech::kFirstUserType; o < end; ++o) {
      if (mask.test(o) && s.resist_class[o] != s.resist_class[r]) {
        mask.clear_type(o);
      }
    }
    s.resist_conn[r] = mask;
  }
  tech::TypeMask all;
  for (tech::TileType t = tech::kMagnet; t < tech::kMaxTileTypes; ++t) {
    all.set(t);
  }
  all.set(tech::kSpace);
  s.resist_changed.assign(tech::kMaxTileTypes, tech::TypeMask{});
  for (tech::TileType r = 0; r < end; ++r) {
    s.resist_changed[r] = all;
  }
  for (tech::TileType r = tech::kFirstUserType; r < end; ++r) {
    const std::int32_t cls = s.resist_class[r];
    if (cls >= 0) {
      s.resist_changed[r] &= ~s.resist_classes[static_cast<std::size_t>(cls)].types;
    }
  }
}

// One residue r of contact c: c follows r in the side-edge, overlap-target,
// overlap-shield and sideoverlap-target sets.
void follow_residue(CapTables& tables, const tech::Technology& tech, tech::TileType c, tech::TileType r,
                    const tech::TypeMask& residues) {
  const tech::TileType end = user_end(tech);
  tables.side_edges[c] |= tables.side_edges[r];
  for (tech::TileType q = tech::kFirstUserType; q < end; ++q) {
    if (tables.overlap_other_types[q].test(r)) {
      tables.overlap_other_types[q].set(c);
    }
  }
  for (auto& [key, mask] : tables.overlap_shield_types) {
    const bool pair = in_range(key.first, tech::kFirstUserType, end) && in_range(key.second, tech::kFirstUserType, end);
    if (pair && mask.test(r) && !residues.test(key.first) && !residues.test(key.second)) {
      mask.set(c);
    }
  }
  for (auto& [key, mask] : tables.side_overlap_other_types) {
    const bool pair = in_range(key.first, tech::kFirstUserType, end) && key.second < end;
    if (pair && mask.test(r)) {
      mask.set(c);
    }
  }
}

// The residue check: contacts were skipped by the capacitance rules.
void contact_residues(CapTables& tables, const tech::Technology& tech) {
  for (tech::TileType c = tech::kFirstUserType; c < type_end(tech); ++c) {
    if (!tech.is_contact(c)) {
      continue;
    }
    const tech::PlaneMask planes = tech.type_planes(c);
    for (std::size_t p = 0; p < tech.plane_count(); ++p) {
      if ((planes & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0) {
        tables.overlap_types[p].set(c);
        tables.side_types[p].set(c);
      }
    }
    const tech::TypeMask residues = tech.full_residues(c);
    for (tech::TileType r = tech::kFirstUserType; r < user_end(tech); ++r) {
      if (residues.test(r)) {
        follow_residue(tables, tech, c, r, residues);
      }
    }
  }
}

// A shield type of (s, t) is an overlap target of s.
void shields_are_targets(CapTables& tables, const tech::Technology& tech) {
  const tech::TileType end = type_end(tech);
  for (const auto& [key, planes] : tables.overlap_shield_planes) {
    if (planes == 0 || !in_range(key.first, tech::kFirstUserType, end) ||
        !in_range(key.second, tech::kFirstUserType, end)) {
      continue;
    }
    const auto found = tables.overlap_shield_types.find(key);
    if (found == tables.overlap_shield_types.end()) {
      continue;
    }
    const tech::PlaneId p1 = plane_of(tech, key.first);
    for (tech::TileType r = tech::kFirstUserType; r < end; ++r) {
      if (!found->second.test(r)) {
        continue;
      }
      tables.overlap_planes |= tech::plane_bit(p1);
      tables.overlap_other_planes[key.first] |= tech::plane_bit(plane_of(tech, r));
      tables.overlap_types[p1].set(key.first);
      tables.overlap_other_types[key.first].set(r);
    }
  }
}

template <typename Map>
void mask_pairs(Map& map, const tech::TypeMask& active, tech::TileType end) {
  for (auto& [key, mask] : map) {
    if (in_range(key.first, tech::kFirstUserType, end) && in_range(key.second, tech::kFirstUserType, end)) {
      mask &= active;
    }
  }
}

// Every coupling mask loses the types that do not take part in extraction.
void mask_with_active(CapTables& tables, const tech::Technology& tech, const tech::TypeMask& active) {
  const tech::TileType end = type_end(tech);
  for (tech::TileType s = tech::kFirstUserType; s < end; ++s) {
    tables.overlap_other_types[s] &= active;
    tables.perimeter_mask[s] &= active;
  }
  mask_pairs(tables.overlap_shield_types, active, end);
  mask_pairs(tables.side_overlap_other_types, active, end);
  mask_pairs(tables.side_couple_other_edges, active, end);
  for (std::size_t p = 0; p < tech.plane_count(); ++p) {
    tables.overlap_types[p] &= active;
    tables.side_types[p] &= active;
  }
}

// ResistClass::contact_resistance: the `contact` value of the class's
// lowest type that has one.
void class_contact_values(ExtStyle& s) {
  for (ResistClass& rc : s.resist_classes) {
    for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
      if (rc.types.test(static_cast<tech::TileType>(t)) && s.via_resistance[t] != 0) {
        rc.contact_resistance = s.via_resistance[t];
        break;
      }
    }
  }
}

// The first class whose types meet `types`, or nullopt.
std::optional<std::int32_t> class_meeting(const ExtStyle& s, const tech::TypeMask& types) {
  for (std::size_t n = 0; n < s.resist_classes.size(); ++n) {
    if (s.resist_classes[n].types.intersects(types)) {
      return static_cast<std::int32_t>(n);
    }
  }
  return std::nullopt;
}

DeviceInfo info_of(const ExtStyle& s, const DevicePattern& d, tech::TileType type) {
  DeviceInfo info;
  info.model = d.model;
  info.type = type;
  info.substrate_node = d.substrate_node;
  const tech::TypeMask source = d.terminals.empty() ? tech::TypeMask{} : d.terminals[0];
  const std::optional<std::int32_t> src = class_meeting(s, source);
  info.source_class = src.value_or(-1);
  if (d.terminals.size() < 2) {
    // ExtGetDevInfo copies its loop index: the class found, or the class
    // count when none was (magic-compat ON).
    const auto count = static_cast<std::int32_t>(s.resist_classes.size());
    info.drain_class = s.magic_compat ? src.value_or(count) : info.source_class;
  } else {
    info.drain_class = class_meeting(s, d.terminals[1]).value_or(-1);
  }
  info.substrate_class = class_meeting(s, d.substrate_types).value_or(-1);
  return info;
}

}  // namespace

std::vector<DeviceInfo> build_device_info(const ExtStyle& style, const tech::Technology& tech) {
  std::vector<DeviceInfo> table;
  std::vector<std::string> seen;
  for (tech::TileType t = tech::kFirstUserType; t < type_end(tech); ++t) {
    for (const DevicePattern* d : style.records_for(t)) {
      if (std::find(seen.begin(), seen.end(), d->model) != seen.end()) {
        continue;
      }
      seen.push_back(d->model);
      table.push_back(info_of(style, *d, t));
    }
  }
  return table;
}

void derive_rule_lists(ExtStyle& style, const tech::Technology& /*tech*/) {
  CapRules& caps = style.caps;
  const CapTables& tables = caps.tables;
  caps.area.clear();
  caps.perimeter.clear();
  caps.overlap.clear();
  caps.sidewall.clear();
  caps.sideoverlap.clear();
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    if (tables.area[t] != 0.0) {
      caps.area.push_back(TypeCap{static_cast<tech::TileType>(t), {}, tables.area[t]});
    }
  }
  // Every stored value: extraction reads exts_perimCap itself; the mask
  // (which loses space with the inactive types) is only for printing.
  for (const auto& [key, cap] : tables.perimeter) {
    caps.perimeter.push_back(TypeCap{key.first, tech::TypeMask::of(key.second), cap});
  }
  for (const auto& [key, cap] : tables.overlap) {
    const auto shield = tables.overlap_shield_types.find(key);
    caps.overlap.push_back(OverlapRule{tech::TypeMask::of(key.first), tech::TypeMask::of(key.second), cap,
                                       shield == tables.overlap_shield_types.end() ? tech::TypeMask{}
                                                                                   : shield->second});
  }
  for (const auto& [key, rules] : tables.side_couple) {
    for (const EdgeRule& r : rules) {
      caps.sidewall.push_back(SidewallRule{tech::TypeMask::of(key.first), tech::TypeMask::of(key.second),
                                           r.near_types, r.far_types, r.cap, r.offset});
    }
  }
  for (const auto& [key, rules] : tables.side_overlap) {
    for (const EdgeRule& r : rules) {
      caps.sideoverlap.push_back(SideOverlapRule{tech::TypeMask::of(key.first), tech::TypeMask::of(key.second),
                                                 r.near_types, r.cap, r.far_types});
    }
  }
}

void finalize_style(ExtStyle& style, const tech::Technology& tech) {
  substrate_device_types(style, tech);
  resist_connectivity(style, tech);
  contact_residues(style.caps.tables, tech);
  shields_are_targets(style.caps.tables, tech);
  mask_with_active(style.caps.tables, tech, style.active);
  class_contact_values(style);
  derive_rule_lists(style, tech);
  style.set_device_info(build_device_info(style, tech));
}

}  // namespace hefesto::extract::detail
