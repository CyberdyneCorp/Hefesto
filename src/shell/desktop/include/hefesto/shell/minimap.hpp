// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The minimap's layout thumbnail kept between edits, so that an edit
// recomposites only the thumbnail cells under the changed area (the M1 gap
// "incremental rebuild", carried from M2 to M4: "Per-area minimap update",
// openspec/changes/m4-scripting/design.md D17).
//
// The thumbnail is the one minimap_thumbnail() (canvas_overlay.hpp)
// describes: the content rectangle of minimap_geometry() cut into cells of
// `cell_px` pixels, each cell compositing the PDK colors of the layers that
// cover it in redisplay order. Compositing is order dependent, so an update
// resets the dirty cells and composites them again from a scene holding
// every rectangle that touches them (the canvas builds that partial scene
// with display::build_scene over dirty_area(); everything outside the dirty
// cells is kept). The result equals a full rebuild over the edited scene.
//
// Toolkit-independent; unit-tested in tests/unit/shell/minimap_test.cpp.
// Owner: CARRY (openspec/changes/m4-scripting/design.md D2).

#pragma once

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/display/scene.hpp"
#include "hefesto/shell/canvas_overlay.hpp"
#include "hefesto/shell/theme_tokens.hpp"

namespace hefesto::shell {

/// Where a thumbnail is drawn: the world bounds it fits and the minimap's
/// size in pixels (see minimap_thumbnail()).
struct MinimapFrame {
  db::Rect bounds;
  Rgba background;
  double width = 0.0;
  double height = 0.0;
  double cell_px = 2.0;
  double padding = 12.0;

  friend bool operator==(const MinimapFrame& a, const MinimapFrame& b) noexcept {
    return a.bounds == b.bounds && a.background.r == b.background.r && a.background.g == b.background.g &&
           a.background.b == b.background.b && a.width == b.width && a.height == b.height &&
           a.cell_px == b.cell_px && a.padding == b.padding;
  }
};

/// A range of thumbnail cells, [col0, col1) × [row0, row1), rows from the top.
struct MinimapCells {
  std::size_t col0 = 0;
  std::size_t col1 = 0;
  std::size_t row0 = 0;
  std::size_t row1 = 0;

  [[nodiscard]] bool empty() const noexcept { return col0 >= col1 || row0 >= row1; }
  friend bool operator==(const MinimapCells&, const MinimapCells&) = default;
};

class MinimapThumbnail {
 public:
  /// Composites every cell from `scene` (a full rebuild).
  void rebuild(const display::LayoutScene& scene, const MinimapFrame& frame);

  /// True when a thumbnail is built for `frame`: an update can follow. A
  /// change of the bounds (the layout grew) or of the size needs a rebuild.
  [[nodiscard]] bool built_for(const MinimapFrame& frame) const noexcept { return built_ && frame_ == frame; }

  /// The cells an edit of `area` (root coordinates) changes: every cell
  /// whose world extent touches it; empty when nothing is built or `area`
  /// misses the thumbnail.
  [[nodiscard]] MinimapCells dirty_cells(const db::Rect& area) const;

  /// The world rectangle the partial scene of an update must cover: the
  /// extent of dirty_cells(area), rounded outward to whole units. Every
  /// rectangle that can paint one of those cells touches it. std::nullopt
  /// when there is nothing to update.
  [[nodiscard]] std::optional<db::Rect> dirty_area(const db::Rect& area) const;

  /// Resets the cells of dirty_cells(area) and composites them again from
  /// `partial`, a scene of everything touching dirty_area(area) (whole
  /// rectangles, not cut at the area's edge). Cells outside are kept.
  void update(const display::LayoutScene& partial, const db::Rect& area);

  /// The thumbnail as fills (adjacent cells of one color on a row merged),
  /// as minimap_thumbnail() returns them.
  [[nodiscard]] std::vector<MinimapFill> fills() const;

  /// Number of cells composited by the last rebuild() or update() (tests:
  /// an update touches only the dirty cells).
  [[nodiscard]] std::size_t last_composited() const noexcept { return last_composited_; }

 private:
  struct Tint {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 0.0F;
  };

  [[nodiscard]] std::pair<std::size_t, std::size_t> span(double lo, double hi, std::size_t count) const;
  [[nodiscard]] MinimapCells rect_cells(const db::Rect& r) const;
  void composite(const display::LayoutScene& scene, const MinimapCells& clip);
  void paint(const db::Rect& r, const Tint& tint, const MinimapCells& clip);
  void edges(const db::Rect& r, const Tint& tint, const MinimapCells& clip);
  static void blend(Rgba& c, const Tint& t);
  static Tint layer_tint(const display::SceneLayer& layer);
  static Tint style_tint(const display::ResolvedStyle& style, float alpha);

  MinimapFrame frame_;
  bool built_ = false;
  PixelRect content_;
  double scale_ = 1.0;  // content pixels per world unit
  std::size_t cols_ = 0;
  std::size_t rows_ = 0;
  std::vector<Rgba> cells_;  // a == 0: nothing painted
  std::size_t last_composited_ = 0;
};

}  // namespace hefesto::shell
