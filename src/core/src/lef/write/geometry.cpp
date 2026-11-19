// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LEF macro geometry (capability lef-write "Magic type to LEF layer name",
// "Geometry record emission", "Contact cut records", "PORT blocks",
// "Names written verbatim"; Q-LEFDEF-35, 39, 52): the LEF name of each
// magic type, the yank of routing material and contacts into the scratch
// planes, the erase of a pin's material from the flat copy, and the RECT
// and POLYGON records with their LAYER lines.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/lefWrite.c lefEraseGeometry,
// lefYankGeometry, lefYankContacts and lefWriteGeometry, and
// lef/defWrite.c defGetType and defMakeInverseLayerMap, Copyright (C)
// 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: LEFW (design D2).

#include <algorithm>

#include "hefesto/db/paint.hpp"
#include "lef/write/macro_impl.hpp"

namespace hefesto::lef::write_detail {

namespace {

db::Rect clip_to(const db::Rect& a, const db::Rect& b) noexcept {
  return db::Rect{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
}

// Paints `type` over the tile's area (or the triangle of a split tile's
// side) into `target`, as DBNMPaintPlane with DBStdPaintTbl does.
void paint_piece(const Yank& y, const db::TileView& t, const db::Rect& area, tech::PlaneId plane, tech::TileType type,
                 db::Plane& target) {
  const db::PaintRow row = db::paint_row(y.tech, plane, type);
  if (t.split) {
    (void)target.paint_triangle(area, t.diagonal, t.right_side, row, y.magic_compat, nullptr, db::PaintMerge::none);
  } else {
    (void)target.paint(area, row, db::PaintMerge::none);
  }
}

// The residues lefYankGeometry copies for a contact tile: those in R with
// a LEF name that live on `plane` (the paint table of a type on another
// plane leaves `plane` unchanged).
void yank_residues(const Yank& y, const db::TileView& t, const db::Rect& area, tech::PlaneId plane,
                   db::Plane& target) {
  const tech::TypeMask residues = y.tech.full_residues(t.type);
  const auto end = static_cast<tech::TileType>(std::min(y.tech.user_type_end(), tech::kMaxTileTypes));
  for (tech::TileType r = tech::kFirstUserType; r < end; ++r) {
    if (residues.test(r) && y.route.test(r) && y.names[r] != nullptr && y.tech.home_plane(r) == plane) {
      paint_piece(y, t, area, plane, r, target);
    }
  }
}

// lefYankContacts' type: the contact itself on its home plane, or the
// component of a stacked type whose home plane is `plane`.
std::optional<tech::TileType> cut_type_on(const tech::Technology& tech, tech::TileType type, tech::PlaneId plane) {
  if (type < tech.user_type_end()) {
    return tech.home_plane(type) == plane ? std::optional<tech::TileType>{type} : std::nullopt;
  }
  const tech::TypeMask components = residue_mask(tech, type);
  const auto end = static_cast<tech::TileType>(std::min(tech.user_type_end(), tech::kMaxTileTypes));
  for (tech::TileType s = tech::kFirstUserType; s < end; ++s) {
    if (components.test(s) && tech.home_plane(s) == plane) {
      return s;
    }
  }
  return std::nullopt;
}

}  // namespace

// defMakeInverseLayerMap(LAYER_MAP_VIAS): defGetType for every type.
NameMap make_name_map(const tech::Technology& tech, const LayerTable& layers) {
  NameMap map{};
  const std::size_t end = std::min(tech.type_count(), tech::kMaxTileTypes);
  for (std::size_t t = tech::kFirstUserType; t < end; ++t) {
    map.at(t) = layers.magic_to_lef(static_cast<tech::TileType>(t));
  }
  return map;
}

tech::TypeMask all_but_space_and_drc() {
  tech::TypeMask mask = tech::TypeMask::all();
  for (const tech::TileType t : {tech::kSpace, tech::kCheckPaint, tech::kCheckSubcell, tech::kErrorP, tech::kErrorS,
                                 tech::kErrorPS}) {
    mask.clear_type(t);
  }
  return mask;
}

tech::TypeMask residue_mask(const tech::Technology& tech, tech::TileType type) {
  const tech::ContactInfo* contact = tech.contact(type);
  if (contact == nullptr) {
    return {};
  }
  if (!contact->stacked) {
    return contact->residues;
  }
  tech::TypeMask components = tech::TypeMask::of(contact->component_a);
  components.set(contact->component_b);
  return components;
}

// lefYankGeometry.
void yank_routing(const Yank& y, const db::Plane& source, tech::PlaneId plane, const db::Rect& area,
                  db::Plane& target, const std::optional<db::Rect>& clip) {
  source.enumerate(area, all_but_space_and_drc(), [&](const db::TileView& t) {
    // A split tile is copied whole: clipping would change its triangle.
    const db::Rect piece = clip && !t.split ? clip_to(t.area, *clip) : t.area;
    if (y.tech.is_contact(t.type)) {
      yank_residues(y, t, piece, plane, target);
    } else if (y.route.test(t.type) && y.names[t.type] != nullptr && t.type < y.tech.user_type_end()) {
      paint_piece(y, t, piece, plane, t.type, target);
    }
    return true;
  });
}

// lefYankContacts.
void yank_contacts(const Yank& y, const db::Plane& source, tech::PlaneId plane, const db::Rect& area,
                   db::Plane& target) {
  source.enumerate(area, all_but_space_and_drc(), [&](const db::TileView& t) {
    if (t.split || !y.tech.is_contact(t.type)) {
      return true;
    }
    const std::optional<tech::TileType> cut = cut_type_on(y.tech, t.type, plane);
    if (cut && y.names[*cut] != nullptr) {
      (void)target.paint(t.area, db::paint_row(y.tech, plane, *cut), db::PaintMerge::none);
    }
    return true;
  });
}

// lefEraseGeometry: DBNMPaintPlane with the erase table, one plane only.
void erase_from(const Yank& y, const db::Plane& selection, tech::PlaneId plane, db::Plane& flat) {
  selection.enumerate(db::whole_plane(), all_but_space_and_drc(), [&](const db::TileView& t) {
    const db::PaintRow row = db::erase_row(y.tech, plane, t.type);
    if (t.split) {
      (void)flat.paint_triangle(t.area, t.diagonal, t.right_side, row, y.magic_compat, nullptr, db::PaintMerge::none);
    } else {
      (void)flat.paint(t.area, row, db::PaintMerge::none);
    }
    return true;
  });
}

void RecordWriter::begin(Block block) {
  block_ = block;
  writes_ = 0;
  last_type_ = tech::kSpace;
  written_cuts_.clear();
}

// lefWriteGeometry over one plane. Magic marks a split tile visited before
// it looks at its side, so with magic-compat ON only the first side
// reported is written (Q-LEFDEF-39); OFF writes both.
void RecordWriter::write_plane(const tech::Technology& tech, const db::Plane& yank, const tech::TypeMask& route,
                               const db::Rect& area, Round round) {
  const void* last_split = nullptr;
  yank.enumerate(area, route, [&](const db::TileView& t) {
    if (t.split) {
      if (magic_compat_ && t.tile == last_split) {
        return true;
      }
      last_split = t.tile;
    }
    if (round == Round::cut) {
      if (!tech.is_contact(t.type)) {
        return true;  // routing left over from the routing round
      }
      if (header && magic_compat_) {
        return false;  // the search stops at the first cut; the header stays pending
      }
      const auto key = std::make_tuple(static_cast<db::Coord>(t.type), t.area.x0, t.area.y0, t.area.x1, t.area.y1);
      if (!written_cuts_.insert(key).second) {
        return true;
      }
    }
    return record(t);
  });
}

bool RecordWriter::record(const db::TileView& t) {
  if (header) {
    const std::function<void()> pending = std::move(header);
    header = nullptr;
    pending();
  }
  if (writes_ == 0) {
    out_ += block_ == Block::port ? "    PORT\n" : "  OBS\n";
  }
  ++writes_;
  const LayerRecord* named = names_.at(t.type);
  if (t.type != last_type_ && named != nullptr) {
    out_ += std::string{kIn2} + "LAYER " + named->canonical_name + " ;\n";
    last_type_ = t.type;
  }
  shape(t);
  return true;
}

void RecordWriter::shape(const db::TileView& t) {
  const auto at = [&](db::Coord v) { return format_distance(oscale_ * static_cast<float>(v), units_); };
  const std::string l = at(t.area.x0);
  const std::string r = at(t.area.x1);
  const std::string b = at(t.area.y0);
  const std::string tp = at(t.area.y1);
  if (!t.split) {
    out_ += std::string{kIn3} + "RECT " + l + " " + b + " " + r + " " + tp + " ;\n";
    return;
  }
  // The triangle by its right-angle corner (TT_SIDE, TT_DIRECTION).
  std::string points;
  switch (db::triangle_corner(t.diagonal, t.right_side)) {
    case db::TriangleCorner::ne:
      points = l + " " + tp + " " + r + " " + tp + " " + r + " " + b;
      break;
    case db::TriangleCorner::se:
      points = r + " " + tp + " " + r + " " + b + " " + l + " " + b;
      break;
    case db::TriangleCorner::sw:
      points = l + " " + tp + " " + r + " " + b + " " + l + " " + b;
      break;
    case db::TriangleCorner::nw:
      points = l + " " + tp + " " + r + " " + tp + " " + l + " " + b;
      break;
  }
  out_ += std::string{kIn3} + "POLYGON " + points + " ;\n";
}

std::unique_ptr<db::CellDef> scratch_cell(const tech::Technology& tech, std::string name) {
  auto def = std::make_unique<db::CellDef>(std::move(name), tech.plane_count());
  def->flags().internal = true;
  def->flags().available = true;
  return def;
}

bool breaks_lef_tokens(std::string_view name) noexcept {
  return name.find_first_of(" ;#\n") != std::string_view::npos;
}

void warn_name(const MacroState& m, std::string_view what, std::string_view name) {
  if (breaks_lef_tokens(name)) {
    m.message("LEF-WRITE-008",
              std::string{what} + " name \"" + std::string{name} +
                  "\" contains a space, ';', '#' or newline; it is written verbatim (Q-LEFDEF-35)",
              true);
  }
}

void MacroState::message(std::string_view code, std::string text, bool native) const {
  if (messages != nullptr) {
    messages->push_back(native ? diag::make_native(code, std::move(text)) : diag::make(code, std::move(text)));
  }
}

}  // namespace hefesto::lef::write_detail
