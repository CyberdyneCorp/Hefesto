// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Cross-sections (openspec/changes/m10-3d-view/design.md D10, P2; task
// 11.1; hefesto-3d-visualization "Interactive 3D inspection"). Read from the
// mesh model only: the rectangles and split-tile triangles of each layer
// that the axis-aligned cut line crosses, merged per layer, in µm from the
// line's start, with the layer's physical height (Q-MISC-72: µm in both
// magic-compat modes). The rules are in section.hpp.
//
// Owner: INSPECT (design D3).

#include "hefesto/view3d/section/section.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hefesto::view3d {

namespace {

constexpr std::string_view kBadLine = "VIEW3D-SECTION-001";
constexpr std::string_view kNoHeightLayout = "VIEW3D-SECTION-002";
constexpr std::string_view kNoRenderHeight = "VIEW3D-STACK-001";

// Pieces closer than this (mesh units) along the line are one interval:
// triangle cuts are computed in double from float corners.
constexpr double kMergeGap = 1e-6;

/// The cut line in mesh units (root coordinates times
/// MeshModel::units_per_root), along one axis.
struct Line {
  bool horizontal = true;  // along x at y = level; otherwise along y at x = level
  double level = 0.0;
  double lo = 0.0;  // the covered range along the axis
  double hi = 0.0;
  double start = 0.0;     // a's coordinate along the axis
  bool reversed = false;  // b lies before a along the axis
};

std::optional<Line> line_of(const Segment& s) {
  const bool horizontal = s.a.y == s.b.y;
  const bool vertical = s.a.x == s.b.x;
  if (horizontal == vertical) {  // diagonal, or a single point
    return std::nullopt;
  }
  Line l;
  l.horizontal = horizontal;
  l.level = horizontal ? s.a.y : s.a.x;
  const double a = horizontal ? s.a.x : s.a.y;
  const double b = horizontal ? s.b.x : s.b.y;
  l.lo = std::min(a, b);
  l.hi = std::max(a, b);
  l.start = a;
  l.reversed = b < a;
  return l;
}

/// A point in mesh units as (along the line, across it).
struct Local {
  double along = 0.0;
  double across = 0.0;
};

Local local(const Line& l, double x, double y) {
  return l.horizontal ? Local{x, y} : Local{y, x};
}

using Range = std::pair<double, double>;  // along the line, mesh units
using Pieces = std::vector<std::vector<Range>>;  // per LayerId

void add_piece(Pieces& pieces, LayerId layer, const Line& l, double from, double to) {
  const double a = std::max(from, l.lo);
  const double b = std::min(to, l.hi);
  if (!(b > a)) {
    return;
  }
  if (layer >= pieces.size()) {
    pieces.resize(static_cast<std::size_t>(layer) + 1U);
  }
  pieces[layer].emplace_back(a, b);
}

void add_rect(Pieces& pieces, const Chunk& c, const RectInstance& r, const Line& l) {
  const Local p0 = local(l, c.origin_x + static_cast<double>(r.x0), c.origin_y + static_cast<double>(r.y0));
  const Local p1 = local(l, c.origin_x + static_cast<double>(r.x1), c.origin_y + static_cast<double>(r.y1));
  const double across_lo = std::min(p0.across, p1.across);
  const double across_hi = std::max(p0.across, p1.across);
  if (across_lo <= l.level && l.level < across_hi) {
    add_piece(pieces, r.layer, l, std::min(p0.along, p1.along), std::max(p0.along, p1.along));
  }
}

/// The range along the line of the triangle's cut at `l.level` (the edges
/// crossing it), when the line samples the triangle (lowest across <=
/// level < highest across).
std::optional<Range> triangle_cut(const std::array<Local, 3>& p, const Line& l) {
  const double lo = std::min({p[0].across, p[1].across, p[2].across});
  const double hi = std::max({p[0].across, p[1].across, p[2].across});
  if (!(lo <= l.level && l.level < hi)) {
    return std::nullopt;
  }
  double from = 0.0;
  double to = 0.0;
  bool any = false;
  for (std::size_t i = 0; i < 3; ++i) {
    const Local& a = p[i];
    const Local& b = p[(i + 1) % 3];
    if (a.across == b.across || l.level < std::min(a.across, b.across) || l.level > std::max(a.across, b.across)) {
      continue;
    }
    const double x = a.along + ((l.level - a.across) * (b.along - a.along) / (b.across - a.across));
    from = any ? std::min(from, x) : x;
    to = any ? std::max(to, x) : x;
    any = true;
  }
  return any ? std::optional<Range>(Range{from, to}) : std::nullopt;
}

void add_triangle(Pieces& pieces, const Chunk& c, const Triangle& t, const Line& l) {
  std::array<Local, 3> p{};
  for (std::size_t i = 0; i < 3; ++i) {
    p[i] = local(l, c.origin_x + static_cast<double>(t.x[i]), c.origin_y + static_cast<double>(t.y[i]));
  }
  if (const auto cut = triangle_cut(p, l)) {
    add_piece(pieces, t.layer, l, cut->first, cut->second);
  }
}

/// Sorts and merges abutting or overlapping ranges.
std::vector<Range> merged(std::vector<Range> ranges) {
  std::sort(ranges.begin(), ranges.end());
  std::vector<Range> out;
  for (const Range& r : ranges) {
    if (!out.empty() && r.first <= out.back().second + kMergeGap) {
      out.back().second = std::max(out.back().second, r.second);
    } else {
      out.push_back(r);
    }
  }
  return out;
}

bool hidden_layer(const std::vector<bool>& hidden, std::size_t layer) {
  return layer < hidden.size() && hidden[layer];
}

double microns_per_unit(const LayerStack& stack) {
  const double mpu = stack.microns_per_unit();
  return std::isfinite(mpu) && mpu > 0.0 ? mpu : 0.0;
}

/// The µm height of `layer`: the model's own h and t when they differ
/// from the stack's (a contact spanned by the J1 option), else the
/// stack's physical height.
std::optional<PhysicalHeight> layer_height_um(const MeshModel& mesh, const LayerStack& stack, LayerId layer) {
  const std::optional<PhysicalHeight> physical = stack.physical_height_um(layer);
  if (!physical || layer >= mesh.layer_heights.size() || stack.mode() != Mode::layout_layer) {
    return physical;
  }
  const DrawHeight drawn = mesh.layer_heights[layer];
  if (drawn == stack.draw_height(layer)) {
    return physical;
  }
  const double mpu = microns_per_unit(stack);
  return PhysicalHeight{static_cast<double>(drawn.height) * mpu, static_cast<double>(drawn.thickness) * mpu};
}

std::string layer_name(const LayerStack& stack, LayerId layer) {
  return layer < stack.size() ? stack.layers()[layer].name : "#" + std::to_string(layer);
}

std::optional<diag::Diagnostic> no_height_diagnostic(const LayerStack& stack, const std::vector<LayerId>& layers) {
  if (layers.empty()) {
    return std::nullopt;
  }
  std::vector<std::string> names;
  for (const LayerId l : layers) {
    std::string name = layer_name(stack, l);
    if (std::find(names.begin(), names.end(), name) == names.end()) {
      names.push_back(std::move(name));
    }
  }
  const bool mask = stack.mode() == Mode::mask_layer;
  std::string text = mask ? "No render height for mask layer(s):" : "No physical height for layer(s):";
  for (const std::string& n : names) {
    text += " " + n;
  }
  text += mask ? "; they are left out of the cross-section (the output style has no `render` statement for them; "
                 "the layout-layer view takes heights from the extract style)"
               : "; they are left out of the cross-section (no extract style height, or no µm per internal unit)";
  return diag::make_native(mask ? kNoRenderHeight : kNoHeightLayout, std::move(text));
}

/// The intervals of one layer in µm from the line's start, by position.
void add_intervals(CrossSection& out, LayerId layer, const std::vector<Range>& ranges, const Line& l, double mpu,
                   const PhysicalHeight& h) {
  const std::size_t first = out.intervals.size();
  for (const Range& r : ranges) {
    CrossSection::Interval iv;
    iv.layer = layer;
    iv.from_um = (l.reversed ? l.start - r.second : r.first - l.start) * mpu;
    iv.to_um = (l.reversed ? l.start - r.first : r.second - l.start) * mpu;
    iv.bottom_um = h.height_um;
    iv.top_um = h.height_um + h.thickness_um;
    out.intervals.push_back(iv);
  }
  std::sort(out.intervals.begin() + static_cast<std::ptrdiff_t>(first), out.intervals.end(),
            [](const CrossSection::Interval& a, const CrossSection::Interval& b) { return a.from_um < b.from_um; });
}

Pieces collect(const MeshModel& mesh, const Line& l) {
  Pieces pieces;
  for (const Chunk& c : mesh.chunks) {
    for (const RectInstance& r : c.rects) {
      add_rect(pieces, c, r, l);
    }
    for (const Triangle& t : c.triangles) {
      add_triangle(pieces, c, t, l);
    }
  }
  return pieces;
}

}  // namespace

CrossSection section(const MeshModel& mesh, const LayerStack& stack, const Segment& line) {
  return section(mesh, stack, line, std::vector<bool>{});
}

CrossSection section(const MeshModel& mesh, const LayerStack& stack, const Segment& line,
                     const std::vector<bool>& hidden) {
  CrossSection out;
  const std::optional<Line> l = line_of(line);
  if (!l) {
    out.diagnostics.push_back(diag::make_native(
        kBadLine, "The cut line is not axis-aligned or has zero length; no cross-section is computed"));
    return out;
  }
  const double mpu = microns_per_unit(stack);
  // µm per mesh unit along the line: mask-layer meshes are in mask units.
  const double along = mpu / static_cast<double>(mesh.units_per_root > 0 ? mesh.units_per_root : 1);
  out.length_um = (l->hi - l->lo) * along;
  const Pieces pieces = collect(mesh, *l);
  for (std::size_t i = 0; i < pieces.size(); ++i) {
    const auto layer = static_cast<LayerId>(i);
    if (pieces[i].empty() || hidden_layer(hidden, i)) {
      continue;
    }
    const std::optional<PhysicalHeight> h = mpu > 0.0 ? layer_height_um(mesh, stack, layer) : std::nullopt;
    if (!h) {
      out.no_height.push_back(layer);
      continue;
    }
    add_intervals(out, layer, merged(pieces[i]), *l, along, *h);
  }
  if (auto d = no_height_diagnostic(stack, out.no_height)) {
    out.diagnostics.push_back(std::move(*d));
  }
  return out;
}

}  // namespace hefesto::view3d
