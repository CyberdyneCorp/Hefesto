// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Pass 1 connections (extract-hierarchy "Interaction processing and
// connections (pass 1)", "Name unification order", "Sticky labels across
// the hierarchy"). Ported from Magic 8.3.694 (fc5daa9e) extract/ExtHier.c
// (extHierConnections, extHierConnectFunc1, extHierConnectFunc2,
// extHierConnectFunc3); Magic is Copyright (C) 1985, 1990 Regents of the
// University of California (permissive license, THIRD_PARTY_NOTICES.md).
//
// Not ported: extTestNMInteract's triangle test of two split tiles (split
// tiles connect when their bounding rectangles touch) and the `connect`
// port records of full R-C extraction (EXT_DOEXTRESIST).
//
// Owner: EXTHIER.

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "hier_context.hpp"
#include "hier_geometry.hpp"

namespace hefesto::extract::hier {

namespace {

constexpr const char* kNone = "(none)";

const tech::TypeMask& paint_types() {
  static const tech::TypeMask mask = ~tech::TypeMask::of(tech::kSpace);
  return mask;
}

const tech::TypeMask& node_conn(const Env& env, tech::TileType t) {
  static const tech::TypeMask none;
  return t < env.style->node_conn.size() ? env.style->node_conn[t] : none;
}

db::Rect overlap_of(const db::Rect& a, const db::Rect& b) {
  return db::Rect{std::max(a.x0, b.x0), std::max(a.y0, b.y0), std::min(a.x1, b.x1), std::min(a.y1, b.y1)};
}

bool positive(const db::Rect& r) {
  return r.x1 > r.x0 && r.y1 > r.y0;
}

void illegal_overlap(Hier& h, tech::TileType a, tech::TileType b, const db::Rect& r) {
  ++h.result->errors;
  feedback(h, r,
           "Illegal overlap between " + std::string(h.env.tech->type_name(a)) + " and " +
               std::string(h.env.tech->type_name(b)) + " (types do not connect)",
           true);
}

// A sticky label of the cumulative buffer (extHierConnectFunc1's label loop).
struct StickyLabel {
  db::Rect rect;
  tech::TileType type = tech::kSpace;
  std::string text;
};

struct Pass {
  Hier* h;
  FlatTree* cum;
  FlatTree* one;
  // The cumulative buffer's sticky labels in list order, so that a tile
  // does not scan (and copy) every label of the buffer. The pass changes a
  // buffer's labels only by inserting hard-way labels, so the snapshot is
  // rebuilt whenever the buffer's label count is not the one it was taken at.
  std::vector<StickyLabel> sticky;
  std::optional<std::size_t> sticky_seen;
};

const std::vector<StickyLabel>& cum_sticky_labels(Pass& ps) {
  const std::vector<db::Label>& labels = ps.cum->def->labels();
  if (ps.sticky_seen == labels.size()) {
    return ps.sticky;
  }
  ps.sticky.clear();
  for (const db::Label& lab : labels) {
    if (lab.sticky) {
      ps.sticky.push_back(StickyLabel{lab.rect, lab.type, lab.text});
    }
  }
  ps.sticky_seen = labels.size();
  return ps.sticky;
}

// extHierConnectFunc2: `cum` (plane `pnum` of the cumulative buffer)
// against the subtree tile `o`.
void connect_pair(Pass& ps, const TileRef& o, const TileRef& c) {
  Hier& h = *ps.h;
  const db::Rect r = overlap_of(o.tile.area, c.tile.area);
  if (r.x1 < r.x0 || r.y1 < r.y0 || (r.x1 == r.x0 && r.y1 == r.y0)) {
    return;
  }
  const tech::TileType otype = side_type(o.tile);
  const tech::TileType ctype = side_type(c.tile);
  if (!node_conn(h.env, otype).test(ctype)) {
    if (positive(r)) {
      illegal_overlap(h, otype, ctype, r);
    }
    return;
  }
  const std::string name1 = *tile_to_node(h, c, *ps.cum, true);
  if (name1 == kNone) {
    return;
  }
  const std::size_t node1 = h.table.node_of(name1);
  const std::string name2 = *tile_to_node(h, o, *ps.one, true);
  if (name2 == kNone) {
    return;
  }
  const std::size_t node2 = h.table.node_of(name2);
  (void)h.table.unify(node1, node2, !h.env.compat->magic_compat);
}

// The sticky labels of the cumulative buffer touching `r` (extHierConnectFunc1's
// label loop); false when a "(none)" name ended it.
void sticky_cum_labels(Pass& ps, const TileRef& o, const db::Rect& r, const tech::TypeMask& connected) {
  Hier& h = *ps.h;
  // The names below are looked up in `one`, so the snapshot of the
  // cumulative buffer's labels stays valid through the loop.
  for (const StickyLabel& lab : cum_sticky_labels(ps)) {
    if (!db::touch(r, lab.rect) || !connected.test(lab.type)) {
      continue;
    }
    const std::size_t node1 = h.table.node_of(lab.text);
    const std::string name = *tile_to_node(h, o, *ps.one, true);
    if (name == kNone) {
      return;
    }
    const std::size_t node2 = h.table.node_of(name);
    (void)h.table.unify(node1, node2, !h.env.compat->magic_compat);
  }
}

// extHierConnectFunc1.
void connect_tile(Pass& ps, const TileRef& o) {
  Hier& h = *ps.h;
  const tech::TypeMask& connected = node_conn(h.env, side_type(o.tile));
  const db::Rect r = db::expand(clip(o.tile.area, h.sub_area), 1);
  for (std::size_t p = tech::kFirstUserPlane; p < ps.cum->def->plane_count(); ++p) {
    const auto pnum = static_cast<tech::PlaneId>(p);
    if ((connected & h.env.tech->plane_types(pnum)).empty()) {
      continue;
    }
    const tech::TypeMask& types = pnum == o.plane ? h.env.style->active : connected;
    const db::Plane& plane = ps.cum->def->plane(pnum);
    const auto visit = [&](const db::TileView& v) {
      connect_pair(ps, o, TileRef{pnum, v});
      return true;
    };
    if (o.tile.split) {
      (void)plane.enumerate_triangle(r, o.tile.diagonal, o.tile.right_side, types, visit);
    } else {
      (void)plane.enumerate(r, types, visit);
    }
  }
  if (h.env.options->labelcheck) {
    sticky_cum_labels(ps, o, r, connected);
  }
}

// extHierConnectFunc3: a sticky subtree label against a cumulative tile.
void connect_label(Pass& ps, const db::Label& lab, const TileRef& c) {
  Hier& h = *ps.h;
  const db::Rect r = overlap_of(lab.rect, c.tile.area);
  if (r.x1 < r.x0 || r.y1 < r.y0) {
    return;
  }
  const tech::TileType ctype = side_type(c.tile);
  if (!node_conn(h.env, lab.type).test(ctype)) {
    if (positive(r)) {
      illegal_overlap(h, lab.type, ctype, r);
    }
    return;
  }
  const std::string name1 = *tile_to_node(h, c, *ps.cum, true);
  if (name1 == kNone) {
    return;
  }
  const std::size_t node1 = h.table.node_of(name1);
  const std::size_t node2 = h.table.node_of(lab.text);
  (void)h.table.unify(node1, node2, !h.env.compat->magic_compat);
}

void sticky_subtree_labels(Pass& ps) {
  Hier& h = *ps.h;
  const std::vector<db::Label> labels = ps.one->def->labels();
  for (const db::Label& lab : labels) {
    const auto home = h.env.tech->home_plane(lab.type);
    if (!lab.sticky || !home || !db::touch(lab.rect, h.sub_area)) {
      continue;
    }
    const db::Rect r = clip(lab.rect, h.sub_area);
    (void)ps.cum->def->plane(*home).enumerate(r, h.env.tech->connects_to(lab.type), [&](const db::TileView& v) {
      connect_label(ps, lab, TileRef{*home, v});
      return true;
    });
  }
}

}  // namespace

void connect_trees(Hier& h, FlatTree& cum, FlatTree& one) {
  Pass ps{&h, &cum, &one, {}, std::nullopt};
  for (std::size_t p = tech::kFirstUserPlane; p < one.def->plane_count(); ++p) {
    const auto pnum = static_cast<tech::PlaneId>(p);
    std::vector<db::TileView> tiles;
    (void)one.def->plane(pnum).enumerate(h.sub_area, paint_types(), [&](const db::TileView& v) {
      tiles.push_back(v);
      return true;
    });
    for (const db::TileView& v : tiles) {
      connect_tile(ps, TileRef{pnum, v});
    }
  }
  if (h.env.options->labelcheck) {
    sticky_subtree_labels(ps);
  }
}

void unify_names(Hier& h, const std::string& cumulative, const std::string& added) {
  const std::size_t node1 = h.table.node_of(cumulative);
  const std::size_t node2 = h.table.node_of(added);
  (void)h.table.unify(node1, node2, !h.env.compat->magic_compat);
}

}  // namespace hefesto::extract::hier
