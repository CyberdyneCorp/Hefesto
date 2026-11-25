// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The antenna area measurement (antenna/measure.hpp; capability
// antenna-check "Antenna area measurement", "Per-plane progressive
// evaluation" steps 2-4): the traced net, the tie-down and gate areas, and
// the per-type surface and sidewall areas with the contact cut count of
// Q-MISC-4 per mode.
//
// Ported from Magic 8.3.694 (fc5daa9e) extflat/EFantenna.c:782-1016
// (areaAccumFunc, antennaAccumFunc), Copyright (C) 1985, 1990 Regents of
// the University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: ANTENNA.

#include <algorithm>
#include <vector>

#include "hefesto/antenna/measure.hpp"
#include "hefesto/db/paint.hpp"

namespace hefesto::antenna {

namespace {

[[nodiscard]] std::int64_t tile_area(const db::TileView& t) noexcept {
  const std::int64_t area = t.area.width() * t.area.height();
  return t.split ? area / 2 : area;
}

[[nodiscard]] bool on_plane(const tech::Technology& tech, tech::TileType type, tech::PlaneId plane) noexcept {
  return (tech.type_planes(type) & tech::plane_bit(plane)) != 0;
}

// areaAccumFunc: a contact counts on its home plane only.
AreaSum accumulate(const TracedNet& net, const tech::Technology& tech, tech::PlaneId plane, const tech::TypeMask& types,
                   AreaSum sum) {
  (void)net.plane(plane).enumerate(db::whole_plane(), types, [&](const db::TileView& t) {
    if (tech.is_contact(t.type) && tech.home_plane(t.type) != plane) {
      return true;
    }
    sum.last = t.area;
    sum.area += tile_area(t);
    return true;
  });
  return sum;
}

// The type of `t` on its side facing `side` of the tile it neighbours
// (TiGetBottomType of a tile above, and so on).
[[nodiscard]] tech::TileType facing_type(const db::TileView& t, db::Side side) noexcept {
  if (!t.split) {
    return t.type;
  }
  const bool backslash = t.diagonal == db::Diagonal::nw_se;
  switch (side) {
    case db::Side::top:  // the neighbour's bottom
      return backslash ? t.left_type : t.right_type;
    case db::Side::bottom:  // the neighbour's top
      return backslash ? t.right_type : t.left_type;
    case db::Side::left:  // the neighbour's right
      return t.right_type;
    case db::Side::right:  // the neighbour's left
      return t.left_type;
  }
  return t.type;
}

// The length of side `side` of `t` that abuts space.
std::int64_t space_edge(const db::Plane& plane, const db::TileView& t, db::Side side,
                        std::vector<db::TileView>& scratch) {
  const bool horizontal = side == db::Side::top || side == db::Side::bottom;
  const db::Coord lo = horizontal ? t.area.x0 : t.area.y0;
  const db::Coord hi = horizontal ? t.area.x1 : t.area.y1;
  plane.neighbors(t, side, lo, hi, scratch);
  std::int64_t length = 0;
  for (const db::TileView& n : scratch) {
    if (facing_type(n, side) != tech::kSpace) {
      continue;
    }
    const db::Coord nlo = horizontal ? n.area.x0 : n.area.y0;
    const db::Coord nhi = horizontal ? n.area.x1 : n.area.y1;
    length += std::int64_t{std::min(hi, nhi)} - std::max(lo, nlo);
  }
  return length;
}

// The perimeter of `t` where it abuts space on its plane.
std::int64_t space_perimeter(const db::Plane& plane, const db::TileView& t) {
  std::vector<db::TileView> scratch;
  std::int64_t perimeter = 0;
  for (const db::Side side : {db::Side::top, db::Side::bottom, db::Side::left, db::Side::right}) {
    perimeter += space_edge(plane, t, side, scratch);
  }
  return perimeter;
}

// The residues of `contact` on `plane` (DBFullResidueMask restricted to the
// plane), in type order.
std::vector<tech::TileType> residues_on(const tech::Technology& tech, tech::TileType contact, tech::PlaneId plane) {
  std::vector<tech::TileType> out;
  const tech::TypeMask residues = tech.full_residues(contact);
  for (std::size_t r = tech::kFirstUserType; r < tech.type_count(); ++r) {
    const auto type = static_cast<tech::TileType>(r);
    if (residues.test(type) && on_plane(tech, type, plane)) {
      out.push_back(type);
    }
  }
  return out;
}

[[nodiscard]] std::int64_t sidewall(std::int64_t length, float thickness) noexcept {
  return static_cast<std::int64_t>(static_cast<float>(length) * thickness);
}

// One tile's contribution to a[.] (antennaAccumFunc).
class Accumulator {
 public:
  Accumulator(const TracedNet& net, const MeasureSetup& setup, tech::PlaneId plane, AntennaAreas& out)
      : net_(net), setup_(setup), plane_(plane), out_(out) {}

  void add(const db::TileView& t) {
    const bool contact = setup_.tech->is_contact(t.type);
    switch (setup_.rules->calc_type(t.type)) {
      case extract::AntennaModel::sidewall:
        contact ? sidewall_contact(t) : sidewall_plain(t);
        break;
      case extract::AntennaModel::surface:
        contact ? surface_contact(t) : surface_plain(t);
        break;
      case extract::AntennaModel::none:  // Q-MISC-3: no calculation type, no area
        break;
    }
  }

 private:
  [[nodiscard]] ContactCuts cuts(const db::TileView& t) const {
    const std::optional<CutRule> rule = setup_.cuts ? setup_.cuts(t.type) : std::nullopt;
    return contact_cuts(static_cast<db::Coord>(t.area.width()), static_cast<db::Coord>(t.area.height()), rule,
                        setup_.magic_compat);
  }

  [[nodiscard]] bool stacked(tech::TileType type) const noexcept { return type >= setup_.tech->user_type_end(); }

  void sidewall_contact(const db::TileView& t) {
    const std::int64_t cperim = cuts(t).perimeter;
    const std::vector<tech::TileType> residues = residues_on(*setup_.tech, t.type, plane_);
    for (const tech::TileType r : residues) {
      out_.area[r] += sidewall(cperim, setup_.rules->thickness(r));
    }
    // The partial model ignores the via's own contribution.
    if (setup_.rules->partial()) {
      return;
    }
    if (!stacked(t.type)) {
      out_.area[t.type] += sidewall(cperim, setup_.rules->thickness(t.type));
    } else if (!residues.empty()) {
      out_.area[residues.front()] += sidewall(cperim, setup_.rules->thickness(residues.front()));
    }
  }

  void sidewall_plain(const db::TileView& t) {
    out_.last = t.area;
    const std::int64_t perimeter = space_perimeter(net_.plane(plane_), t);
    out_.area[t.type] += sidewall(perimeter, setup_.rules->thickness(t.type));
  }

  void surface_contact(const db::TileView& t) {
    const std::int64_t area = cuts(t).area;
    const std::vector<tech::TileType> residues = residues_on(*setup_.tech, t.type, plane_);
    for (const tech::TileType r : residues) {
      out_.area[r] += area;
    }
    if (!stacked(t.type)) {
      out_.area[t.type] += area;
    } else if (!residues.empty()) {
      out_.area[residues.front()] += area;
    }
  }

  void surface_plain(const db::TileView& t) {
    out_.last = t.area;
    out_.area[t.type] += tile_area(t);
  }

  const TracedNet& net_;
  const MeasureSetup& setup_;
  tech::PlaneId plane_;
  AntennaAreas& out_;
};

// DBAllButSpaceAndDRCBits.
tech::TypeMask all_but_space(const tech::Technology& tech) {
  tech::TypeMask mask;
  for (std::size_t t = tech::kFirstUserType; t < tech.type_count(); ++t) {
    mask.set(static_cast<tech::TileType>(t));
  }
  return mask;
}

}  // namespace

TracedNet::TracedNet(const tech::Technology& tech) : tech_(&tech) {
  planes_.reserve(tech.plane_count());
  for (std::size_t p = 0; p < tech.plane_count(); ++p) {
    planes_.emplace_back();
  }
}

diag::Status TracedNet::add(const connect::NetTile& piece) {
  if (piece.plane >= planes_.size()) {
    return diag::Done{};
  }
  const db::PaintRow row = db::paint_row(*tech_, piece.plane, piece.tile.type);
  db::Plane& plane = planes_[piece.plane];
  if (piece.tile.split) {
    return plane.paint_triangle(piece.tile.area, piece.tile.diagonal, piece.tile.right_side, row, true);
  }
  return plane.paint(piece.tile.area, row);
}

ContactCuts contact_cuts(db::Coord w, db::Coord h, const std::optional<CutRule>& rule, bool magic_compat) {
  ContactCuts cuts;
  if (!rule) {
    return cuts;  // no cut rule: area and perimeter 0 in both modes
  }
  const std::int64_t scale = rule->scale_factor > 0 ? rule->scale_factor : 1;
  const std::int64_t pitch = std::int64_t{rule->size} + rule->spacing;
  if (pitch != 0) {
    // Q-MISC-4: ON mixes internal (w, h) and output (border, pitch) units.
    const std::int64_t wu = magic_compat ? w : std::int64_t{w} * scale;
    const std::int64_t hu = magic_compat ? h : std::int64_t{h} * scale;
    cuts.nx = std::max<std::int64_t>((wu - 2 * std::int64_t{rule->border}) / pitch, 1);
    cuts.ny = std::max<std::int64_t>((hu - 2 * std::int64_t{rule->border}) / pitch, 1);
  }
  const float f = static_cast<float>(rule->size) / static_cast<float>(scale);
  const std::int64_t count = cuts.nx * cuts.ny;
  cuts.area = magic_compat ? count * static_cast<std::int64_t>(f * f)
                           : static_cast<std::int64_t>(static_cast<double>(count) * static_cast<double>(f) *
                                                       static_cast<double>(f));
  cuts.perimeter = count * (std::int64_t{rule->size} * 4) / scale;
  return cuts;
}

AreaSum tiedown_area(const TracedNet& net, const MeasureSetup& setup) {
  AreaSum sum;
  for (std::size_t p = 0; p < net.plane_count(); ++p) {
    sum = accumulate(net, *setup.tech, static_cast<tech::PlaneId>(p), setup.rules->tiedown_types(), sum);
  }
  return sum;
}

AreaSum gate_area(const TracedNet& net, const MeasureSetup& setup, tech::PlaneId gate_plane) {
  return accumulate(net, *setup.tech, gate_plane, setup.rules->gate_types(), AreaSum{});
}

bool plane_included(const RuleView& rules, tech::PlaneId plane, const CheckPlane& check) {
  if (rules.partial() && plane != check.plane) {
    return false;
  }
  return rules.plane_order(plane) <= check.order;
}

AntennaAreas antenna_areas(const TracedNet& net, const MeasureSetup& setup, const CheckPlane& check) {
  AntennaAreas out;
  out.area.assign(setup.tech->type_count(), 0);
  const tech::TypeMask types = all_but_space(*setup.tech);
  for (std::size_t p = tech::kFirstUserPlane; p < net.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    if (!plane_included(*setup.rules, plane, check)) {
      continue;
    }
    Accumulator acc(net, setup, plane, out);
    (void)net.plane(plane).enumerate(db::whole_plane(), types, [&](const db::TileView& t) {
      acc.add(t);
      return true;
    });
  }
  return out;
}

}  // namespace hefesto::antenna
