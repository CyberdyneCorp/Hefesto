// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Outline fonts for labels (capability font-labels): Type 3 PostScript
// loading, glyph contours, Bezier flattening, font-label geometry and the
// glyph polygons the scene draws.
// (Magic ref: database/DBlabel.c:1188-1824)
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/db/geometry.hpp"
#include "hefesto/db/label.hpp"
#include "hefesto/diag/outcome.hpp"

namespace hefesto::display {

struct GlyphOutline {
  db::Point advance;                             // font units (setcachedevice wx wy)
  db::Rect bbox;                                 // font units (setcachedevice llx lly urx ury)
  std::vector<std::vector<db::Point>> contours;  // closed, flattened, nonzero fill
};

/// Number of glyph slots: codes 32..126, then `.notdef` at slot 95 (code 127).
inline constexpr std::size_t kGlyphSlots = 96;

struct OutlineFont {
  std::string name;
  db::Rect extents;  // font units; top = trunc(FontBBox ury * load scale)
  std::array<GlyphOutline, kGlyphSlots> glyphs;

  /// The glyph of character `c`; codes below 32 or above 126 use `.notdef`.
  [[nodiscard]] const GlyphOutline& glyph(char c) const noexcept;
};

/// "Outline font file loading" with load scale `scale` (default 1.0).
[[nodiscard]] diag::Outcome<OutlineFont> parse_outline_font(std::string_view text, std::string_view file_name,
                                                            double scale = 1.0);

/// "Bezier flattening": the points that replace the curve P0 P1 P2 P3
/// (B(k/6) for k = 1..5 without repeats of the previous point, then P3).
[[nodiscard]] std::vector<db::Point> flatten_bezier(db::Point p0, db::Point p1, db::Point p2, db::Point p3);

/// The fonts loaded in a session; a label's `font` field indexes it.
struct FontRegistry {
  std::vector<std::shared_ptr<const OutlineFont>> fonts;

  /// DBNameToFont: the index of the font named `name` (case-insensitive),
  /// -1 for "default", -2 when unknown.
  [[nodiscard]] int find(std::string_view name) const;
  /// The font of a label, or nullptr for the default font / a bad index.
  [[nodiscard]] const OutlineFont* font_of(const db::Label& label) const noexcept;
};

/// "Font label geometry" (DBFontLabelSetBBox).
struct FontLabelGeometry {
  /// (xbot,ybot) (xtop,ybot) (xtop,ytop) (xbot,ytop) after rotation, in 1/8
  /// internal units relative to the centre of the label rectangle.
  std::array<db::Point, 4> corners;
  /// The rendered bounding box in internal (cell) units.
  db::Rect bbox;
  /// The rotation normalised once into [0, 360) (stored back by Magic).
  std::int32_t rotation = 0;
  /// The unrotated, justified, offset text box (1/8 units, relative to the
  /// rectangle centre) and the scaled minimum glyph bottom (for rendering).
  db::Rect text_box;
  std::int32_t baseline_drop = 0;
};

/// Computes the geometry of a font label. `magic_compat` selects the
/// bounding-box rounding test of Q-MISC-20.
[[nodiscard]] FontLabelGeometry font_label_geometry(const db::Label& label, const OutlineFont& font,
                                                    bool magic_compat = true);

/// The four corners (FontLabelGeometry::corners).
[[nodiscard]] std::array<db::Point, 4> font_label_corners(const db::Label& label, const OutlineFont& font);

/// A point in cell coordinates with sub-unit precision (glyph outlines).
struct PointF {
  double x = 0.0;
  double y = 0.0;
  friend bool operator==(const PointF&, const PointF&) = default;
};

/// "Font label rendering": the label's glyph contours as closed polygons in
/// cell coordinates, laid along the rotated baseline and turned by a further
/// 180 degrees when the rotation is in [90, 270) so the text reads upright.
[[nodiscard]] std::vector<std::vector<PointF>> font_label_polygons(const db::Label& label, const OutlineFont& font);

}  // namespace hefesto::display
