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
// reader: its pass also parses `lambda`, `units`, `planeorder`,
// `noplaneordering` and `height`, runs the end-of-style plane-order check
// with its warning, applies the defaults (t = 0.05, h = 0.1 * order) and
// divides h and t by dscale with `units microns` (ExtTech.c:3796-3966);
// after `noplaneordering` none of the three runs, as in Magic
// (ExtTech.c:3801-3802).
// Every other keyword stays skipped silently, so a full PDK extract section
// loads with no diagnostic M7's reader would not also give. M7 re-sources
// the view from its full reader; the interface stays.
//
// Messages: a HeightView read reports only the height subset's messages
// (EXTRACT-STYLE-003..006) of the selected style, to the sink it is given;
// StyleView reports the others. Magic prints them when the technology
// loads; the caller decides when to read (and may pass a NullSink).
//
// Units: from_section gives h and t in lambda (the unscaled grid, as
// ExtTechFinal leaves them); rescaled() applies ExtTechScale when the
// internal grid differs from lambda (tech load with a lambda ratio,
// `scalegrid`, finer-grid reads).
//
// This header lines up the area contract's `extract::tech::ExtractStyleSubset`
// / `parse_extract_subset` with the landed M6 naming (namespace
// hefesto::extract, `from_section` / `from_technology`, as StyleView); see
// design.md "As-landed scaffold contracts".
//
// Owner: SCAFFOLD (this header, M10 task 2.3); TECH fills in the
// implementation in src/core/src/extract/tech/ (design D3, tasks 3.1, 3.2).

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

namespace hefesto::mask {
class OutputStyle;
}

namespace hefesto::extract {

/// Where a type's height and thickness come from.
enum class HeightSource : std::uint8_t {
  /// No `height` line, or one whose height or thickness was 0 and was
  /// replaced by the default (h = 0.1 * plane order, t = 0.05); after
  /// `noplaneordering` no default applies and the value stays 0.
  defaulted,
  height_line,  // both values from the style's `height` line
};

/// The height and thickness of one tile type (Magic's `float` exts_height /
/// exts_thick), in lambda after from_section, in internal units after
/// rescaled().
struct TypeHeight {
  float height = 0.0F;
  float thickness = 0.0F;
  HeightSource source = HeightSource::defaulted;

  friend bool operator==(const TypeHeight&, const TypeHeight&) = default;
};

/// "Unset" in StyleHeights::plane_order (Magic's -1).
inline constexpr std::int32_t kNoPlaneOrder = -1;

/// The height tables of one extraction style.
struct StyleHeights {
  std::string style;  // the style's name
  /// exts_planeOrder: index = tech::PlaneId, value = the plane's order
  /// index (higher is farther from the substrate) after the end-of-style
  /// check; kNoPlaneOrder for a plane no `planeorder` line named when the
  /// check was skipped (`noplaneordering`) and for the built-in planes.
  std::vector<std::int32_t> plane_order;
  bool plane_ordering = true;       // false after `noplaneordering`
  bool units_microns = false;       // `units microns` / `units um` (else lambda)
  float units_per_lambda = 100.0F;  // `lambda` (exts_unitsPerLambda), rescaled with the grid
  float distance_scale = 1.0F;      // the dscale h and t were divided by (1 with lambda units)
  std::array<TypeHeight, tech::kMaxTileTypes> heights{};  // index = tech::TileType

  friend bool operator==(const StyleHeights&, const StyleHeights&) = default;
};

class HeightView {
 public:
  /// A view with no extraction style.
  HeightView() = default;
  explicit HeightView(StyleHeights current) : current_(std::move(current)) {}

  /// The style loaded with the technology (the first one declared;
  /// "default" when the section declares none), read from `section` (the
  /// technology's raw `extract` section) by the one extract reader. dscale
  /// comes from the technology's first cifoutput style as read (the
  /// current one at tech load).
  [[nodiscard]] static HeightView from_section(const tech::RawSection& section, const tech::Technology& tech,
                                               diag::DiagnosticSink& sink);
  /// The same for the style named `style`; no style when the section
  /// declares no style of that name.
  [[nodiscard]] static HeightView from_section(const tech::RawSection& section, const tech::Technology& tech,
                                               diag::DiagnosticSink& sink, std::string_view style);
  /// The same with an explicit style choice and dscale (µm per lambda,
  /// microns_per_lambda()), for a session whose current cifoutput style is
  /// not the first one.
  [[nodiscard]] static HeightView from_section(const tech::RawSection& section, const tech::Technology& tech,
                                               diag::DiagnosticSink& sink, std::optional<std::string_view> style,
                                               float microns_per_lambda);
  /// The technology's last `extract` section, or no style without one.
  [[nodiscard]] static HeightView from_technology(const tech::Technology& tech, diag::DiagnosticSink& sink);

  /// CIFGetOutputScale(1000) of `style` at the unscaled grid: 10 * scale
  /// factor / (expander * 1000) in `float`; 1.0 without a style.
  [[nodiscard]] static float microns_per_lambda(const mask::OutputStyle* style) noexcept;

  /// ExtTechScale(scale_n, scale_d) (ExtTech.c:4090-4093): h and t
  /// multiplied by scale_d and then divided by scale_n in `float`, and
  /// units-per-lambda multiplied by scale_n / scale_d: the internal unit
  /// became scale_n / scale_d lambda (the arguments Magic passes after a
  /// tech load, DBLambda[0] and DBLambda[1], and after `scalegrid`, as
  /// drc::rescale). Unchanged when either is not positive or they are equal.
  [[nodiscard]] HeightView rescaled(std::int32_t scale_n, std::int32_t scale_d) const;

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
