// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The BLOCKAGES section of DEF write (capability def-write "BLOCKAGES
// section"; Q-LEFDEF-101 fixed).
//
// Magic 8.3.694 calls defBlockageGeometryFunc(tile, plane, defobsdata)
// through DBSrConnect, which passes (tile, dinfo, plane, clientData): the
// collector reads its data from the plane number and crashes (SIGSEGV) on
// the first tile of any node that is neither a port nor a special net.
// Hefesto collects that geometry as intended and says, in a Hefesto-only
// message, that Magic crashes on the input.
//
// Ported from Magic 8.3.694 (fc5daa9e) lef/defWrite.c defWriteBlockages,
// defblockageVisit, defBlockageGeometryFunc and defSimpleBlockageFunc,
// Copyright (C) 1985, 1990 Regents of the University of California (see
// THIRD_PARTY_NOTICES.md).
//
// Owner: DEFW (openspec/changes/m6-lef-def-oasis/design.md D2).

#include <string>
#include <vector>

#include "def/write/internal.hpp"
#include "hefesto/connect/search.hpp"

namespace hefesto::def::write_detail {

namespace {

/// One LEF route or via record's blockage list.
struct Bucket {
  std::string name;
  tech::TypeMask mask;
  std::vector<db::Rect> rects;  // in insertion order; written newest first
};

class Blockages {
 public:
  explicit Blockages(const Context& c) : c_(c) {}

  void add_bucket(const InfoEntry& e) {
    Bucket b;
    const auto type = e.type();
    const bool route = e.layer_class() == lef::LayerClass::route;
    b.name = route ? e.canonical_name() : c_.map.name(type.value_or(tech::kSpace)).value_or(std::string{});
    if (type) {
      b.mask.set(*type);
    }
    if (const auto obs = e.obstruction_type()) {
      b.mask.set(*obs);
      if (!c_.active(*obs)) {
        extra_.set(*obs);
      }
    }
    buckets_.push_back(std::move(b));
  }

  /// The first bucket holding `type`, with the tile's rectangle.
  void add(tech::TileType type, const db::Rect& area) {
    for (Bucket& b : buckets_) {
      if (b.mask.test(type)) {
        b.rects.push_back(area);
        return;
      }
    }
  }

  /// A tile reached on `plane`: a stacked contact counts as its residue
  /// homed there.
  void add_tile(const db::TileView& tile, tech::PlaneId plane) {
    tech::TileType type = tile.type;
    if (type == tech::kSpace) {
      return;
    }
    if (const tech::ContactInfo* info = c_.tech.contact(type); info != nullptr && info->stacked) {
      type = homed_residue(*info, plane);
      if (type == tech::kSpace) {
        return;
      }
    }
    add(type, tile.area);
  }

  /// The first residue (type order) of a stacked type homed on `plane`;
  /// space when none is.
  [[nodiscard]] tech::TileType homed_residue(const tech::ContactInfo& info, tech::PlaneId plane) const {
    for (std::size_t t = tech::kFirstUserType; t < c_.tech.user_type_end(); ++t) {
      const auto r = static_cast<tech::TileType>(t);
      if (info.residues.test(r) && c_.tech.home_plane(r) == plane) {
        return r;
      }
    }
    return tech::kSpace;
  }

  [[nodiscard]] bool empty() const noexcept { return buckets_.empty(); }
  [[nodiscard]] const tech::TypeMask& extra() const noexcept { return extra_; }
  [[nodiscard]] const std::vector<Bucket>& buckets() const noexcept { return buckets_; }

 private:
  const Context& c_;
  std::vector<Bucket> buckets_;
  tech::TypeMask extra_;  // ExtraObsLayersMask
};

class Collector final : public connect::NetVisitor {
 public:
  explicit Collector(Blockages& b) : b_(b) {}
  bool on_tile(const connect::NetTile& t) override {
    reached_ = true;
    b_.add_tile(t.tile, t.plane);
    return true;
  }
  [[nodiscard]] bool reached() const noexcept { return reached_; }

 private:
  Blockages& b_;
  bool reached_ = false;
};

// defblockageVisit for every node that is neither a port nor special.
void node_geometry(const Context& c, const extflat::Netlist& netlist, const NetPlan& plan, Blockages& b) {
  bool warned = false;
  for (std::size_t i = 0; i < netlist.nodes().size(); ++i) {
    const extflat::FlatNode& node = netlist.nodes()[i];
    if (node.port || plan.special[i] || plan.skip[i]) {
      continue;
    }
    const auto type = node_type(c, node.layer);
    if (!type) {
      continue;
    }
    Collector collect(b);
    connect::SearchOptions options;
    options.magic_compat = c.compat;
    const db::Rect at{node.location.x, node.location.y, node.location.x + 1, node.location.y + 1};
    (void)connect::search_flat(c.cell, c.tech, connect::SearchStart{at, c.tech.connects_to(*type)}, options,
                               collect);
    if (collect.reached() && !warned) {
      warned = true;
      c.say_native(kMagicCrash, "Net " + def_name(name_components(node), '/') +
                                    " is neither a port nor a special net; Magic 8.3.694 crashes collecting its "
                                    "blockage geometry (Q-LEFDEF-101). It is written under BLOCKAGES.");
    }
  }
}

// defSimpleBlockageFunc over every plane: the non-electrical obstruction
// paint.
void obstruction_geometry(const Context& c, Blockages& b) {
  if (b.extra().empty()) {
    return;
  }
  for (std::size_t p = tech::kFirstUserPlane; p < c.cell.plane_count(); ++p) {
    const db::Plane& plane = c.cell.plane(static_cast<tech::PlaneId>(p));
    (void)plane.enumerate(db::whole_plane(), b.extra(), [&](const db::TileView& t) {
      if (t.type != tech::kSpace) {
        b.add(t.type, t.area);
      }
      return true;
    });
  }
}

}  // namespace

void write_blockages(const Context& c, const std::vector<InfoEntry>& order, const extflat::Netlist* netlist,
                     const NetPlan& plan, std::string& out) {
  Blockages b(c);
  for (const InfoEntry& e : order) {
    const lef::LayerClass cls = e.layer_class();
    if (e.live() && (cls == lef::LayerClass::route || cls == lef::LayerClass::cut)) {
      b.add_bucket(e);
    }
  }
  if (netlist != nullptr && !b.empty()) {
    node_geometry(c, *netlist, plan, b);
  }
  obstruction_geometry(c, b);
  std::size_t nonempty = 0;
  for (const Bucket& bucket : b.buckets()) {
    nonempty += bucket.rects.empty() ? 0U : 1U;
  }
  if (nonempty == 0) {
    return;
  }
  out += "BLOCKAGES " + std::to_string(nonempty) + " ;\n";
  for (const Bucket& bucket : b.buckets()) {
    if (bucket.rects.empty()) {
      continue;
    }
    out += "   - LAYER " + bucket.name;
    for (auto it = bucket.rects.rbegin(); it != bucket.rects.rend(); ++it) {
      out += "\n      RECT ( " + g10(scaled(it->x0, c.scale)) + " " + g10(scaled(it->y0, c.scale)) + " ) ( " +
             g10(scaled(it->x1, c.scale)) + " " + g10(scaled(it->y1, c.scale)) + " )";
    }
    out += " ;\n";
  }
  out += "END BLOCKAGES\n\n";
}

}  // namespace hefesto::def::write_detail
