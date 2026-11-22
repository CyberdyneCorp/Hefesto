// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF macro obstructions (capability lef-write "Obstructions without
// hiding", "Obstructions with zero setback", "Obstructions with positive
// setback"; Q-LEFDEF-38): the OBS block of lefWriteMacro. Without -hide it
// is everything left in the flat copy after the pins; with -hide it is a
// cover of each hidden and well type over the layer's extent (OBS_BBOX
// included), minus the -hide 0 pin cut-outs, plus the flat copy's material
// in the setback strips.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefWrite.c lefWriteMacro (the
// obstruction part, lefWrite.c:1822-2033) and lefGetBound, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: LEFW (design D2).

#include <algorithm>
#include <array>

#include "hefesto/db/paint.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "lef/write/macro_impl.hpp"

namespace hefesto::lef::write_detail {

namespace {

// The four strips outside B shrunk by the setback, within the cell's
// bounding box: below, above, left, right.
std::array<db::Rect, 4> setback_strips(const MacroState& m) {
  const db::Rect bb = m.cell.bbox();
  const db::Rect& b = m.boundary;
  const db::Coord s = m.options.setback;
  db::Rect below = bb;
  below.y1 = b.y0 + s;
  db::Rect above = bb;
  above.y0 = b.y1 - s;
  db::Rect left = bb;
  left.y0 = b.y0 + s;
  left.y1 = b.y1 - s;
  left.x1 = b.x0 + s;
  db::Rect right = left;
  right.x0 = b.x1 - s;
  right.x1 = bb.x1;
  return {below, above, left, right};
}

// OBS_BBOX as four integers; a malformed one is reported and ignored.
db::Rect obs_bbox(const MacroState& m) {
  const db::PropertyValue* value = m.cell.properties().find("OBS_BBOX");
  if (value == nullptr) {
    return db::Rect{};
  }
  const auto* dims = std::get_if<db::DimensionList>(value);
  if (dims == nullptr || dims->values.size() != 4) {
    diag::Diagnostic d =
        diag::make("LEF-WRITE-007", "Improper values for obstruction bounding box OBS_BBOX property");
    // Magic's TxError has no line end here, so the next message joins it
    // on its line (LEFDEF-132); Hefesto ends the line with magic-compat OFF
    // (Q-LEFDEF-134, compat).
    d.no_newline = m.ctx.magic_compat;
    m.messages->push_back(std::move(d));
    return db::Rect{};
  }
  return db::Rect{dims->values[0], dims->values[1], dims->values[2], dims->values[3]};
}

bool has_paint(const db::Plane& plane) {
  return !plane.enumerate(db::whole_plane(), all_but_space_and_drc(), [](const db::TileView&) { return false; });
}

// The extent of all material left in the flat copy on the planes holding
// `type` (lefGetBound), enlarged by OBS_BBOX on planes where the original
// cell has paint.
db::Rect layer_bound(const MacroState& m, tech::TileType type, const db::Rect& manual) {
  db::Rect bound;
  for (std::size_t p = tech::kFirstUserPlane; p < m.flat->plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    if (!m.tech().plane_types(plane).test(type)) {
      continue;
    }
    m.flat->plane(plane).enumerate(db::whole_plane(), all_but_space_and_drc(), [&](const db::TileView& t) {
      bound = db::include(bound, t.area);
      return true;
    });
    if (!manual.is_null() && p < m.cell.plane_count() && has_paint(m.cell.plane(plane))) {
      bound = db::include(bound, manual);
    }
  }
  return bound;
}

// GeoClip: each side clamped; the result may be inverted (then nothing is
// painted).
db::Rect geo_clip(const db::Rect& r, const db::Rect& area) {
  return db::Rect{std::max(r.x0, area.x0), std::max(r.y0, area.y0), std::min(r.x1, area.x1), std::min(r.y1, area.y1)};
}

// The hidden-layer cover: each type of H or W over its layer's extent,
// clipped to B shrunk by the setback.
void paint_cover(const MacroState& m, db::CellDef& yank) {
  const db::Rect manual = obs_bbox(m);
  const db::Rect limit = db::expand(m.boundary, -m.options.setback);
  const tech::TypeMask covered = m.sets.hide | m.sets.well;
  const std::size_t end = std::min(m.tech().type_count(), tech::kMaxTileTypes);
  for (std::size_t t = tech::kFirstUserType; t < end; ++t) {
    const auto type = static_cast<tech::TileType>(t);
    if (!covered.test(type)) {
      continue;
    }
    const db::Rect r = geo_clip(layer_bound(m, type, manual), limit);
    if (!r.is_null()) {
      (void)db::paint_type(yank, m.tech(), r, type);
    }
  }
}

// The spacing a -hide 0 pin keeps from the cover: the widest wide-spacing
// rule of its type, else the type's default spacing to itself.
db::Coord cutout_spacing(const MacroState& m, tech::TileType type) {
  if (m.ctx.rules == nullptr || type == tech::kSpace) {
    return 0;
  }
  const db::Coord wide = drc::default_wide_spacing(*m.ctx.rules, m.tech(), type, 1000000);
  return wide != 0 ? wide : drc::default_spacing(*m.ctx.rules, m.tech(), type, type);
}

void cut_pins(const MacroState& m, db::CellDef& yank) {
  for (const Cutout& c : m.cutouts) {
    if (c.type == tech::kSpace) {
      continue;
    }
    (void)db::erase_type(yank, m.tech(), db::expand(c.area, cutout_spacing(m, c.type)), c.type);
  }
}

// The flat copy's routing material in the setback strips: whole tiles
// with magic-compat ON, clipped to the strips with it OFF (Q-LEFDEF-38,
// positive setback only).
void yank_strips(const MacroState& m, db::CellDef& yank) {
  const bool clip = !m.ctx.magic_compat && m.options.setback > 0;
  const Yank y = m.yank();
  const std::array<db::Rect, 4> strips = setback_strips(m);
  for (std::size_t p = tech::kFirstUserPlane; p < m.flat->plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    for (const db::Rect& r : strips) {
      yank_routing(y, m.flat->plane(plane), plane, r, yank.plane(plane),
                   clip ? std::optional<db::Rect>{r} : std::nullopt);
    }
  }
}

void yank_everything(const MacroState& m, db::CellDef& yank) {
  const Yank y = m.yank();
  for (std::size_t p = tech::kFirstUserPlane; p < m.flat->plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    yank_routing(y, m.flat->plane(plane), plane, db::whole_plane(), yank.plane(plane));
  }
}

// The cut round of one plane: everything without -hide, the setback
// strips with -hide d > 0, nothing with -hide 0.
void write_cuts(const MacroState& m, RecordWriter& w, db::CellDef& yank, tech::PlaneId plane) {
  const Yank y = m.yank();
  if (m.options.setback < 0) {
    yank_contacts(y, m.flat->plane(plane), plane, db::whole_plane(), yank.plane(plane), w.marks());
    w.write_plane(m.tech(), yank.plane(plane), m.sets.route, db::whole_plane(), Round::cut);
    return;
  }
  if (m.options.setback == 0) {
    return;
  }
  for (const db::Rect& r : setback_strips(m)) {
    yank_contacts(y, m.flat->plane(plane), plane, r, yank.plane(plane), w.marks());
    w.write_plane(m.tech(), yank.plane(plane), m.sets.route, r, Round::cut);
  }
}

}  // namespace

diag::Status write_obstructions(MacroState& m) {
  auto yank = scratch_cell(m.tech(), "__lefYank__");
  if (m.options.setback >= 0) {
    paint_cover(m, *yank);
    cut_pins(m, *yank);
    yank_strips(m, *yank);
  } else {
    yank_everything(m, *yank);
  }
  RecordWriter w = m.writer();
  w.begin(Block::obstruction);
  for (std::size_t p = tech::kFirstUserPlane; p < yank->plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    w.write_plane(m.tech(), yank->plane(plane), m.sets.route, db::whole_plane(), Round::routing);
    write_cuts(m, w, *yank, plane);
  }
  if (w.writes() > 0) {
    *m.out += std::string{kIn0} + "END\n";
  }
  return diag::Done{};
}

}  // namespace hefesto::lef::write_detail
