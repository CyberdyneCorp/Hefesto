// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Shared helpers of the view3d/mesh tests: a small technology (plane
// `active` with ndiff and poly, plane `metal1` with metal1), hand-built
// layer stacks (TECH builds the real ones in parallel), and checks over a
// MeshModel in root coordinates: per-layer face areas, side lengths and
// the closure test (each top-face boundary edge is matched by a side face).

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "hefesto/db/cell.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/view3d/mesh/build.hpp"
#include "hefesto/view3d/stack/layer_stack.hpp"

namespace hefesto::view3d::test {

struct MeshTech {
  std::shared_ptr<const tech::Technology> tech;
  tech::TileType ndiff = 0;
  tech::TileType poly = 0;
  tech::TileType metal1 = 0;
};

inline MeshTech build_tech() {
  tech::TechnologyBuilder b;
  b.set_name("mesh");
  MeshTech t;
  REQUIRE(b.add_plane("active").ok());
  REQUIRE(b.add_plane("metal1").ok());
  const auto type = [&](std::string_view plane, std::string_view name) {
    auto r = b.add_type(plane, name);
    REQUIRE(r.ok());
    return r.value();
  };
  t.ndiff = type("active", "ndiff");
  t.poly = type("active", "poly");
  t.metal1 = type("metal1", "metal1");
  auto fin = b.finalize();
  REQUIRE(fin.ok());
  t.tech = fin.value();
  return t;
}

inline StackLayer layer(const std::string& name, tech::TileType type, float h, float t, std::size_t style = 0) {
  StackLayer l;
  l.name = name;
  l.type = type;
  l.style = style;
  l.height = h;
  l.thickness = t;
  l.source = ValueSource::extract;
  return l;
}

inline LayerStack stack_of(std::vector<StackLayer> layers) {
  return LayerStack{Mode::layout_layer, std::move(layers), true, 0.01};
}

inline void paint(db::CellDef& def, const MeshTech& t, const db::Rect& r, tech::TileType type) {
  REQUIRE(db::paint(def, *t.tech, r, type).ok());
  db::recompute_bbox(def);
}

inline db::CellUse& place(db::CellDef& parent, db::CellDef& child, const db::Transform& tr, const std::string& id) {
  db::PlaceOptions o;
  o.id = id;
  o.transform = tr;
  auto u = db::place(parent, child, o);
  REQUIRE(u.ok());
  db::recompute_bbox(parent);
  return *u.value();
}

inline std::optional<MeshModel> build(const LayerStack& stack, const db::CellDef& cell, const db::Rect& area,
                                      bool cut_box = true) {
  BuildContext ctx;
  return build_from_layout(stack, cell, ClipArea{area, cut_box, 0}, ctx);
}

/// A segment or rectangle in root coordinates.
struct Seg {
  double x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  std::uint8_t flags = 0;

  friend bool operator==(const Seg&, const Seg&) = default;
  friend bool operator<(const Seg& a, const Seg& b) {
    return std::tie(a.x0, a.y0, a.x1, a.y1, a.flags) < std::tie(b.x0, b.y0, b.x1, b.y1, b.flags);
  }
};

inline std::vector<Seg> rects_of(const MeshModel& m, LayerId layer) {
  std::vector<Seg> out;
  for (const Chunk& c : m.chunks) {
    for (const RectInstance& r : c.rects) {
      if (r.layer == layer) {
        out.push_back(Seg{c.origin_x + static_cast<double>(r.x0), c.origin_y + static_cast<double>(r.y0),
                          c.origin_x + static_cast<double>(r.x1), c.origin_y + static_cast<double>(r.y1), 0});
      }
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

inline std::vector<Seg> sides_of(const MeshModel& m, LayerId layer) {
  std::vector<Seg> out;
  for (const Chunk& c : m.chunks) {
    for (const SideInstance& s : c.sides) {
      if (s.layer == layer) {
        out.push_back(Seg{c.origin_x + static_cast<double>(s.x0), c.origin_y + static_cast<double>(s.y0),
                          c.origin_x + static_cast<double>(s.x1), c.origin_y + static_cast<double>(s.y1), s.flags});
      }
    }
  }
  std::sort(out.begin(), out.end());
  return out;
}

/// Each triangle as its three vertices in root coordinates.
inline std::vector<std::vector<std::pair<double, double>>> triangles_of(const MeshModel& m, LayerId layer) {
  std::vector<std::vector<std::pair<double, double>>> out;
  for (const Chunk& c : m.chunks) {
    for (const Triangle& t : c.triangles) {
      if (t.layer == layer) {
        out.push_back({{c.origin_x + static_cast<double>(t.x[0]), c.origin_y + static_cast<double>(t.y[0])},
                       {c.origin_x + static_cast<double>(t.x[1]), c.origin_y + static_cast<double>(t.y[1])},
                       {c.origin_x + static_cast<double>(t.x[2]), c.origin_y + static_cast<double>(t.y[2])}});
      }
    }
  }
  return out;
}

/// The top-face area of one layer (rectangles and triangles).
inline double face_area(const MeshModel& m, LayerId layer) {
  double a = 0.0;
  for (const Seg& r : rects_of(m, layer)) {
    a += (r.x1 - r.x0) * (r.y1 - r.y0);
  }
  for (const auto& t : triangles_of(m, layer)) {
    a += 0.5 * (((t[1].first - t[0].first) * (t[2].second - t[0].second)) -
                ((t[2].first - t[0].first) * (t[1].second - t[0].second)));
  }
  return a;
}

inline double side_length(const MeshModel& m, LayerId layer) {
  double len = 0.0;
  for (const Seg& s : sides_of(m, layer)) {
    len += std::hypot(s.x1 - s.x0, s.y1 - s.y0);
  }
  return len;
}

/// "Each edge shared by two faces": the boundary of the top faces (their
/// counter-clockwise edges, inner edges cancelling) equals the edges the
/// side faces stand on, oriented by their outward flag. Every collinear
/// set of edges is compared as a signed coverage function along its line.
class Closure {
 public:
  void add_top(double ax, double ay, double bx, double by) { add(ax, ay, bx, by, 1); }
  void add_side(const Seg& s) {
    // Outward on the left of a->b: the interior is on the right, so the
    // top face runs b->a along this edge.
    if ((s.flags & side_flag::outward_left) != 0) {
      add(s.x1, s.y1, s.x0, s.y0, -1);
    } else {
      add(s.x0, s.y0, s.x1, s.y1, -1);
    }
  }

  /// Empty when closed; otherwise a description of the first open line.
  [[nodiscard]] std::string open_edge() const {
    for (const auto& [key, deltas] : lines_) {
      int level = 0;
      for (const auto& [t, d] : deltas) {
        level += d;
        if (level != 0) {
          return "line (" + std::to_string(std::get<0>(key)) + "," + std::to_string(std::get<1>(key)) + "," +
                 std::to_string(std::get<2>(key)) + ") at " + std::to_string(t);
        }
      }
    }
    return {};
  }

 private:
  using Key = std::tuple<int, double, double>;  // kind (0 vertical, 1 other), slope or x, intercept

  void add(double ax, double ay, double bx, double by, int sign) {
    if (ax == bx && ay == by) {
      return;
    }
    Key key;
    double ta = 0;
    double tb = 0;
    if (ax == bx) {
      key = Key{0, ax, 0.0};
      ta = ay;
      tb = by;
    } else {
      const double slope = (by - ay) / (bx - ax);
      key = Key{1, slope, std::round((ay - (slope * ax)) * 1e6) / 1e6};
      ta = ax;
      tb = bx;
    }
    const int dir = tb > ta ? 1 : -1;
    auto& d = lines_[key];
    d[std::min(ta, tb)] += sign * dir;
    d[std::max(ta, tb)] -= sign * dir;
  }

  std::map<Key, std::map<double, int>> lines_;
};

/// The closure check of one layer of `m`.
inline std::string open_edge(const MeshModel& m, LayerId layer) {
  Closure c;
  for (const Seg& r : rects_of(m, layer)) {
    c.add_top(r.x0, r.y0, r.x1, r.y0);
    c.add_top(r.x1, r.y0, r.x1, r.y1);
    c.add_top(r.x1, r.y1, r.x0, r.y1);
    c.add_top(r.x0, r.y1, r.x0, r.y0);
  }
  for (const auto& t : triangles_of(m, layer)) {
    for (int i = 0; i < 3; ++i) {
      const auto& a = t[static_cast<std::size_t>(i)];
      const auto& b = t[static_cast<std::size_t>((i + 1) % 3)];
      c.add_top(a.first, a.second, b.first, b.second);
    }
  }
  for (const Seg& s : sides_of(m, layer)) {
    c.add_side(s);
  }
  return c.open_edge();
}

// A 16 x 16 grid of separate metal1 squares with an ndiff strip under it.
inline void paint_grid(db::CellDef& def, const MeshTech& t, bool reverse) {
  for (int k = 0; k < 256; ++k) {
    const int i = reverse ? 255 - k : k;
    const db::Coord x = (i % 16) * 10;
    const db::Coord y = (i / 16) * 10;
    paint(def, t, db::Rect{x, y, x + 6, y + 6}, t.metal1);
  }
  paint(def, t, db::Rect{0, 0, 160, 3}, t.ndiff);
}

inline LayerStack grid_stack(const MeshTech& t) {
  return stack_of({layer("metal1", t.metal1, 2.0F, 0.5F), layer("ndiff", t.ndiff, 0.0F, 0.1F)});
}

// The bytes of every field (padding excluded): floats compared bit for bit.
inline std::uint32_t bits(float f) { return std::bit_cast<std::uint32_t>(f); }

inline bool same(const RectInstance& a, const RectInstance& b) {
  return bits(a.x0) == bits(b.x0) && bits(a.y0) == bits(b.y0) && bits(a.x1) == bits(b.x1) &&
         bits(a.y1) == bits(b.y1) && a.layer == b.layer;
}

inline bool same(const SideInstance& a, const SideInstance& b) {
  return bits(a.x0) == bits(b.x0) && bits(a.y0) == bits(b.y0) && bits(a.x1) == bits(b.x1) &&
         bits(a.y1) == bits(b.y1) && a.layer == b.layer && a.flags == b.flags && bits(a.height) == bits(b.height) &&
         bits(a.thickness) == bits(b.thickness);
}

inline bool same(const Triangle& a, const Triangle& b) {
  for (int i = 0; i < 3; ++i) {
    if (bits(a.x[i]) != bits(b.x[i]) || bits(a.y[i]) != bits(b.y[i])) {
      return false;
    }
  }
  return a.layer == b.layer && bits(a.height) == bits(b.height) && bits(a.thickness) == bits(b.thickness);
}

template <typename T>
inline bool bytes_equal(const std::vector<T>& a, const std::vector<T>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (!same(a[i], b[i])) {
      return false;
    }
  }
  return true;
}

// Versions are left out: they come from a process-wide counter, so two
// builds of the same cell differ in them alone (chunks_test.cpp checks
// them).
inline bool chunks_byte_equal(const Chunk& x, const Chunk& y) {
  return x.id == y.id && x.origin_x == y.origin_x && x.origin_y == y.origin_y &&
         x.area == y.area && bytes_equal(x.rects, y.rects) && bytes_equal(x.sides, y.sides) &&
         bytes_equal(x.triangles, y.triangles);
}

inline bool models_byte_equal(const MeshModel& a, const MeshModel& b) {
  if (a.chunks.size() != b.chunks.size() || !(a.region == b.region)) {
    return false;
  }
  for (std::size_t i = 0; i < a.chunks.size(); ++i) {
    if (!chunks_byte_equal(a.chunks[i], b.chunks[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace hefesto::view3d::test
