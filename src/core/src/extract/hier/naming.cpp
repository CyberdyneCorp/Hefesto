// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Node names in hierarchical extraction (extract-hierarchy "Interaction
// processing and connections (pass 1)", "Hard-way node names", "Array
// interactions"). Ported from Magic 8.3.694 (fc5daa9e) extract/ExtSubtree.c
// (extSubtreeTileToNode, extConnFindFunc) and extract/ExtArray.c
// (extArrayTileToNode, extArrayRange); Magic is Copyright (C) 1985, 1990
// Regents of the University of California (permissive license,
// THIRD_PARTY_NOTICES.md).
//
// Owner: EXTHIER.

#include <string>
#include <utility>

#include "hier_context.hpp"
#include "hier_geometry.hpp"

namespace hefesto::extract::hier {

namespace {

constexpr const char* kNone = "(none)";
constexpr const char* kWarning = "Warning: node labels should be inside overlap area";
constexpr const char* kError = "Cannot find the name of this node (probable extractor error)";

const tech::TypeMask& paint_types() {
  static const tech::TypeMask mask = ~tech::TypeMask::of(tech::kSpace);
  return mask;
}

// The node of `t` in `et` when it has labels (step 1 of both namings).
std::optional<std::size_t> labelled_node(const FlatTree& et, const TileRef& t) {
  const auto n = node_at(et, t);
  if (n && *n < et.nodes.size() && !et.nodes[*n].labels.empty()) {
    return n;
  }
  return std::nullopt;
}

void name_error(Hier& h, const TileRef& t) {
  ++h.result->errors;
  feedback(h, t.tile.area, kError, true);
}

std::optional<std::string> subtree_name(Hier& h, const TileRef& t, FlatTree& et, bool hard) {
  if (const auto n = labelled_node(et, t)) {
    return flat_node_name(et.nodes[*n], h);
  }
  if (et.look_names != nullptr) {
    if (auto name = parent_name_at(h, t)) {
      return name;
    }
  }
  if (!hard) {
    return std::nullopt;
  }
  if (node_at(et, t) && hard_node(h, t, et)) {
    if (h.env.options->warn_labels) {
      feedback(h, t.tile.area, kWarning, false);
      ++h.result->warnings;
    }
    return flat_node_name(et.nodes[*node_at(et, t)], h);
  }
  name_error(h, t);
  return std::string{kNone};
}

// extArrayRange.
void append_range(std::string& out, std::int32_t lo, std::int32_t hi, bool prev, bool follow) {
  if (!prev) {
    out += '[';
  }
  if (hi < lo) {
    std::swap(lo, hi);
  }
  out += std::to_string(lo) + ":" + std::to_string(hi);
  out += follow ? ',' : ']';
}

std::int32_t far_index(std::int32_t v, std::int32_t lo, std::int32_t hi) {
  return v == lo ? hi : lo;
}

std::string array_prefix(const Hier& h, const FlatTree& et) {
  const db::CellUse& use = *h.array.use;
  const bool hx = has_x(use);
  const bool hy = has_y(use);
  const ArrayState& a = h.array;
  std::string out = use.id;
  const bool primary = &et == a.primary;
  const std::int32_t fy = far_index(a.py, use.array.ylo, use.array.yhi);
  const std::int32_t fx = far_index(a.px, use.array.xlo, use.array.xhi);
  if (hy) {
    append_range(out, primary ? a.py : a.iy, primary ? fy - (a.iy - a.py) : fy, false, hx);
  }
  if (hx) {
    append_range(out, primary ? a.px : a.ix, primary ? fx - (a.ix - a.px) : fx, hy, false);
  }
  return out + "/";
}

std::optional<std::string> array_name(Hier& h, const TileRef& t, FlatTree& et, bool hard) {
  std::optional<std::size_t> n = labelled_node(et, t);
  if (!n && node_at(et, t) && hard_node(h, t, et)) {
    n = node_at(et, t);
  }
  if (!n) {
    if (!hard) {
      return std::nullopt;
    }
    name_error(h, t);
    return std::string{kNone};
  }
  return array_prefix(h, et) + flat_node_name(et.nodes[*n], h);
}

}  // namespace

std::optional<std::string> parent_name_at(const Hier& h, const TileRef& t) {
  const ParentNodes& parent = h.inputs->parent;
  if (parent.finder == nullptr || t.plane < tech::kFirstUserPlane || t.plane >= h.parent->plane_count()) {
    return std::nullopt;
  }
  const db::Plane& plane = h.parent->plane(t.plane);
  std::optional<std::size_t> found;
  const auto visit = [&](const db::TileView& v) {
    found = parent.finder->node_of(plane, v, parent.list_size);
    return !found.has_value();
  };
  if (t.tile.split) {
    (void)plane.enumerate_triangle(t.tile.area, t.tile.diagonal, t.tile.right_side, paint_types(), visit);
  } else {
    (void)plane.enumerate(t.tile.area, paint_types(), visit);
  }
  if (!found || *found >= parent.names.size()) {
    return std::nullopt;
  }
  return parent.names[*found];
}

std::optional<std::string> tile_to_node(Hier& h, const TileRef& t, FlatTree& et, bool hard) {
  return h.naming == Naming::array ? array_name(h, t, et, hard) : subtree_name(h, t, et, hard);
}

}  // namespace hefesto::extract::hier
