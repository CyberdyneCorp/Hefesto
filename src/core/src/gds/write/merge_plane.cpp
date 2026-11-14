// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Merged polygon output of one mask plane (capability gds-write "Merged
// polygon output", Q-MASK-217 replicate). Ported from Magic 8.3.694
// calma/CalmaWrite.c:2394-2700 (calmaMergePaintFunc), 2273-2330
// (calmaProcessBoundary) and 2723-2800 (calmaWritePaintFunc); Magic is (c)
// the Regents of the University of California, distributed under its
// permissive license (THIRD_PARTY_NOTICES.md).
//
// Magic keeps the tile ring of the last isolated tile in a static and reuses
// it for the next tile. The ring is fully re-initialized before use and its
// length always equals the tile's starting corner count, so the reuse is not
// observable and each tile gets a fresh ring here.
//
// Owner: GDSW (openspec/changes/m5-mask-gds-cif/design.md D2).

#include "hefesto/gds/write/merge_plane.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <unordered_map>

#include "hefesto/gds/write/merge_rings.hpp"

namespace hefesto::gds {

namespace {

using merge::BoundList;
using merge::RingArena;

/// The four facing types of a tile (TiGetTopType and its siblings).
tech::TileType top_type(const db::TileView& t) noexcept {
  if (!t.split) {
    return t.type;
  }
  return t.diagonal == db::Diagonal::nw_se ? t.right_type : t.left_type;
}
tech::TileType bottom_type(const db::TileView& t) noexcept {
  if (!t.split) {
    return t.type;
  }
  return t.diagonal == db::Diagonal::nw_se ? t.left_type : t.right_type;
}
tech::TileType left_type(const db::TileView& t) noexcept {
  return t.split ? t.left_type : t.type;
}
tech::TileType right_type(const db::TileView& t) noexcept {
  return t.split ? t.right_type : t.type;
}

/// A step of the counterclockwise search of one tile.
enum class Step : std::uint8_t { top, left, bottom, right, diagonal, end };

/// The steps per split type (-1 rectangle, else 2*side + direction).
constexpr std::array<std::array<Step, 4>, 5> kSteps{{
    {Step::top, Step::left, Step::bottom, Step::right},       // rectangle
    {Step::top, Step::left, Step::diagonal, Step::end},        // 0: `/`, upper-left solid
    {Step::left, Step::bottom, Step::diagonal, Step::end},     // 1: `\`, lower-left solid
    {Step::diagonal, Step::bottom, Step::right, Step::end},    // 2: `/`, lower-right solid
    {Step::top, Step::diagonal, Step::right, Step::end},       // 3: `\`, upper-right solid
}};

int split_type_of(const db::TileView& t) noexcept {
  if (!t.split) {
    return -1;
  }
  int type = t.diagonal == db::Diagonal::nw_se ? 1 : 0;
  if (t.left_type == tech::kSpace) {
    type |= 2;
  }
  return type;
}

/// GeoClip.
db::Rect clipped(db::Rect r, const std::optional<db::Rect>& clip) noexcept {
  if (clip) {
    r.x0 = std::max(r.x0, clip->x0);
    r.y0 = std::max(r.y0, clip->y0);
    r.x1 = std::min(r.x1, clip->x1);
    r.y1 = std::min(r.y1, clip->y1);
  }
  return r;
}

/// calmaWritePaintFunc's points (no closing point) for an isolated tile.
mask::Polygon isolated_polygon(const db::TileView& t, int split_type, const std::optional<db::Rect>& clip) {
  const db::Rect r = clipped(t.area, clip);
  switch (split_type) {
    case 0:
      return {{r.x0, r.y0}, {r.x0, r.y1}, {r.x1, r.y1}};
    case 1:
      return {{r.x0, r.y1}, {r.x0, r.y0}, {r.x1, r.y0}};
    case 2:
      return {{r.x0, r.y0}, {r.x1, r.y0}, {r.x1, r.y1}};
    case 3:
      return {{r.x0, r.y1}, {r.x1, r.y1}, {r.x1, r.y0}};
    default:
      return {{r.x0, r.y0}, {r.x1, r.y0}, {r.x1, r.y1}, {r.x0, r.y1}};
  }
}

/// The ring of one tile under construction.
struct TileRing {
  std::uint32_t edge = 0;    // the ring head (Magic's `edge`)
  std::uint32_t cursor = 0;  // calmaAddSegment's `lb`
  std::int64_t num_points = 0;
  std::int64_t internal_edges = 0;
};

class PlaneMerger {
 public:
  PlaneMerger(const db::Plane& plane, const std::optional<db::Rect>& clip, std::vector<mask::Polygon>& out)
      : plane_(plane), clip_(clip), out_(out) {}

  /// calmaMergePaintFunc for one enumerated tile.
  void visit(const db::TileView& tile) {
    if (marked(tile, Mark::processed)) {
      return;
    }
    arena_.clear();
    bounds_.clear();
    stack_.clear();
    push(tile);
    while (!stack_.empty()) {
      const db::TileView t = stack_.back();
      stack_.pop_back();
      if (!marked(t, Mark::pending)) {
        continue;
      }
      marks_[t.tile] = Mark::processed;
      if (process(t) && stack_.empty()) {
        return;  // an isolated tile ends the component (its bounds are dropped, as in Magic)
      }
    }
    merge::remove_degenerate(arena_, bounds_);
    merge::remove_colinear(arena_, bounds_);
    for (auto bound = bounds_.rbegin(); bound != bounds_.rend(); ++bound) {
      out_.push_back(arena_.points(bound->first));
    }
  }

 private:
  enum class Mark : std::uint8_t { pending, processed };

  [[nodiscard]] bool marked(const db::TileView& t, Mark mark) const {
    const auto it = marks_.find(t.tile);
    return it != marks_.end() && it->second == mark;
  }

  /// PUSHTILEC: an unmarked tile becomes pending and is stacked.
  void push(const db::TileView& t) {
    if (marks_.try_emplace(t.tile, Mark::pending).second) {
      stack_.push_back(t);
    }
  }

  /// One popped tile: its ring, searches and merge. True when it was
  /// isolated (and written at once).
  bool process(const db::TileView& t) {
    const int split_type = split_type_of(t);
    TileRing ring = init_ring(t, split_type);
    for (const Step step : kSteps[static_cast<std::size_t>(split_type + 1)]) {
      if (step == Step::end) {
        break;
      }
      run_step(t, step, split_type, ring);
    }
    if (ring.internal_edges == 0) {
      out_.push_back(isolated_polygon(t, split_type, clip_));
      return true;
    }
    merge::merge_segments(arena_, ring.edge, bounds_, ring.num_points);
    return false;
  }

  TileRing init_ring(const db::TileView& t, int split_type) {
    const db::Coord llx = t.area.x0;
    const db::Coord lly = t.area.y0;
    const db::Coord urx = t.area.x1;
    const db::Coord ury = t.area.y1;
    TileRing ring;
    ring.num_points = split_type < 0 ? 0 : 1;
    switch (split_type) {
      case 0:
        ring.edge = arena_.make_ring({{urx, ury}, {llx, ury}, {llx, lly}});
        break;
      case 1:
        ring.edge = arena_.make_ring({{llx, ury}, {llx, lly}, {urx, lly}});
        break;
      case 2:
        ring.edge = arena_.make_ring({{urx, ury}, {llx, lly}, {urx, lly}});
        break;
      case 3:
        ring.edge = arena_.make_ring({{urx, ury}, {llx, ury}, {urx, lly}});
        break;
      default:
        ring.edge = arena_.make_ring({{urx, ury}, {llx, ury}, {llx, lly}, {urx, lly}});
        break;
    }
    ring.cursor = ring.edge;
    return ring;
  }

  void run_step(const db::TileView& t, Step step, int split_type, TileRing& ring) {
    if (step == Step::diagonal) {
      add_diagonal(t, split_type, ring);
      return;
    }
    const db::Rect& a = t.area;
    const bool horizontal = step == Step::top || step == Step::bottom;
    plane_.neighbors(t, side_of(step), horizontal ? a.x0 : a.y0, horizontal ? a.x1 : a.y1, scratch_);
    // neighbors() orders by increasing x or y; Magic walks the top side
    // right to left and the right side top to bottom.
    if (step == Step::top || step == Step::right) {
      std::reverse(scratch_.begin(), scratch_.end());
    }
    for (const db::TileView& n : scratch_) {
      add_neighbor(t, step, n, ring);
    }
  }

  static db::Side side_of(Step step) noexcept {
    switch (step) {
      case Step::top:
        return db::Side::top;
      case Step::left:
        return db::Side::left;
      case Step::bottom:
        return db::Side::bottom;
      default:
        return db::Side::right;
    }
  }

  void add_neighbor(const db::TileView& t, Step step, const db::TileView& n, TileRing& ring) {
    const db::Rect& a = t.area;
    const db::Rect& b = n.area;
    bool external = false;
    db::Point p1;
    db::Point p2;
    switch (step) {
      case Step::top:
        external = bottom_type(n) != top_type(t);
        p1 = {std::min(a.x1, b.x1), a.y1};
        p2 = {std::max(a.x0, b.x0), a.y1};
        break;
      case Step::left:
        external = right_type(n) != left_type(t);
        p1 = {a.x0, std::min(a.y1, b.y1)};
        p2 = {a.x0, std::max(a.y0, b.y0)};
        break;
      case Step::bottom:
        external = top_type(n) != bottom_type(t);
        p1 = {std::max(a.x0, b.x0), a.y0};
        p2 = {std::min(a.x1, b.x1), a.y0};
        break;
      default:
        external = left_type(n) != right_type(t);
        p1 = {a.x1, std::max(a.y0, b.y0)};
        p2 = {a.x1, std::min(a.y1, b.y1)};
        break;
    }
    ring.internal_edges += merge::add_segment(arena_, ring.cursor, external, p1, p2);
    ++ring.num_points;
    if (!external) {
      push(n);
    }
  }

  void add_diagonal(const db::TileView& t, int split_type, TileRing& ring) {
    const db::Rect& a = t.area;
    db::Point p1;
    db::Point p2;
    switch (split_type) {
      case 0:
        p1 = {a.x0, a.y0};
        p2 = {a.x1, a.y1};
        break;
      case 1:
        p1 = {a.x1, a.y0};
        p2 = {a.x0, a.y1};
        break;
      case 2:
        p1 = {a.x1, a.y1};
        p2 = {a.x0, a.y0};
        break;
      default:
        p1 = {a.x0, a.y1};
        p2 = {a.x1, a.y0};
        break;
    }
    ring.internal_edges += merge::add_segment(arena_, ring.cursor, true, p1, p2);
  }

  const db::Plane& plane_;
  const std::optional<db::Rect>& clip_;
  std::vector<mask::Polygon>& out_;
  std::unordered_map<const void*, Mark> marks_;
  std::vector<db::TileView> stack_;
  std::vector<db::TileView> scratch_;
  RingArena arena_;
  BoundList bounds_;
};

diag::Diagnostic out_of_range(std::int64_t value) {
  return diag::make("GDS-WRITE-004", "GDS output coordinate " + std::to_string(value) +
                                         " does not fit a 4-byte integer; the write fails.");
}

}  // namespace

std::vector<mask::Polygon> merge_plane_polygons(const mask::MaskPlane& plane, const std::optional<db::Rect>& clip) {
  std::vector<mask::Polygon> out;
  PlaneMerger merger(plane.plane(), clip, out);
  const db::Rect area = clip.value_or(db::whole_plane());
  plane.plane().enumerate(area, tech::TypeMask::of(mask::kMaskSolid), [&](const db::TileView& t) {
    merger.visit(t);
    return true;
  });
  return out;
}

diag::Status write_xy(RecordWriter& out, std::span<const std::int64_t> coords) {
  std::vector<std::int32_t> values;
  values.reserve(coords.size());
  for (const std::int64_t v : coords) {
    if (v < std::numeric_limits<std::int32_t>::min() || v > std::numeric_limits<std::int32_t>::max()) {
      return out_of_range(v);
    }
    values.push_back(static_cast<std::int32_t>(v));
  }
  return out.write_int4(RecordType::xy, values);
}

diag::Status write_boundary(RecordWriter& out, std::int32_t layer, std::int32_t datatype,
                            std::span<const db::Point> polygon, std::int64_t scale) {
  if (polygon.empty()) {
    return diag::Done{};
  }
  std::vector<std::int64_t> coords;
  coords.reserve(2 * (polygon.size() + 1));
  const auto put = [&](db::Point p) {
    std::int64_t x = 0;
    std::int64_t y = 0;
    if (__builtin_mul_overflow(std::int64_t{p.x}, scale, &x) || __builtin_mul_overflow(std::int64_t{p.y}, scale, &y)) {
      return false;
    }
    coords.push_back(x);
    coords.push_back(y);
    return true;
  };
  for (const db::Point p : polygon) {
    if (!put(p)) {
      return out_of_range(std::numeric_limits<std::int64_t>::max());
    }
  }
  if (!put(polygon.front())) {
    return out_of_range(std::numeric_limits<std::int64_t>::max());
  }
  const std::array<std::int16_t, 1> layer_value{static_cast<std::int16_t>(layer)};
  const std::array<std::int16_t, 1> datatype_value{static_cast<std::int16_t>(datatype)};
  diag::Status status = out.write_empty(RecordType::boundary);
  if (status) {
    status = out.write_int2(RecordType::layer, layer_value);
  }
  if (status) {
    status = out.write_int2(RecordType::datatype, datatype_value);
  }
  if (status) {
    status = write_xy(out, coords);
  }
  if (status) {
    status = out.write_empty(RecordType::endel);
  }
  return status;
}

diag::Status emit_merged_boundaries(RecordWriter& out, const mask::MaskPlane& plane, std::int32_t layer,
                                    std::int32_t datatype, std::int64_t paint_scale,
                                    const std::optional<db::Rect>& clip) {
  for (const mask::Polygon& polygon : merge_plane_polygons(plane, clip)) {
    if (diag::Status status = write_boundary(out, layer, datatype, polygon, paint_scale); !status) {
      return status;
    }
  }
  return diag::Done{};
}

}  // namespace hefesto::gds
