// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The Builder's scratch-plane accessors, the run order of ResExtractNet
// and the export (ResGetDevice for the input devices and the driver).
// (Magic ref: resis/ResMain.c:1243-1580, :2066-2110)
//
// Owner: RESNET (openspec/changes/m7-extraction design D3).

#include <string>
#include <unordered_map>
#include <utility>

#include "extresist/network/builder.hpp"
#include "hefesto/db/plane.hpp"

namespace hefesto::extresist::detail {

Builder::Builder(const NetGeometry& geometry, const extract::ExtStyle& style, const DeviceAnnotations& devices,
                 const BuildInput& build)
    : geo(geometry), style_(style), in(devices), input(build), tech_(*build.technology), def(*geometry.scratch) {
  g.geometry = build.geometry;
  g.magic_compat = build.magic_compat;
  g.port_nodes = &build.port_nodes;
  start_type = build.driver.type;
  start = build.start;
}

std::vector<tech::PlaneId> Builder::user_planes() const {
  std::vector<tech::PlaneId> planes;
  for (std::size_t p = tech::kFirstUserPlane; p < def.plane_count(); ++p) {
    planes.push_back(static_cast<tech::PlaneId>(p));
  }
  return planes;
}

void Builder::open_editors() {
  editors_.clear();
  editors_.resize(def.plane_count());
  for (const tech::PlaneId p : user_planes()) {
    editors_[p] = std::make_unique<db::StitchEditor>(def.plane(p));
  }
}

TileTypes Builder::types(const TileRef& t) {
  const db::TileView v = ed(t.plane).view(t.tile);
  if (v.split) {
    return TileTypes{true, v.diagonal == db::Diagonal::nw_se ? 1 : 0, v.left_type, v.right_type};
  }
  return TileTypes{false, 0, v.type, v.type};
}

TileRef Builder::goto_point(tech::PlaneId plane, db::Point p) {
  db::StitchEditor& e = ed(plane);
  const db::StitchTile t = e.tile_at(p, e.hint());
  if (t) {
    e.set_hint(t);
  }
  return TileRef{plane, t};
}

Id Builder::info(const TileRef& t) {
  if (!t) {
    return kNil;
  }
  const std::uintptr_t c = ed(t.plane).client(t.tile);
  return c == db::kClientDefault ? kNil : static_cast<Id>(c);
}

Id Builder::add_field(const TileRef& t) {
  const Id existing = info(t);
  if (existing != kNil) {
    return existing;
  }
  TileInfo ti;
  ti.plane = t.plane;
  ti.tile = t.tile;
  g.tiles.push_back(std::move(ti));
  const Id id = static_cast<Id>(g.tiles.size() - 1);
  ed(t.plane).set_client(t.tile, id);
  return id;
}

void Builder::search(tech::PlaneId plane, const db::Rect& area, const tech::TypeMask& mask,
                     const std::function<bool(const TileRef&, bool)>& visit) {
  def.plane(plane).enumerate(area, mask, [&](const db::TileView& v) {
    return !visit(TileRef{plane, db::StitchEditor::handle(v)}, v.split && v.right_side);
  });
}

bool Builder::each_top(const TileRef& t, const std::function<bool(const TileRef&)>& visit) {
  const db::Rect r = area(t);
  for (TileRef tp = stitch(t, db::Stitch::rt); tp && area(tp).x1 > r.x0; tp = stitch(tp, db::Stitch::bl)) {
    if (visit(tp)) {
      return true;
    }
  }
  return false;
}

bool Builder::each_bottom(const TileRef& t, const std::function<bool(const TileRef&)>& visit) {
  const db::Rect r = area(t);
  for (TileRef tp = stitch(t, db::Stitch::lb); tp && area(tp).x0 < r.x1; tp = stitch(tp, db::Stitch::tr)) {
    if (visit(tp)) {
      return true;
    }
  }
  return false;
}

bool Builder::each_right(const TileRef& t, const std::function<bool(const TileRef&)>& visit) {
  const db::Rect r = area(t);
  for (TileRef tp = stitch(t, db::Stitch::tr); tp && area(tp).y1 > r.y0; tp = stitch(tp, db::Stitch::lb)) {
    if (visit(tp)) {
      return true;
    }
  }
  return false;
}

bool Builder::each_left(const TileRef& t, const std::function<bool(const TileRef&)>& visit) {
  const db::Rect r = area(t);
  for (TileRef tp = stitch(t, db::Stitch::bl); tp && area(tp).y0 < r.y1; tp = stitch(tp, db::Stitch::rt)) {
    if (visit(tp)) {
      return true;
    }
  }
  return false;
}

tech::TypeMask Builder::sd_types(const extract::DevicePattern& d, std::int32_t i) {
  // exts_deviceSDTypes[i]; the array ends with an empty mask, so a class
  // past the listed ones has no types (Magic would read past the array).
  return i >= 0 && static_cast<std::size_t>(i) < d.terminals.size() ? d.terminals[static_cast<std::size_t>(i)]
                                                                    : tech::TypeMask{};
}

bool Builder::run() {
  open_editors();
  copy_devices();
  bbox = db::Rect{};
  bool any = false;
  for (const tech::PlaneId p : user_planes()) {
    const db::Rect b = def.plane(p).bounding_box();
    if (b.is_null()) {
      continue;
    }
    bbox = any ? db::Rect{std::min(bbox.x0, b.x0), std::min(bbox.y0, b.y0), std::max(bbox.x1, b.x1),
                          std::max(bbox.y1, b.y1)}
               : b;
    any = true;
  }
  shave_contacts();
  find_contacts();
  dissolve_contacts();
  if (!fracture_planes()) {
    return false;
  }
  for (const DevTile& dev : dev_tiles) {
    add_device_plumbing(dev);
  }
  add_plumbing();
  make_ports();
  make_label_ports();
  find_new_contact_tiles();
  preprocess_devices();
  return process_tiles();
}

namespace {

/// The done list, then the pending queue: the order of Network::nodes.
std::unordered_map<Id, std::size_t> node_indices(const Graph& g) {
  std::unordered_map<Id, std::size_t> index;
  for (const Id head : {g.done, g.pending}) {
    for (Id n = head; n != kNil; n = g.nodes[n].more) {
      index.emplace(n, index.size());
    }
  }
  return index;
}

}  // namespace

Id Builder::device_at(db::Point p, tech::TileType type) {
  const auto plane = tech_.home_plane(type);
  if (!has_editor(plane)) {
    return kNil;
  }
  const TileRef t = goto_point(*plane, p);
  const TileTypes ty = types(t);
  const bool device = style_.device_types.test(ty.left) || (ty.split && style_.device_types.test(ty.right));
  const Id ti = info(t);
  return device && ti != kNil ? g.tiles[ti].device : kNil;
}

Network Builder::finish() {
  Network out = g.to_network();
  const auto dev_index = [&](Id d) -> std::optional<std::size_t> {
    if (d == kNil) {
      return std::nullopt;
    }
    return g.devices.size() - 1 - d;
  };
  for (const NetDevice& d : in.devices) {
    out.input_devices.push_back(dev_index(device_at(d.location, d.type)));
  }
  const Id driver = device_at(input.driver.location, start_type);
  if (driver != kNil && g.devices[driver].terms.size() > 2 && g.devices[driver].terms[2] != kNil) {
    const auto index = node_indices(g);
    const auto it = index.find(g.devices[driver].terms[2]);
    if (it != index.end()) {
      out.driver_source = it->second;
    }
  }
  out.node_cap = input.node_cap;
  out.added_sinks = added_sinks;
  return out;
}

}  // namespace hefesto::extresist::detail
