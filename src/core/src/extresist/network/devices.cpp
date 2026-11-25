// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The per-tile records after fracturing (capability extresist-network
// "Device annotation", "Net geometry collection" steps 6-7): device
// records and their terminal regions (ResAddDevPlumbing,
// ResAddTerminalPlumbing, ResUnmarkTerminal), a record on every other tile
// (ResAddPlumbing), the ports (ResMakeDriverSinkPorts, ResMakeLabelPorts),
// the contact tiles (ResFindNewContactTiles) and the device dimensions and
// other-plane terminals (ResPreProcessDevices).
// (Magic ref: resis/ResUtils.c:102-1086, resis/ResMain.c:262-700)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "extresist/network/builder.hpp"
#include "hefesto/db/plane.hpp"

namespace hefesto::extresist::detail {

namespace {

tech::TypeMask non_space_user_types() {
  tech::TypeMask m;
  for (tech::TileType t = tech::kFirstUserType; t < tech::kMaxTileTypes; ++t) {
    m.set(t);
  }
  return m;
}

/// The facing type of each side's neighbours, in Magic's order of sides.
struct SideWalk {
  bool (Builder::*walk)(const TileRef&, const std::function<bool(const TileRef&)>&);
  tech::TileType (*facing)(const TileTypes&);
  std::uint32_t edge;
};

tech::TileType bottom_of(const TileTypes& t) {
  return t.bottom();
}
tech::TileType top_of(const TileTypes& t) {
  return t.top();
}
tech::TileType left_of(const TileTypes& t) {
  return t.left;
}
tech::TileType right_of(const TileTypes& t) {
  return t.split ? t.right : t.left;
}

constexpr SideWalk kSides[] = {
    {&Builder::each_top, &bottom_of, kTopEdge},
    {&Builder::each_bottom, &top_of, kBottomEdge},
    {&Builder::each_right, &left_of, kRightEdge},
    {&Builder::each_left, &right_of, kLeftEdge},
};

/// ResAddDevPlumbing's walk over one device.
class DeviceWalk {
 public:
  DeviceWalk(Builder& b, const DevTile& dev, Id device, Id first_info)
      : b_(b), dev_(dev), device_(device), first_(first_info) {
    nterms_ = dev.record->terminal_count + 2;
  }

  void run(const TileRef& tile) {
    std::vector<TileRef> stack{tile};
    while (!stack.empty()) {
      const TileRef tp1 = stack.back();
      stack.pop_back();
      const TileTypes ty = b_.types(tp1);
      tech::TileType t1 = ty.left;
      if (ty.split) {
        t1 = conn(dev_.type).test(ty.right) ? ty.right : ty.left;
      }
      const Id re0 = b_.info(tp1);
      for (const SideWalk& s : kSides) {
        (b_.*s.walk)(tp1, [&](const TileRef& tp2) {
          visit(tp2, s.facing(b_.types(tp2)), t1, re0, s.edge, stack);
          return false;
        });
      }
    }
    finish(tile);
  }

 private:
  [[nodiscard]] tech::TypeMask conn(tech::TileType t) const {
    return t < b_.style_.device_conn.size() ? b_.style_.device_conn[t] : tech::TypeMask{};
  }

  void visit(const TileRef& tp2, tech::TileType facing, tech::TileType t1, Id re0, std::uint32_t edge,
             std::vector<TileRef>& stack) {
    if (conn(t1).test(facing) && b_.info(tp2) == kNil) {
      stack.push_back(tp2);
      const Id info = b_.add_field(tp2);
      b_.g.tiles[info].device = device_;
      b_.g.tiles[info].status |= kTileDev;
      Device& d = b_.g.devices[device_];
      const db::Rect r = b_.area(tp2);
      if (r.x0 < d.inside.x0 || (r.x0 == d.inside.x0 && r.y0 < d.inside.y0)) {
        d.inside = r;
      }
      return;
    }
    // Magic tests the bottom type of the neighbour on every side here.
    const tech::TileType bottom = b_.types(tp2).bottom();
    if (source_term_ < 0) {
      first_source(tp2, bottom);
    }
    if (source_term_ >= 0 && Builder::sd_types(*dev_.record, source_term_).test(bottom)) {
      if (b_.info(tp2) == kNil && source_term_ == nterms_ - 3) {
        terminal_plumbing(tp2, source_term_);
      }
      const Id info = b_.add_field(tp2);
      if ((b_.g.tiles[info].status & kTileSD) != 0) {
        b_.g.tiles[re0].source_edge |= edge;
      }
    }
  }

  /// The first S/D class holding `bottom` makes `tp2` the source tile.
  void first_source(const TileRef& tp2, tech::TileType bottom) {
    for (std::int32_t i = 0; i < nterms_ - 2; ++i) {
      if (Builder::sd_types(*dev_.record, i).test(bottom)) {
        source_tile_ = tp2;
        source_term_ = i;
        terminal_plumbing(tp2, i);
        return;
      }
    }
  }

  /// ResAddTerminalPlumbing: flags the S/D region of class `term`.
  void terminal_plumbing(const TileRef& tile, std::int32_t term) {
    const tech::TypeMask sd = Builder::sd_types(*dev_.record, term);
    std::vector<TileRef> stack{tile};
    while (!stack.empty()) {
      const TileRef tp1 = stack.back();
      stack.pop_back();
      for (const SideWalk& s : kSides) {
        (b_.*s.walk)(tp1, [&](const TileRef& tp2) {
          if (sd.test(s.facing(b_.types(tp2)))) {
            const Id re = b_.add_field(tp2);
            if ((b_.g.tiles[re].status & kTileSD) == 0) {
              b_.g.tiles[re].status |= kTileSD;
              stack.push_back(tp2);
            }
          }
          return false;
        });
      }
    }
  }

  /// ResUnmarkTerminal: clears the S/D flags of the source region again.
  void unmark(const TileRef& tile, std::int32_t term) {
    const tech::TypeMask sd = Builder::sd_types(*dev_.record, term);
    const Id first = b_.info(tile);
    if (first == kNil) {
      return;
    }
    b_.g.tiles[first].status &= ~kTileSD;
    std::vector<TileRef> stack{tile};
    while (!stack.empty()) {
      const TileRef tp1 = stack.back();
      stack.pop_back();
      for (const SideWalk& s : kSides) {
        (b_.*s.walk)(tp1, [&](const TileRef& tp2) {
          const Id re2 = b_.info(tp2);
          if (re2 != kNil && (b_.g.tiles[re2].status & kTileSD) != 0 && sd.test(s.facing(b_.types(tp2)))) {
            b_.g.tiles[re2].status &= ~kTileSD;
            stack.push_back(tp2);
          }
          return false;
        });
      }
    }
  }

  void finish(const TileRef& tile) {
    const db::Rect r = b_.area(tile);
    if (!source_tile_) {
      for (const tech::PlaneId p : b_.user_planes()) {
        for (std::int32_t i = 0; i < nterms_ - 2; ++i) {
          const tech::TypeMask sd = Builder::sd_types(*dev_.record, i);
          if (!b_.tech_.plane_types(p).intersects(sd)) {
            continue;
          }
          b_.search(p, r, sd, [&](const TileRef& t, bool) {  // resMultiPlaneTerm
            b_.g.tiles[b_.add_field(t)].status |= kTileSD;
            b_.g.tiles[first_].source_edge |= kOtherPlane;
            return false;
          });
        }
      }
    }
    tech::TypeMask subs = dev_.record->substrate_types;
    subs.clear_type(tech::kSpace);
    for (const tech::PlaneId p : b_.user_planes()) {
      if (b_.tech_.plane_types(p).intersects(subs)) {
        b_.search(p, r, subs, [&](const TileRef& t, bool) {  // resSubstrateTerm
          b_.g.tiles[b_.add_field(t)].status |= kTileSubs;
          return false;
        });
      }
    }
    if (source_tile_) {
      unmark(source_tile_, source_term_);
    }
  }

  Builder& b_;
  const DevTile& dev_;
  Id device_;
  Id first_;
  std::int32_t nterms_ = 2;
  std::int32_t source_term_ = -1;
  TileRef source_tile_;
};

}  // namespace

void Builder::add_device_plumbing(const DevTile& dev) {
  const auto plane = tech_.home_plane(dev.type);
  if (!plane || dev.record == nullptr) {
    return;
  }
  const TileRef tile = goto_point(*plane, db::Point{dev.area.x0, dev.area.y0});
  const TileTypes ty = types(tile);
  if (!tile || (!ty.split && ty.left == tech::kSpace)) {
    g.message("Bad device location at (" + std::to_string(dev.area.x0) + " " + std::to_string(dev.area.y0) +
              ").  There is nothing here.");
    return;
  }
  tech::TileType loctype = ty.left;
  if (ty.split) {
    const tech::TypeMask conn = dev.type < style_.device_conn.size() ? style_.device_conn[dev.type] : tech::TypeMask{};
    loctype = conn.test(ty.right) ? ty.right : ty.left;
  }
  Device d;
  d.terms.assign(static_cast<std::size_t>(dev.record->terminal_count + 2), kNil);
  d.inside = area(tile);
  d.devtype = loctype;
  g.devices.push_back(std::move(d));
  const Id device = static_cast<Id>(g.devices.size() - 1);
  const Id first = add_field(tile);
  g.tiles[first].device = device;
  g.tiles[first].status |= kTileDev;
  DeviceWalk walk(*this, dev, device, first);
  walk.run(tile);
}

void Builder::add_plumbing() {
  const tech::TypeMask mask = non_space_user_types();
  for (const tech::PlaneId p : user_planes()) {
    search(p, db::whole_plane(), mask, [&](const TileRef& t, bool) {
      add_field(t);
      return false;
    });
  }
}

namespace {

/// The plane and mask a port or label searches: the uppermost residue's
/// plane for a contact (it has been dissolved).
std::pair<std::optional<tech::PlaneId>, tech::TypeMask> port_plane(const tech::Technology& tech, tech::TileType type) {
  if (!tech.is_contact(type)) {
    return {tech.home_plane(type), tech::TypeMask::of(type)};
  }
  const tech::TypeMask mask = tech.full_residues(type);
  for (std::size_t t = tech.user_type_end(); t-- > tech::kFirstUserType;) {
    if (mask.test(static_cast<tech::TileType>(t))) {
      return {tech.home_plane(static_cast<tech::TileType>(t)), mask};
    }
  }
  return {std::nullopt, mask};
}

}  // namespace

void Builder::make_ports() {
  for (std::size_t i = 0; i < input.ports.size(); ++i) {
    const NetPort& port = input.ports[i];
    if (port.type <= tech::kSpace) {
      g.message("Warning:  Label \"" + port.name + "\" is unconnected.");
      continue;
    }
    db::Rect rect = port.rect;
    if (port.sink && rect.x0 == rect.x1) {
      --rect.x0;
      ++rect.x1;
    }
    if (port.sink && rect.y0 == rect.y1) {
      --rect.y0;
      ++rect.y1;
    }
    const auto [plane, mask] = port_plane(tech_, port.type);
    if (!plane || *plane >= def.plane_count() || editors_[*plane] == nullptr) {
      continue;
    }
    search(*plane, rect, mask, [&](const TileRef& t, bool side) {  // ResAddPortFunc
      const Id ti = info(t);
      const TileTypes ty = types(t);
      if (ti == kNil || (ty.split && ty.left != tech::kSpace && ty.right != tech::kSpace && side)) {
        return false;
      }
      auto& ports = g.tiles[ti].ports;
      ports.insert(ports.begin(), Port{port.name, db::Point{rect.x0, rect.y0}, i});
      return false;
    });
  }
}

void Builder::make_label_ports() {
  for (const db::Label& lab : geo.labels) {
    if (lab.text.empty()) {
      continue;
    }
    if (input.has_drive_point && input.has_drive_point(lab.text, lab.rect)) {
      break;  // Magic leaves the whole label loop here
    }
    added_sinks.insert(added_sinks.begin(), NetPort{lab.text, lab.rect, lab.type, true});
    if (db::encloses(lab.rect, input.driver.location)) {
      start_type = lab.type;
    }
    const auto [plane, mask] = port_plane(tech_, lab.type);
    if (!plane || *plane >= def.plane_count() || editors_[*plane] == nullptr) {
      continue;
    }
    search(*plane, lab.rect, mask, [&](const TileRef& t, bool side) {  // ResAddLabelFunc
      const Id ti = info(t);
      const TileTypes ty = types(t);
      if (ti == kNil || (ty.split && ty.left != tech::kSpace && ty.right != tech::kSpace && side)) {
        return false;
      }
      auto& ports = g.tiles[ti].ports;
      ports.insert(ports.begin(), Port{lab.text, db::Point{lab.rect.x0, lab.rect.y0}, input.ports.size()});
      return false;
    });
  }
}

void Builder::find_new_contact_tiles() {
  tech::TileType last = tech::kSpace;
  tech::TypeMask mask;
  const std::optional<tech::PlaneId> subs_plane = style_.substrate.plane;
  for (std::size_t ci = 0; ci < g.contacts.size(); ++ci) {
    Contact& c = g.contacts[ci];
    if (c.type != last) {
      last = c.type;
      mask = tech_.full_residues(c.type);
      if (subs_plane) {
        mask |= tech_.connects_to(c.type) & tech_.plane_types(*subs_plane);
      }
    }
    const tech::PlaneMask pmask = tech_.type_planes(c.type);
    for (const tech::PlaneId p : user_planes()) {
      if ((pmask & tech::plane_bit(p)) == 0 && (!subs_plane || p != *subs_plane)) {
        continue;
      }
      const TileRef tile = goto_point(p, c.center);
      const TileTypes ty = types(tile);
      bool hit = (ty.split && mask.test(ty.right)) || mask.test(ty.left);
      if (!hit && !ty.split && tech_.is_contact(ty.left)) {
        hit = tech_.contact(ty.left)->residues.intersects(mask);
      }
      const Id ti = info(tile);
      // At most four tiles per contact (LAYERS_PER_CONTACT; Magic writes past
      // the array beyond that).
      if (!hit || ti == kNil || c.current >= 4) {
        continue;
      }
      c.tile[static_cast<std::size_t>(c.current)] = ti;
      ++c.current;
      g.push(g.tiles[ti].contacts, static_cast<Id>(ci));
    }
  }
}

namespace {

/// The planes holding any type of `mask` (DBTechTypesToPlanes).
tech::PlaneMask planes_of(const tech::Technology& tech, const tech::TypeMask& mask) {
  tech::PlaneMask m = 0;
  for (tech::TileType t = 0; t < tech::kMaxTileTypes; ++t) {
    if (mask.test(t)) {
      m |= tech.type_planes(t);
    }
  }
  return m;
}

}  // namespace

void Builder::preprocess_devices() {
  for (const DevTile& dt : dev_tiles) {
    std::optional<tech::PlaneId> plane = tech_.home_plane(dt.type);
    if (tech_.is_contact(dt.type)) {
      const tech::TypeMask res = tech_.full_residues(dt.type);
      for (tech::TileType r = tech::kFirstUserType; r < tech_.user_type_end(); ++r) {
        if (res.test(r) && style_.device_types.test(r)) {
          plane = tech_.home_plane(r);
          break;
        }
      }
    }
    if (!plane || *plane >= def.plane_count() || editors_[*plane] == nullptr) {
      continue;
    }
    const db::Point ll{dt.area.x0, dt.area.y0};
    const TileRef tile = goto_point(*plane, ll);
    const TileTypes ty = types(tile);
    tech::TileType tt = ty.left;
    bool isdev = style_.device_types.test(tt);
    if (ty.split && !isdev) {
      tt = ty.right;
      isdev = style_.device_types.test(tt);
    }
    const Id ti = info(tile);
    if (ti == kNil || g.tiles[ti].device == kNil || !isdev) {
      g.message("Bad Device Location at " + std::to_string(ll.x) + "," + std::to_string(ll.y));
      continue;
    }
    if ((g.tiles[ti].status & kTileMark) != 0) {
      continue;
    }
    g.tiles[ti].status |= kTileMark;
    Device& rd = g.devices[g.tiles[ti].device];
    rd.perim += dt.perim;
    rd.length += dt.overlap;
    rd.area += (dt.area.x1 - dt.area.x0) * (dt.area.y1 - dt.area.y0);
    rd.tiles++;
    const auto record_terms = [&](tech::TypeMask mask, std::int32_t term) {
      mask.clear_type(tech::kSpace);
      const tech::PlaneMask pmask = planes_of(tech_, mask);
      const auto home = tech_.home_plane(tt);
      if (pmask == 0 || (home && (pmask & tech::plane_bit(*home)) != 0)) {
        return;
      }
      for (const tech::PlaneId p : user_planes()) {
        if ((pmask & tech::plane_bit(p)) == 0) {
          continue;
        }
        const TileRef tp = goto_point(p, ll);
        if (tp && mask.test(types(tp).left)) {
          auto& list = dev_nodes[tp.tile.tile];
          list.insert(list.begin(), DevTerm{term, tile});
        }
      }
    };
    record_terms(dt.record->substrate_types, -1);
    for (std::int32_t i = 0; i < dt.record->terminal_count; ++i) {
      record_terms(sd_types(*dt.record, i), i);
    }
  }
  for (Device& d : g.devices) {
    const std::int32_t width = d.perim;
    const std::int32_t length = d.length;
    if (d.tiles == 0) {
      continue;
    }
    if (length != 0) {
      d.length = static_cast<std::int32_t>(static_cast<float>(length) / static_cast<float>(d.tiles << 1));
      d.width = (width - length) >> 1;
    } else {
      const double perimeter = d.perim / 4.0;
      const double w = perimeter + std::sqrt(perimeter * perimeter - static_cast<double>(d.area));
      // A NaN (the area exceeds the square of a quarter perimeter) is
      // undefined in Magic's int conversion; it gives 0 here.
      d.width = std::isfinite(w) ? static_cast<std::int32_t>(w) : 0;
      d.length = (d.perim - 2 * d.width) >> 1;
    }
  }
}

}  // namespace hefesto::extresist::detail
