// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The route writer of the SPECIALNETS and NETS sections (capability
// def-write "Wire segment derivation", "Route record assembly"; Q-LEFDEF-95,
// 96, 97 and 112).
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defWrite.c defCheckForBreak,
// defWriteRouteWidth, defWriteCoord, defNetGeometryFunc, defMaxWireFunc,
// defMinWireFunc and defExemptWireFunc, Copyright (C) 1985, 1990 Regents
// of the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: DEFW (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D2).

#include "def/write/route.hpp"

#include <algorithm>
#include <limits>
#include <string>

#include "hefesto/drc/rules/query.hpp"

namespace hefesto::def::write_detail {

namespace {

constexpr std::size_t kMaxColumns = 70;  // MAX_DEF_COLUMNS

bool inside(const db::Rect& outer, const db::Rect& inner) {
  return inner.x0 >= outer.x0 && inner.x1 <= outer.x1 && inner.y0 >= outer.y0 && inner.y1 <= outer.y1;
}

/// GEO_TOUCH: the rectangles share an edge or overlap.
bool touching(const db::Rect& a, const db::Rect& b) {
  return a.x0 <= b.x1 && a.x1 >= b.x0 && a.y0 <= b.y1 && a.y1 >= b.y0;
}

/// The residues of `type` (DBResidueMask): a contact's, else itself.
tech::TypeMask residue_mask(const tech::Technology& tech, tech::TileType type) {
  const tech::ContactInfo* info = tech.contact(type);
  return info != nullptr ? info->residues : tech::TypeMask::of(type);
}

}  // namespace

RouteWriter::RouteWriter(const Context& c, NetMode mode, const ViaPositions& vias, RuleBook& rules,
                         std::string& out)
    : c_(c), mode_(mode), vias_(vias), rules_(rules), out_(out) {
  const compat::CompatSettings compat{c.compat, true};
  for (const lef::ContactRegion& r : lef::contact_regions(c.cell, c.tech, compat)) {
    regions_.emplace(std::make_tuple(r.plane, r.area.y0, r.area.x0), r);
  }
}

void RouteWriter::begin_net(std::size_t name_length) {
  column_ = 5 + name_length;
  has_prev_ = false;
  prev_type_ = tech::kSpace;
  seen_.clear();
}

void RouteWriter::check_break(std::size_t add) {
  column_ += add;
  if (column_ > kMaxColumns) {
    out_ += "\n      ";
    column_ = 6 + add;
  }
}

void RouteWriter::end_net() {
  if (has_prev_) {
    if (prev_orient_ != Orient::center) {
      coord(px_, py_, prev_orient_);
    }
    column_ = 0;
  }
}

void RouteWriter::coord(float x, float y, Orient o) {
  if (column_ + 12 > kMaxColumns) {
    out_ += "\n      ";
    column_ = 6;
  }
  std::size_t total = 4;
  out_ += " ( ";
  const std::string xs = o == Orient::north ? std::string{"*"} : g10(x);
  const std::string ys = o == Orient::east ? std::string{"*"} : g10(y);
  out_ += xs + " " + ys + " )";
  total += xs.size() + 1 + ys.size() + 1;
  column_ += total;
}

void RouteWriter::route_width(db::Coord width) {
  const std::string v = g10(scaled(width, c_.scale));
  check_break(v.size() + 1);
  out_ += v + " ";
}

void RouteWriter::tile(const db::TileView& tile, tech::PlaneId plane) {
  if (tile.type == tech::kSpace || exempt_.contains(tile.tile)) {
    return;
  }
  if (!c_.compat && !seen_.insert({plane, tile.tile}).second) {
    return;  // Q-LEFDEF-112 OFF: each tile once per net
  }
  if (c_.tech.is_contact(tile.type)) {
    contact_regions(tile, plane);
    return;
  }
  process(tile, plane, tile.type, tile.area);
}

// The contact regions whose lower-left corner lies in `tile` (the
// bottom-most, left-most tile of its region; several with magic-compat OFF,
// Q-LEFDEF-111).
void RouteWriter::contact_regions(const db::TileView& tile, tech::PlaneId plane) {
  const tech::ContactInfo* info = c_.tech.contact(tile.type);
  const auto lo = regions_.lower_bound({plane, tile.area.y0, std::numeric_limits<db::Coord>::min()});
  for (auto it = lo; it != regions_.end() && std::get<0>(it->first) == plane && std::get<1>(it->first) < tile.area.y1;
       ++it) {
    const lef::ContactRegion& r = it->second;
    const bool mine = r.type == tile.type || (info != nullptr && info->stacked && info->residues.test(r.type));
    if (mine && r.area.x0 >= tile.area.x0 && r.area.x0 < tile.area.x1) {
      process(tile, plane, r.type, r.area);
    }
  }
}

void RouteWriter::process(const db::TileView& tile, tech::PlaneId plane, tech::TileType type, const db::Rect& area) {
  const LefMapping& m = c_.map.at(type);
  if (!m.name) {
    return;  // types not in the LEF definition are not written
  }
  const std::optional<RuleBook::Selected> last_rule = rule_;
  Segment s;
  s.type = type;
  s.r = area;
  s.rorig = area;
  s.w = area.x1 - area.x0;
  s.h = area.y1 - area.y0;
  s.midline = area.y0 + area.y1;
  set_width(s, m);
  if (s.orient != Orient::center) {
    if (!wire(s, plane, *m.name)) {
      return;
    }
    wire_endpoints(s);
  } else {
    via_endpoints(s, c_.scale);
    residues(s);
  }
  bool same = has_prev_ && same_route(s);
  if (same && rule_) {
    same = false;  // a via after a taper rule cancels it
  }
  if (last_rule != rule_) {
    same = false;
  }
  emit(s, same, *m.name);
  update(s, tile);
}

void RouteWriter::set_width(Segment& s, const LefMapping& m) const {
  const bool via = m.has_record ? m.via_class : c_.tech.is_contact(s.type);
  if (via) {
    s.orient = Orient::center;
  }
  if (mode_ != NetMode::regular) {
    s.width = std::min(s.w, s.h);
    return;
  }
  s.width = 0;
  if (!via && m.has_record) {
    s.width = m.route_width.value_or(0);
  }
  if (s.width == 0 && c_.rules != nullptr) {
    s.width = drc::default_width(*c_.rules, c_.tech, s.type);
  }
}

// The non-via part of defNetGeometryFunc: orientation, slivers and the
// non-default width rule. False: the tile writes nothing.
bool RouteWriter::wire(Segment& s, tech::PlaneId plane, const std::string& lef_name) {
  const db::Coord rw = s.width;
  if (s.h != rw && (s.w == rw || (rw == 0 && s.h > s.w))) {
    s.orient = Orient::north;
    s.midline = s.r.x0 + s.r.x1;
  }
  rule_.reset();
  const bool odd = (s.h != rw && s.w != rw) || (s.h == rw && s.w < rw) || (s.w == rw && s.h < rw);
  if (!odd) {
    return true;
  }
  if (s.w < rw) {
    return false;
  }
  if (s.h < rw && !grow_sliver(s, plane)) {
    return false;
  }
  if (s.h != rw && s.w != rw && mode_ == NetMode::regular) {
    rule_ = rules_.select(c_, s.type, lef_name, std::min(s.w, s.h));
  }
  const bool vertical = s.h > s.w;
  s.orient = vertical ? Orient::north : Orient::east;
  s.midline = vertical ? s.r.x0 + s.r.x1 : s.r.y0 + s.r.y1;
  return true;
}

// A sliver lower than the route width: grows it up and down to the nearest
// tile that does not connect, in steps of the width; false when it stays
// too low. Tiles inside the grown piece are not written again.
bool RouteWriter::grow_sliver(Segment& s, tech::PlaneId plane) {
  const db::Plane& p = c_.cell.plane(plane);
  const tech::TypeMask& other = c_.tech.not_connected(s.type);
  const db::Coord rw = s.width;
  db::Rect up = s.rorig;
  up.y1 = s.rorig.y0 + rw;
  up.y0 = s.rorig.y1;
  db::Coord top = 0;
  do {
    up.y1 += rw;
    top = up.y1;
    (void)p.enumerate(up, other, [&](const db::TileView& t) {
      top = std::min(top, t.area.y0);
      return true;
    });
  } while (top == up.y1 && up.y1 < db::kPlaneMax);
  db::Rect down = s.rorig;
  down.y0 = s.rorig.y1 - rw;
  down.y1 = s.rorig.y0;
  db::Coord bottom = 0;
  do {
    down.y0 -= rw;
    bottom = down.y0;
    (void)p.enumerate(down, other, [&](const db::TileView& t) {
      bottom = std::max(bottom, t.area.y1);
      return true;
    });
  } while (bottom == down.y0 && down.y0 > -db::kPlaneMax);
  s.r = s.rorig;
  s.r.y1 = std::max(s.r.y1, top);
  s.r.y0 = std::min(s.r.y0, bottom);
  s.h = s.r.y1 - s.r.y0;
  if (s.h < rw) {
    return false;
  }
  if (s.r != s.rorig) {
    (void)p.enumerate(s.r, c_.tech.connects_to(s.type), [&](const db::TileView& t) {
      if (!c_.tech.is_contact(t.type) && inside(s.r, t.area)) {
        exempt_.insert(t.tile);
      }
      return true;
    });
  }
  return true;
}

void RouteWriter::wire_endpoints(Segment& s) const {
  const float scale = c_.scale;
  const bool east = s.orient == Orient::east;
  const float mid = static_cast<float>(s.midline) * scale / 2;
  float lo = scaled(east ? s.r.x0 : s.r.y0, scale);
  float hi = scaled(east ? s.r.x1 : s.r.y1, scale);
  if (s.width == 0) {
    s.width = east ? s.h : s.w;
  }
  if (mode_ == NetMode::regular) {
    const db::Coord q = rule_ ? rule_->width : s.width;
    // Q-LEFDEF-95: Magic halves the width as an integer.
    const float inset = c_.compat ? static_cast<float>(q / 2) * scale : static_cast<float>(q) * scale / 2;
    lo = lo + inset;
    hi = hi - inset;
  }
  s.x1 = east ? lo : mid;
  s.x2 = east ? hi : mid;
  s.y1 = east ? mid : lo;
  s.y2 = east ? mid : hi;
}

void RouteWriter::via_endpoints(Segment& s, float scale) {
  s.y1 = static_cast<float>(s.midline) * scale / 2;
  s.y2 = s.y1;
  s.x1 = static_cast<float>(s.r.x0 + s.r.x1) * scale / 2;
  s.x2 = s.x1;
}

void RouteWriter::residues(Segment& s) const {
  const tech::TypeMask mask = residue_mask(c_.tech, s.type);
  for (std::size_t t = tech::kFirstUserType; t < c_.tech.user_type_end(); ++t) {
    const auto stype = static_cast<tech::TileType>(t);
    if (!mask.test(stype)) {
      continue;
    }
    if (s.rtype == tech::kSpace && (stype == prev_type_ || !has_prev_)) {
      s.rtype = stype;
    } else {
      s.r2type = stype;
    }
  }
}

// "sameroute": the end strip of the previous piece touches the start strip
// of this one along the previous direction.
bool RouteWriter::same_route(const Segment& s) const {
  db::Rect r2 = prev_area_;
  if (prev_orient_ == Orient::east) {
    r2.x0 = r2.x1 - 1;
  } else if (prev_orient_ == Orient::north) {
    r2.y0 = r2.y1 - 1;
  }
  db::Rect r = s.r;
  if (s.orient == Orient::east) {
    r.x1 = r.x0 + 1;
  } else if (s.orient == Orient::north) {
    r.y1 = r.y0 + 1;
  }
  if (!touching(r, r2)) {
    return false;
  }
  const bool special = mode_ != NetMode::regular;
  switch (prev_orient_) {
    case Orient::east:
      return (r.x0 == r2.x1 || r.x1 == r2.x0) && !(special && (r.y1 != r2.y1 || r.y0 != r2.y0));
    case Orient::north:
      return (r.y0 == r2.y1 || r.y1 == r2.y0) && !(special && (r.x1 != r2.x1 || r.x0 != r2.x0));
    case Orient::center:
      break;
  }
  return !special;  // a special net never continues from a via
}

void RouteWriter::emit(const Segment& s, bool same, const std::string& lef_name) {
  if (same && s.type == prev_type_) {
    continue_route(s, lef_name);
  } else if (same && s.orient == Orient::center && s.rtype == prev_type_) {
    continue_through_via(s, lef_name);
  } else {
    new_record(s, lef_name);
  }
}

// A route reaching a via: bring it to the via centre, then name the via.
void RouteWriter::continue_through_via(const Segment& s, const std::string& lef_name) {
  if (prev_orient_ == Orient::north && s.x1 == px_) {
    coord(px_, s.y1, prev_orient_);
  } else if (prev_orient_ == Orient::east && s.y1 == py_) {
    coord(s.x1, py_, prev_orient_);
  } else if (prev_orient_ == Orient::east) {
    coord(s.x1, py_, prev_orient_);
    coord(s.x1, s.y1, Orient::north);
  } else if (prev_orient_ == Orient::north) {
    coord(px_, s.y1, prev_orient_);
    coord(s.x1, s.y1, Orient::east);
  }
  via_name(s, lef_name);
}

void RouteWriter::new_record(const Segment& s, const std::string& lef_name) {
  if (has_prev_) {
    if (prev_orient_ != Orient::center) {
      coord(px_, py_, prev_orient_);
    }
    out_ += "\n      NEW ";
    column_ = 10;
  } else {
    out_ += "\n      + ROUTED ";
    column_ = 15;
  }
  if (s.orient == Orient::center) {
    route_layer(s);
  } else {
    check_break(lef_name.size() + 1);
    out_ += lef_name + " ";
  }
  if (rule_) {
    out_ += "TAPERRULE " + rule_->name + " ";  // not counted (Q-LEFDEF-97)
  }
  if (mode_ != NetMode::regular) {
    route_width(s.width);
  }
  coord(s.x1, s.y1, Orient::center);
  if (s.orient == Orient::center) {
    via_name(s, lef_name);
  }
}

// The route layer a record that starts on a via names: its residue that
// continues the route, else the other one.
void RouteWriter::route_layer(const Segment& s) {
  const tech::TileType t = s.rtype == tech::kSpace ? s.r2type : s.rtype;
  const auto& name = c_.map.name(t);
  if (!name) {
    // Magic takes strlen(NULL) here and crashes; fixed in both modes.
    c_.say_native(kMagicCrash, "No LEF route layer for magic type \"" + std::string{c_.tech.type_name(t)} +
                                   "\" of a via; Magic 8.3.694 crashes here. No layer name is written.");
    return;
  }
  check_break(name->size() + 1);
  out_ += *name + " ";
}

void RouteWriter::continue_route(const Segment& s, const std::string& lef_name) {
  // Q-LEFDEF-96: Magic compares the previous y with x2.
  const float dog = c_.compat ? s.x2 : s.y1;
  if (s.orient == prev_orient_ && px_ != s.x1 && py_ != dog) {
    coord(px_, py_, prev_orient_);
    coord(s.x1, s.y1, s.orient);
    return;
  }
  switch (prev_orient_) {
    case Orient::east:
      if (px_ == s.x1) {
        coord(s.x1, py_, prev_orient_);
      } else {
        restart(s, lef_name, false);
      }
      return;
    case Orient::north:
      if (py_ == s.y1) {
        coord(px_, s.y1, prev_orient_);
      } else {
        restart(s, lef_name, true);
      }
      return;
    case Orient::center:
      break;
  }
  if (s.orient == Orient::north && s.x1 != px_) {
    coord(s.x1, py_, Orient::east);
  } else if (s.orient == Orient::east && s.y1 != py_) {
    coord(px_, s.y1, Orient::north);
  }
}

// "Don't know how to connect the route segments": end the route and start
// a NEW one. Magic sets the column after the width for a vertical route
// (Q-LEFDEF-97, replicated).
void RouteWriter::restart(const Segment& s, const std::string& lef_name, bool column_after_width) {
  coord(px_, py_, prev_orient_);
  out_ += "\n      NEW " + lef_name;
  if (!column_after_width) {
    column_ = 10 + lef_name.size();
  }
  if (mode_ != NetMode::regular) {
    out_ += " ";
    ++column_;
    route_width(s.width);
  }
  if (column_after_width) {
    column_ = 10 + lef_name.size();
  }
  coord(s.x1, s.y1, Orient::center);
}

void RouteWriter::via_name(const Segment& s, const std::string& lef_name) {
  const auto plane = c_.tech.home_plane(s.type);
  const std::string key = std::string{c_.tech.plane_name(plane.value_or(0), tech::NameForm::short_name)} + "_" +
                          std::to_string(s.rorig.x0) + "_" + std::to_string(s.rorig.y0);
  const auto found = vias_.find(key);
  if (found != vias_.end()) {
    check_break(found->second.size() + 2);
    out_ += " " + found->second + " ";
    return;
  }
  c_.say(kNoVia, "Cannot find via name " + key + " in table!");
  const std::string suffix = "_" + g10(scaled(s.w, c_.scale)) + "_" + g10(scaled(s.h, c_.scale));
  check_break(lef_name.size() + suffix.size() + 2);
  out_ += " " + lef_name + suffix + " ";
}

void RouteWriter::update(const Segment& s, const db::TileView& tile) {
  if (s.orient == Orient::center) {
    prev_type_ = s.rtype != tech::kSpace && s.r2type != tech::kSpace ? s.r2type : tech::kSpace;
  } else {
    prev_type_ = s.type;
  }
  px_ = s.x2;
  py_ = s.y2;
  prev_orient_ = s.orient;
  prev_area_ = tile.area;
  has_prev_ = true;
}

}  // namespace hefesto::def::write_detail
