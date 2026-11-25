// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The ghost classes of the layer stack (hefesto/view3d/stack/ghost.hpp;
// Q-MISC-85). Owner: SHELL (round 4, the classifier only).

#include "hefesto/view3d/stack/ghost.hpp"

#include <cstddef>

namespace hefesto::view3d {

tech::PlaneMask conducting_planes(const tech::Technology& tech) {
  tech::PlaneMask planes = 0;
  const std::size_t types = tech.type_count();
  for (const tech::ContactInfo& contact : tech.contacts()) {
    const tech::TypeMask residues = tech.full_residues(contact.type);
    for (std::size_t t = tech::kFirstUserType; t < types; ++t) {
      const auto type = static_cast<tech::TileType>(t);
      if (!residues.test(type)) continue;
      if (const auto plane = tech.home_plane(type)) planes |= tech::plane_bit(*plane);
    }
  }
  return planes;
}

bool ghosted_type(const tech::Technology& tech, tech::TileType type, tech::PlaneMask conducting) {
  // No contact at all: nothing tells conducting planes apart, ghost nothing.
  if (conducting == 0 || type < tech::kFirstUserType || tech.is_contact(type)) return false;
  const auto plane = tech.home_plane(type);
  return plane.has_value() && (conducting & tech::plane_bit(*plane)) == 0;
}

std::vector<bool> ghosted_by_default(const LayerStack& stack, const tech::Technology& tech) {
  const tech::PlaneMask conducting = conducting_planes(tech);
  std::vector<bool> ghosted(stack.size(), false);
  for (std::size_t i = 0; i < stack.size(); ++i) {
    const std::optional<tech::TileType>& type = stack.layers()[i].type;
    ghosted[i] = type.has_value() && ghosted_type(tech, *type, conducting);
  }
  return ghosted;
}

}  // namespace hefesto::view3d
