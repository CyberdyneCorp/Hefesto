// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private helper: the technology masks Magic's database code reads from
// global tables, derived from the tech::Technology contract.
//   DBPlaneTypes[p]  -> TechMasks::plane_types(p)  (database/DBtcontact.c:741-800)
//   DBResidueMask(t) -> TechMasks::residues(t)     (empty for non-contacts)
//   DBAllButSpaceAndDRCBits -> all_but_space_and_drc() (database/DBtechtype.c:566)

#pragma once

#include <cstddef>
#include <vector>

#include "hefesto/tech/technology.hpp"

namespace hefesto::db {

[[nodiscard]] inline tech::TypeMask all_but_space_and_drc() {
  tech::TypeMask m;
  for (std::size_t t = tech::kMagnet; t < tech::kMaxTileTypes; ++t) {
    m.set(static_cast<tech::TileType>(t));
  }
  return m;
}

class TechMasks {
 public:
  TechMasks(const tech::Technology& tech, std::size_t plane_count) : tech_(tech), planes_(plane_count) {
    for (std::size_t p = 1; p < plane_count; ++p) {
      planes_[p].set(tech::kSpace);
    }
    for (std::size_t t = 1; t < tech.type_count(); ++t) {
      const auto home = tech.home_plane(static_cast<tech::TileType>(t));
      if (home && *home < plane_count) {
        planes_[*home].set(static_cast<tech::TileType>(t));
      }
    }
    for (const tech::ContactInfo& c : tech.contacts()) {
      for (std::size_t p = 0; p < plane_count; ++p) {
        if ((c.planes & tech::plane_bit(static_cast<tech::PlaneId>(p))) != 0) {
          planes_[p].set(c.type);
        }
      }
    }
  }

  [[nodiscard]] const tech::TypeMask& plane_types(tech::PlaneId p) const { return planes_.at(p); }

  [[nodiscard]] tech::TypeMask residues(tech::TileType t) const {
    const tech::ContactInfo* c = tech_.contact(t);
    return c != nullptr ? c->residues : tech::TypeMask{};
  }

 private:
  const tech::Technology& tech_;
  std::vector<tech::TypeMask> planes_;
};

}  // namespace hefesto::db
