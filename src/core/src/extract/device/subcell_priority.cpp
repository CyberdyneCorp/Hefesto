// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Subcell device priority (extract-devices "Subcell device priority").
// Ported from Magic 8.3.694 (fc5daa9e) extract/ExtBasic.c (extBasic's
// region loop, :251-283, and extFoundFunc, :173-182); Magic is Copyright (C)
// 1985, 1990 Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTDEV.

#include "hefesto/db/array_overlap.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/use_index.hpp"
#include "hefesto/extract/device/devices.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract {

namespace {

bool own_paint(const db::CellDef& def, const db::Rect& area, const tech::TypeMask& types) {
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count(); ++p) {
    const bool stopped = !def.plane(static_cast<tech::PlaneId>(p)).enumerate(area, types, [](const db::TileView&) {
      return false;
    });
    if (stopped) {
      return true;
    }
  }
  return false;
}

// DBTreeSrTiles with extFoundFunc: a tile of `types` under `area` in any
// cell below `def` (the paint of `def` itself does not count).
bool below(const db::CellDef& def, const db::Rect& area, const tech::TypeMask& types) {
  bool found = false;
  (void)db::uses_overlapping(def, area, [&](db::CellUse& u) {
    if (u.def == nullptr) {
      return true;
    }
    const db::ElementRange r = db::array_overlap(u, area);
    for (std::int32_t y = r.ylo; y <= r.yhi && !found; ++y) {
      for (std::int32_t x = r.xlo; x <= r.xhi && !found; ++x) {
        const db::Rect child = db::apply(db::invert(db::element_transform(u, x, y)), area);
        found = own_paint(*u.def, child, types) || below(*u.def, child, types);
      }
    }
    return !found;
  });
  return found;
}

}  // namespace

void disable_subcell_devices(const db::CellDef& cell, std::vector<DeviceRegionTiles>& regions,
                             const tech::Technology& technology, const ExtStyle& style) {
  for (DeviceRegionTiles& reg : regions) {
    const db::Point ll{reg.first.area.x0, reg.first.area.y0};
    const db::Rect square{ll.x, ll.y, ll.x + 1, ll.y + 1};
    const tech::TypeMask mask = style.device_types & technology.plane_types(reg.region.plane);
    if (below(cell, square, mask)) {
      reg.disabled = true;
    }
  }
}

}  // namespace hefesto::extract
