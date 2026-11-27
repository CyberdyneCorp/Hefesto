// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The net search of net highlighting and the layout-net overlay of
// mask-layer mode (pick.hpp; openspec/changes/m10-3d-view/design.md D10,
// tasks.md 9.5, 10.4). The net's tiles are painted into a scratch cell in
// root coordinates (NetPaint); the overlay reads that cell back, one
// prism per tile of each type on its home plane (a contact's images on
// its other planes are skipped), clipped to the cut box and scaled to the
// mesh's units relative to the first prism's corner, at most
// kMaxNetOverlayPrisms prisms.
//
// Owner: CORE (M10 round 6; the search moved here from highlight.cpp).

#include <algorithm>
#include <limits>
#include <optional>

#include "hefesto/db/paint.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/select/selection.hpp"
#include "hefesto/view3d/client/session.hpp"
#include "hefesto/view3d/pick/pick.hpp"
#include "view3d/pick/internal.hpp"

namespace hefesto::view3d {

namespace detail {

NetPaint::NetPaint(const tech::Technology& tech) : tech_(&tech), cell_("(3d net)", tech.plane_count()) {}

bool NetPaint::on_tile(const connect::NetTile& t) {
  db::Plane& plane = cell_.plane(t.plane);
  const db::PaintRow row = db::paint_row(*tech_, t.plane, t.tile.type);
  if (t.tile.split) {
    (void)plane.paint_triangle(t.tile.area, t.tile.diagonal, t.tile.right_side, row);
  } else {
    (void)plane.paint(t.tile.area, row);
  }
  return true;
}

bool usable_seed(const PickTarget& seed, const NetOptions& options) noexcept {
  const bool flat = options.scope == NetScope::flat;
  return seed.type.has_value() && seed.cell != nullptr && !seed.material.is_null() && !(flat && !seed.use_path.empty());
}

bool search_net(const db::CellDef& cell, const db::Rect& area, tech::TileType type, const NetOptions& options,
                NetPaint& net) {
  const tech::Technology& tech = net.technology();
  connect::SearchStart start;
  start.area = area;
  start.types = tech::TypeMask::of(type);
  connect::SearchOptions search;
  search.magic_compat = options.magic_compat;
  const diag::Status status = options.scope == NetScope::flat
                                  ? connect::search_flat(cell, tech, start, search, net)
                                  : connect::search_hierarchical(cell, tech, start, search, net);
  return status.ok();
}

}  // namespace detail

namespace {

/// The drawn stack's z range (NetOverlayPrism): the layers with a positive
/// thickness, or every layer when none has one.
struct ZRange {
  float bottom = 0.0F;
  float top = 0.0F;
};

ZRange z_range(const MeshModel& mesh) {
  const std::vector<DrawHeight>& heights = mesh.layer_heights;
  const bool any_thick =
      std::any_of(heights.begin(), heights.end(), [](const DrawHeight& d) { return d.thickness > 0.0F; });
  std::optional<ZRange> range;
  for (const DrawHeight& d : heights) {
    if (any_thick && !(d.thickness > 0.0F)) {
      continue;
    }
    const float top = d.height + d.thickness;
    range = range ? ZRange{std::min(range->bottom, d.height), std::max(range->top, top)} : ZRange{d.height, top};
  }
  return range.value_or(ZRange{});
}

/// The types other than space, for enumerating a scratch plane.
tech::TypeMask painted_types() {
  tech::TypeMask mask = tech::TypeMask::all();
  mask.clear_type(tech::kSpace);
  return mask;
}

/// Reads the net paint back as prisms in mesh units, relative to the first
/// prism's lower-left corner (NetOverlay::origin_x, origin_y), at most
/// kMaxNetOverlayPrisms of them.
class OverlayWriter {
 public:
  OverlayWriter(const MeshModel& mesh, const std::optional<db::Rect>& cut_box)
      : scale_(mesh.units_per_root > 0 ? mesh.units_per_root : 1),
        clip_(cut_box.value_or(db::whole_plane())),
        z_(z_range(mesh)) {}

  NetOverlay read(const detail::NetPaint& net) {
    const tech::Technology& tech = net.technology();
    const db::CellDef& cell = net.cell();
    const tech::TypeMask types = painted_types();
    for (std::size_t p = tech::kFirstUserPlane; p < cell.plane_count() && !out_.truncated; ++p) {
      const auto plane = static_cast<tech::PlaneId>(p);
      (void)cell.plane(plane).enumerate(clip_, types, [&](const db::TileView& tile) {
        return tech.home_plane(tile.type) != plane || add(tile);
      });
    }
    return std::move(out_);
  }

 private:
  /// False once the overlay is full (the tile is then left out).
  bool add(const db::TileView& tile) {
    const db::Rect r{std::max(tile.area.x0, clip_.x0), std::max(tile.area.y0, clip_.y0),
                     std::min(tile.area.x1, clip_.x1), std::min(tile.area.y1, clip_.y1)};
    if (r.x0 >= r.x1 || r.y0 >= r.y1) {
      return true;
    }
    if (out_.prisms.size() >= kMaxNetOverlayPrisms) {
      out_.truncated = true;
      return false;
    }
    if (out_.prisms.empty()) {
      out_.origin_x = to_mesh(r.x0);
      out_.origin_y = to_mesh(r.y0);
    }
    out_.prisms.push_back(NetOverlayPrism{relative(r.x0, out_.origin_x), relative(r.y0, out_.origin_y),
                                          relative(r.x1, out_.origin_x), relative(r.y1, out_.origin_y), z_.bottom,
                                          z_.top});
    out_.types.push_back(tile.type);
    return true;
  }

  [[nodiscard]] double to_mesh(db::Coord v) const noexcept {
    return static_cast<double>(v) * static_cast<double>(scale_);
  }

  [[nodiscard]] float relative(db::Coord v, double origin) const noexcept {
    return static_cast<float>(to_mesh(v) - origin);
  }

  std::int32_t scale_;
  db::Rect clip_;
  ZRange z_;
  NetOverlay out_;
};

/// Whether `net` already holds `type` material overlapping `area` (a
/// connected piece of one type lies in one net).
bool in_net(const detail::NetPaint& net, tech::PlaneId plane, const db::Rect& area, tech::TileType type) {
  const bool stopped = !net.cell().plane(plane).enumerate(
      area, tech::TypeMask::of(type), [&](const db::TileView& tile) { return !db::overlap(tile.area, area); });
  return stopped;
}

/// Adds the net of every selected tile not yet in `net`.
void search_selection(const db::CellDef& root, const db::CellDef& selected, const NetOptions& options,
                      detail::NetPaint& net) {
  const tech::Technology& tech = net.technology();
  const tech::TypeMask types = painted_types();
  for (std::size_t p = tech::kFirstUserPlane; p < selected.plane_count(); ++p) {
    const auto plane = static_cast<tech::PlaneId>(p);
    (void)selected.plane(plane).enumerate(db::whole_plane(), types, [&](const db::TileView& tile) {
      if (tech.home_plane(tile.type) == plane && !in_net(net, plane, tile.area, tile.type)) {
        (void)detail::search_net(root, tile.area, tile.type, options, net);
      }
      return true;
    });
  }
}

}  // namespace

NetOverlay net_overlay(const MeshModel& mesh, const editor::EditorState& editor, const WindowState& window,
                       const PickTarget& seed, const NetOptions& options) {
  if (!detail::usable_seed(seed, options) || seed.cell != editor.cells.find(window.cell)) {
    return NetOverlay{};
  }
  detail::NetPaint net(*editor.technology);
  if (!detail::search_net(*seed.cell, seed.material, *seed.type, options, net)) {
    return NetOverlay{};
  }
  return OverlayWriter(mesh, window.cut_box).read(net);
}

NetOverlay net_overlay(const MeshModel& mesh, const editor::EditorState& editor, const WindowState& window,
                       const select::Selection& selection, const NetOptions& options) {
  const db::CellDef* root = editor.cells.find(window.cell);
  if (root == nullptr || selection.root() != root || selection.def() == nullptr) {
    return NetOverlay{};
  }
  detail::NetPaint net(*editor.technology);
  search_selection(*root, *selection.def(), options, net);
  return OverlayWriter(mesh, window.cut_box).read(net);
}

}  // namespace hefesto::view3d
