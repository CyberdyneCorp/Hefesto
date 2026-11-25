// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Triangles of the exported layers (design D14): fixed triangulation,
// coordinates from the integer geometry in double rounded once to float,
// normals from the integer geometry. hefesto/view3d/export/export.hpp
// describes the faces and their order.
//
// Owner: EXPORT (design D3).

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "view3d/export/model.hpp"

namespace hefesto::view3d::detail {

namespace {

using Vec3 = std::array<float, 3>;

struct Slab {
  float bottom = 0.0F;
  float top = 0.0F;
};

// Root coordinates of a chunk-relative value are integers: origin + offset.
class Placement {
 public:
  Placement(const MeshModel& mesh, double unit_um) : x0_(mesh.region.x0), y0_(mesh.region.y0), unit_(unit_um) {}

  void set_chunk(const Chunk& chunk) {
    cx_ = chunk.origin_x;
    cy_ = chunk.origin_y;
  }
  // Model units relative to the model origin.
  [[nodiscard]] double dx(float x) const { return cx_ + static_cast<double>(x) - static_cast<double>(x0_); }
  [[nodiscard]] double dy(float y) const { return cy_ + static_cast<double>(y) - static_cast<double>(y0_); }
  [[nodiscard]] float x(float value) const { return static_cast<float>(dx(value) * unit_); }
  [[nodiscard]] float y(float value) const { return static_cast<float>(dy(value) * unit_); }

 private:
  db::Coord x0_;
  db::Coord y0_;
  double unit_;
  double cx_ = 0.0;
  double cy_ = 0.0;
};

void push_vertex(LayerGeometry& g, const Vec3& p, const Vec3& n) {
  g.positions.insert(g.positions.end(), p.begin(), p.end());
  g.normals.insert(g.normals.end(), n.begin(), n.end());
}

void add_triangle(LayerGeometry& g, const std::array<Vec3, 3>& v, const Vec3& n) {
  const auto base = static_cast<std::uint32_t>(g.vertex_count());
  for (const Vec3& p : v) {
    push_vertex(g, p, n);
  }
  g.indices.insert(g.indices.end(), {base, base + 1, base + 2});
}

// Two triangles (0, 1, 2) and (0, 2, 3): the diagonal from corner 0.
void add_quad(LayerGeometry& g, const std::array<Vec3, 4>& v, const Vec3& n) {
  const auto base = static_cast<std::uint32_t>(g.vertex_count());
  for (const Vec3& p : v) {
    push_vertex(g, p, n);
  }
  g.indices.insert(g.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

constexpr Vec3 kUp{0.0F, 0.0F, 1.0F};
constexpr Vec3 kDown{0.0F, 0.0F, -1.0F};

void add_rect(LayerGeometry& g, const Placement& at, const RectInstance& r, const Slab& z) {
  if (!(r.x0 < r.x1 && r.y0 < r.y1)) {
    return;
  }
  const float x0 = at.x(r.x0);
  const float y0 = at.y(r.y0);
  const float x1 = at.x(r.x1);
  const float y1 = at.y(r.y1);
  add_quad(g, {Vec3{x0, y0, z.top}, Vec3{x1, y0, z.top}, Vec3{x1, y1, z.top}, Vec3{x0, y1, z.top}}, kUp);
  if (z.top != z.bottom) {
    add_quad(g, {Vec3{x0, y0, z.bottom}, Vec3{x0, y1, z.bottom}, Vec3{x1, y1, z.bottom}, Vec3{x1, y0, z.bottom}},
             kDown);
  }
}

Slab own_slab(const LayerStack& stack, float height, float thickness, double k) {
  const double h = draw_to_um(stack, height);
  const double t = draw_to_um(stack, thickness);
  return Slab{static_cast<float>(h * k), static_cast<float>((h + t) * k)};
}

void add_side(LayerGeometry& g, const Placement& at, const SideInstance& s, const Slab& z) {
  const double dx = at.dx(s.x1) - at.dx(s.x0);
  const double dy = at.dy(s.y1) - at.dy(s.y0);
  const double length = std::sqrt((dx * dx) + (dy * dy));
  if (length == 0.0 || z.top == z.bottom) {
    return;
  }
  const bool left = (s.flags & side_flag::outward_left) != 0;
  // Outward to the right of a -> b: (dy, -dx); to the left: (-dy, dx).
  const double sign = left ? -1.0 : 1.0;
  const Vec3 n{static_cast<float>(sign * dy / length), static_cast<float>(-sign * dx / length), 0.0F};
  Vec3 a{at.x(s.x0), at.y(s.y0), 0.0F};
  Vec3 b{at.x(s.x1), at.y(s.y1), 0.0F};
  if (left) {
    std::swap(a, b);
  }
  add_quad(g, {Vec3{a[0], a[1], z.bottom}, Vec3{b[0], b[1], z.bottom}, Vec3{b[0], b[1], z.top},
               Vec3{a[0], a[1], z.top}},
           n);
}

void add_split_triangle(LayerGeometry& g, const Placement& at, const Triangle& t, const Slab& z) {
  std::array<double, 3> px{};
  std::array<double, 3> py{};
  for (std::size_t i = 0; i < 3; ++i) {
    px[i] = at.dx(t.x[i]);
    py[i] = at.dy(t.y[i]);
  }
  const double area2 = ((px[1] - px[0]) * (py[2] - py[0])) - ((px[2] - px[0]) * (py[1] - py[0]));
  if (area2 == 0.0) {
    return;
  }
  std::array<std::size_t, 3> order{0, 1, 2};
  if (area2 < 0.0) {
    std::swap(order[1], order[2]);  // counter-clockwise seen from above
  }
  std::array<Vec3, 3> top{};
  std::array<Vec3, 3> bottom{};
  for (std::size_t i = 0; i < 3; ++i) {
    const std::size_t v = order[i];
    top[i] = Vec3{at.x(t.x[v]), at.y(t.y[v]), z.top};
    bottom[2 - i] = Vec3{top[i][0], top[i][1], z.bottom};
  }
  add_triangle(g, top, kUp);
  if (z.top != z.bottom) {
    add_triangle(g, bottom, kDown);
  }
}

class Builder {
 public:
  Builder(const MeshModel& mesh, const LayerStack& stack, const ExportOptions& options,
          std::span<const ExportLayer> layers)
      : stack_(stack), k_(options.zscale.value_or(1.0)),
        at_(mesh, detail::xy_unit_um(mesh, stack, options)),
        slot_(stack.size(), kNone), slabs_(layers.size()), out_(layers.size()) {
    for (std::size_t i = 0; i < layers.size(); ++i) {
      slot_[layers[i].layer] = i;
      slabs_[i] = Slab{static_cast<float>(layers[i].z_bottom_um), static_cast<float>(layers[i].z_top_um)};
    }
  }

  void add_chunk(const Chunk& chunk) {
    at_.set_chunk(chunk);
    for (const RectInstance& r : chunk.rects) {
      if (const std::size_t i = slot(r.layer); i != kNone) {
        add_rect(out_[i], at_, r, slabs_[i]);
      }
    }
    for (const SideInstance& s : chunk.sides) {
      if (const std::size_t i = slot(s.layer); i != kNone) {
        const bool own = (s.flags & side_flag::own_height) != 0;
        add_side(out_[i], at_, s, own ? own_slab(stack_, s.height, s.thickness, k_) : slabs_[i]);
      }
    }
    for (const Triangle& t : chunk.triangles) {
      if (const std::size_t i = slot(t.layer); i != kNone) {
        add_split_triangle(out_[i], at_, t, own_slab(stack_, t.height, t.thickness, k_));
      }
    }
  }

  [[nodiscard]] std::vector<LayerGeometry> take() { return std::move(out_); }

 private:
  static constexpr std::size_t kNone = static_cast<std::size_t>(-1);

  [[nodiscard]] std::size_t slot(LayerId layer) const { return layer < slot_.size() ? slot_[layer] : kNone; }

  const LayerStack& stack_;
  double k_;
  Placement at_;
  std::vector<std::size_t> slot_;  // per stack layer: index in the exported layers
  std::vector<Slab> slabs_;
  std::vector<LayerGeometry> out_;
};

}  // namespace

std::vector<LayerGeometry> build_geometry(const MeshModel& mesh, const LayerStack& stack,
                                          const ExportOptions& options, std::span<const ExportLayer> layers) {
  Builder builder(mesh, stack, options, layers);
  for (const Chunk& chunk : mesh.chunks) {
    builder.add_chunk(chunk);
  }
  return builder.take();
}

}  // namespace hefesto::view3d::detail
