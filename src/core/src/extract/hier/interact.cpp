// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Interaction areas (extract-hierarchy "Interaction halo and step squares",
// "Interaction area"). Ported from Magic 8.3.694 (fc5daa9e)
// extract/ExtSubtree.c (extSubtree's square loop, :164-284) and
// drc/DRCsubcell.c (DRCFindInteractions, drcSubcellFunc, drcFindOtherCells,
// drcSubCheckPaint), following the DRC engine's port
// (src/core/src/drc/engine/interaction.cpp, Q-MASK-400). Magic is Copyright
// (C) 1985, 1990 Regents of the University of California (permissive
// license, THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#include "hefesto/extract/hier/interact.hpp"

#include <functional>

#include "hefesto/extract/tech/style.hpp"
#include "hier_geometry.hpp"

namespace hefesto::extract {

namespace {

using hier::clip;
using hier::kNullRect;

const tech::TypeMask& paint_types() {
  static const tech::TypeMask mask = ~tech::TypeMask::of(tech::kSpace);
  return mask;
}

// drcIncludeArea over the paint planes: the bounding box of every non-space
// tile overlapping `area`, at full tile extent.
db::Rect paint_bbox(const db::CellDef& def, const db::Rect& area) {
  db::Rect out = kNullRect;
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count(); ++p) {
    def.plane(static_cast<tech::PlaneId>(p)).enumerate(area, paint_types(), [&](const db::TileView& t) {
      out = db::include(out, t.area);
      return true;
    });
  }
  return out;
}

bool has_paint(const db::CellDef& def, const db::Rect& area) {
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count(); ++p) {
    const bool stopped = !def.plane(static_cast<tech::PlaneId>(p)).enumerate(area, paint_types(), [](const db::TileView&) {
      return false;
    });
    if (stopped) {
      return true;
    }
  }
  return false;
}

// DBCellSrArea over every element of the uses of `def` overlapping `area`;
// `visit` returns false to stop. Returns false when stopped.
bool search_elements(const db::CellDef& def, const db::Rect& area,
                     const std::function<bool(const db::CellDef&, const db::Rect&)>& visit) {
  for (const db::CellUse* u : hier::uses_in(def, area)) {
    const bool stopped = hier::array_elements(*u, area, [&](const hier::Element& e) {
      return !visit(*u->def, db::apply(db::invert(e.to_parent), area));
    });
    if (stopped) {
      return false;
    }
  }
  return true;
}

// DBTreeSrTiles(..., DBAllButSpaceAndDRCBits, drcAlwaysOne): paint anywhere
// in the subtree of `def` under `area`.
bool subtree_has_paint(const db::CellDef& def, const db::Rect& area) {
  if (has_paint(def, area)) {
    return true;
  }
  return !search_elements(def, area, [](const db::CellDef& d, const db::Rect& a) { return !subtree_has_paint(d, a); });
}

struct LinkedUse {
  const db::CellUse* use = nullptr;
  db::Rect area;
};

// drcSubcellFunc for every use near `search`; returns whether one was found.
bool subcell_pass(const db::CellDef& def, const db::Rect& search, std::int32_t radius, db::Rect& total,
                  std::vector<LinkedUse>& linked) {
  const std::vector<const db::CellUse*> uses = hier::uses_in(def, search);
  for (const db::CellUse* u : uses) {
    const db::Rect halo_area = clip(db::expand(u->bbox, radius), search);
    db::Rect found = paint_bbox(def, halo_area);
    if (!db::surround(found, halo_area)) {
      linked.insert(linked.begin(), LinkedUse{u, halo_area});  // dsa_dlu is a stack
    }
    if (found.is_null()) {
      continue;
    }
    found = clip(db::expand(found, radius), halo_area);
    total = db::include(total, found);
  }
  return !uses.empty();
}

// drcFindOtherCells over the linked uses. Q-MASK-400: with magic-compat ON
// the seed GeoNullRect is expanded and included even when no sibling is
// found; OFF skips a use without siblings.
void sibling_pass(const db::CellDef& def, std::int32_t radius, const std::vector<LinkedUse>& linked, bool magic_compat,
                  db::Rect& total) {
  for (const LinkedUse& l : linked) {
    db::Rect others = kNullRect;
    bool found = false;
    for (const db::CellUse* u : hier::uses_in(def, l.area)) {
      if (u != l.use) {
        others = db::include(others, u->bbox);
        found = true;
      }
    }
    if (!found && !magic_compat) {
      continue;
    }
    others = clip(db::expand(others, radius), l.area);
    total = db::include(total, others);
  }
}

// The second pass: at least two sources of paint in `area` (drcSubCheckPaint,
// with the DRC engine's nested-use quirk).
bool two_sources(const db::CellDef& def, const db::Rect& area) {
  int sources = has_paint(def, area) ? 1 : 0;
  std::function<bool(const db::CellDef&, const db::Rect&)> walk = [&](const db::CellDef& d, const db::Rect& a) {
    return search_elements(d, a, [&](const db::CellDef& child, const db::Rect& child_area) {
      if (subtree_has_paint(child, child_area) && ++sources >= 2) {
        return false;
      }
      return walk(child, child_area);
    });
  };
  (void)walk(def, area);
  return sources >= 2;
}

// The labels of `cell` touching `square` widen the interaction area
// (extSubtree, ExtSubtree.c:211-247). `found` is DRCFindInteractions' result.
std::optional<db::Rect> widen_by_labels(const db::CellDef& cell, const db::Rect& square, const db::Rect& bloat,
                                        std::optional<db::Rect> found) {
  for (const db::Label& lab : cell.labels()) {
    if (!db::overlap(lab.rect, square) && !db::touch(lab.rect, square)) {
      continue;
    }
    const db::Rect rlab = clip(lab.rect, bloat);
    if (found) {
      found = hier::include_all(*found, rlab);
      continue;
    }
    db::Rect area = rlab;
    if (area.x1 - area.x0 == 0) {
      ++area.x1;
      --area.x0;
    }
    if (area.y1 - area.y0 == 0) {
      ++area.y1;
      --area.y0;
    }
    found = area;
  }
  return found;
}

InteractionSquare classify(const db::CellDef& cell, const db::Rect& square, std::int32_t halo, bool magic_compat) {
  InteractionSquare out;
  out.square = square;
  const db::Rect bloat = db::expand(square, halo);
  const auto result = find_interaction(cell, bloat, halo, magic_compat);
  if (!result) {
    return out;
  }
  const std::optional<db::Rect> area = widen_by_labels(cell, square, bloat, *result);
  if (!area) {
    out.kind = SquareKind::substrate;
    return out;
  }
  out.kind = SquareKind::interaction;
  out.area = *area;
  out.clip = clip(*area, square);
  return out;
}

}  // namespace

std::int32_t hier_halo(const ExtStyle& style, const ExtractOptions& options, const Overrides& overrides) {
  if (!options.coupling || !options.adjust) {
    return 1;
  }
  return overrides.halo.value_or(style.side_halo) + 1;
}

std::int32_t hier_step(const ExtStyle& style, const Overrides& overrides) {
  return overrides.step_size.value_or(style.step_size);
}

std::optional<std::optional<db::Rect>> find_interaction(const db::CellDef& cell, const db::Rect& area,
                                                        std::int32_t radius, bool magic_compat) {
  const db::Rect search = db::expand(area, radius);
  db::Rect total = kNullRect;
  std::vector<LinkedUse> linked;
  if (!subcell_pass(cell, search, radius, total, linked)) {
    return std::nullopt;
  }
  sibling_pass(cell, radius, linked, magic_compat, total);
  if (total.is_null() || !two_sources(cell, total)) {
    return std::optional<db::Rect>{};
  }
  const db::Rect found = clip(total, area);
  if (found.is_null()) {
    return std::optional<db::Rect>{};
  }
  return std::optional<db::Rect>{found};
}

std::vector<InteractionSquare> interaction_squares(const db::CellDef& cell, std::int32_t step, std::int32_t halo,
                                                   bool magic_compat) {
  std::vector<InteractionSquare> out;
  const db::Rect b = cell.bbox();
  const std::int64_t sx = step > 0 ? step : std::max<std::int64_t>(b.width(), 1);
  const std::int64_t sy = step > 0 ? step : std::max<std::int64_t>(b.height(), 1);
  for (std::int64_t y = b.y0; y < b.y1; y += sy) {
    for (std::int64_t x = b.x0; x < b.x1; x += sx) {
      const db::Rect square{static_cast<db::Coord>(x), static_cast<db::Coord>(y), static_cast<db::Coord>(x + sx),
                            static_cast<db::Coord>(y + sy)};
      out.push_back(classify(cell, square, halo, magic_compat));
    }
  }
  return out;
}

}  // namespace hefesto::extract
