// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// HeightView (capability extract-tech-section, the subset the 3D
// layout-layer view reads; view-3d "Layer height and thickness for the
// layout-layer view"; openspec/changes/m10-3d-view/design.md D5, Open
// Question 4 decided (a)): the current extraction style's plane order and
// the height and thickness of every tile type, as a view over the tables of
// the one `extract-tech-section` reader that M6 landed for StyleView
// (src/core/src/extract/tech/style_reader.cpp). There is one extract
// reader: TECH extends that reader's pass with `scale`/`lambda`, `units`,
// `planeorder`, `noplaneordering`, `height`, the end-of-style plane-order
// check and its warnings, the defaults (t = 0.05, h = 0.1 * order) and the
// conversion by dscale with `units microns` (ExtTech.c:3962-3966), and
// sources this view from it; it never adds a second parser. Every other
// keyword stays skipped silently, so a full PDK extract section loads with
// no diagnostic M7's reader would not also give. M7 re-sources the view
// from its full reader; the interface stays.
//
// This header lines up the area contract's `extract::tech::ExtractStyleSubset`
// / `parse_extract_subset` with the landed M6 naming (namespace
// hefesto::extract, `from_section` / `from_technology`, as StyleView); see
// design.md "As-landed scaffold contracts".
//
// Owner: SCAFFOLD (this header, M10 task 2.3); TECH fills in the
// implementation in src/core/src/extract/tech/ (design D3, task 3.1).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::extract {

/// Where a type's height and thickness come from.
enum class HeightSource : std::uint8_t {
  defaulted,  // no `height` line: h = 0.1 * plane order, t = 0.05 (before conversion)
  height_line,  // the style's `height` line
};

/// The height and thickness of one tile type, in internal units after the
/// style's conversion (Magic's `float` exts_height / exts_thick).
struct TypeHeight {
  float height = 0.0F;
  float thickness = 0.0F;
  HeightSource source = HeightSource::defaulted;

  friend bool operator==(const TypeHeight&, const TypeHeight&) = default;
};

/// The height tables of one extraction style.
struct StyleHeights {
  std::string style;                       // the style's name
  std::vector<tech::PlaneId> plane_order;  // `planeorder`, else the default order
  bool plane_ordering = true;              // false after `noplaneordering`
  bool units_microns = false;              // `units microns` (else lambda)
  float distance_scale = 1.0F;             // dscale: `scale` / `lambda` with the grid
  std::array<TypeHeight, tech::kMaxTileTypes> heights{};  // index = tech::TileType
};

class HeightView {
 public:
  /// A view with no extraction style.
  HeightView() = default;
  explicit HeightView(StyleHeights current) : current_(std::move(current)) {}

  /// The style loaded with the technology (the first one declared;
  /// "default" when the section declares none), read from `section` (the
  /// technology's raw `extract` section) by the one extract reader.
  [[nodiscard]] static HeightView from_section(const tech::RawSection& section, const tech::Technology& tech,
                                               diag::DiagnosticSink& sink);
  /// The same for the style named `style`; no style when the section
  /// declares no style of that name.
  [[nodiscard]] static HeightView from_section(const tech::RawSection& section, const tech::Technology& tech,
                                               diag::DiagnosticSink& sink, std::string_view style);
  /// The technology's last `extract` section, or no style without one.
  [[nodiscard]] static HeightView from_technology(const tech::Technology& tech, diag::DiagnosticSink& sink);

  /// The view after the internal grid became n/d times finer (h and t
  /// rescaled as ExtTech.c:4090-4093; task 3.2).
  [[nodiscard]] HeightView rescaled(std::int32_t n, std::int32_t d) const;

  [[nodiscard]] bool has_style() const noexcept { return current_.has_value(); }
  [[nodiscard]] std::optional<std::string> style_name() const {
    return current_ ? std::optional<std::string>{current_->style} : std::nullopt;
  }
  /// The height and thickness of `type`; nullopt with no style.
  [[nodiscard]] std::optional<TypeHeight> height(tech::TileType type) const {
    if (!current_ || type >= current_->heights.size()) {
      return std::nullopt;
    }
    return current_->heights[type];
  }
  /// The whole table, when there is a style.
  [[nodiscard]] const StyleHeights* style() const noexcept { return current_ ? &*current_ : nullptr; }

 private:
  std::optional<StyleHeights> current_;
};

}  // namespace hefesto::extract
