// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// LayerStack (view-3d "Layer height and thickness for the layout-layer
// view", "Render statement in the cifoutput section"; openspec/changes/
// m10-3d-view/design.md D4, D5; Q-MISC-72 compat, Q-MISC-77 fix): the
// drawable layers of one 3D window in drawing order, each a (layer,
// display style) pair, with its height and thickness and where they came
// from.
//
// - Layout-layer mode: one entry per (tile type, display style) pair, in
//   style-table order and, within a style, in type order (Magic's redisplay
//   searches each style's types together, W3Dmain.c:1692-1702); a type in
//   two styles gives two entries (Q-MISC-77). h and t from the extract
//   style (extract::HeightView, already rescaled to internal units).
// - Mask-layer mode: one entry per mask layer of the current output style,
//   in style order, with its `render` style, height and thickness as stored
//   (layers without `render`: the pale-highlight style, h = t = 0).
//
// The compat branch point of Q-MISC-72 is draw_height() alone; the
// physical µm values (cross-sections, export, the layer-stack panel,
// overlays) come from physical_height_um(), which never depends on the
// compat setting. A layer whose values have no source (a mask layer
// without a `render` line, or any layer when there is no extract style)
// has no physical height (nullopt); missing_height_warning() gives the
// VIEW3D-STACK-001 warning that names the mask layers.
//
// Owner: SCAFFOLD (this header, M10 task 2.3); TECH fills in
// src/core/src/view3d/stack/ (design D3, tasks 3.3, 3.4).

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/db/plane.hpp"
#include "hefesto/diag/diagnostic.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/mask/style/style.hpp"
#include "hefesto/tech/technology.hpp"
#include "hefesto/tech/types.hpp"
#include "hefesto/view3d/mode.hpp"

namespace hefesto::view3d {

/// The display style table index of Magic's STYLE_PALEHIGHLIGHTS, the
/// style of a mask layer without `render` (CIFtech.c:875).
inline constexpr std::size_t kPaleHighlightStyle = 3;

/// The position in the technology style table that Magic stores for a
/// render style (cl_renderStyle, what `render name` returns): the table
/// index minus the fixed slots (negative for a fixed style: -49 for the
/// pale-highlight default).
[[nodiscard]] constexpr std::int32_t render_style_position(std::size_t table_index) noexcept {
  return static_cast<std::int32_t>(table_index) - static_cast<std::int32_t>(display::kFixedStyleCount);
}

/// Where a layer's height and thickness come from.
enum class ValueSource : std::uint8_t {
  extract,          // the extract style's `height` line
  extract_default,  // the extract style's defaults (h = 0.1 * order, t = 0.05)
  render,           // the output style's `render` statement
  none,             // no extract style, or a mask layer without `render`: h = t = 0
};

/// A height and thickness as the view draws them (z_top = -h * s_z,
/// z_bot = z_top - t * s_z; view-3d "Rendered geometry").
struct DrawHeight {
  float height = 0.0F;
  float thickness = 0.0F;

  friend bool operator==(const DrawHeight&, const DrawHeight&) = default;
};

/// A physical height and thickness in µm.
struct PhysicalHeight {
  double height_um = 0.0;
  double thickness_um = 0.0;

  friend bool operator==(const PhysicalHeight&, const PhysicalHeight&) = default;
};

/// One drawable (layer, display style) pair.
struct StackLayer {
  std::string name;                       // tile type name, or mask layer name
  std::optional<tech::TileType> type;     // layout-layer mode
  std::optional<std::size_t> mask_layer;  // mask-layer mode: index in OutputStyle::layers
  std::size_t style = 0;                  // display style table index
  float height = 0.0F;                    // as stored: internal units, or `render` value
  float thickness = 0.0F;
  ValueSource source = ValueSource::none;

  friend bool operator==(const StackLayer&, const StackLayer&) = default;
};

class LayerStack {
 public:
  LayerStack() = default;
  /// `microns_per_unit`: µm per internal unit (layout-layer mode), or the
  /// output style's cs_scaleFactor / (100 * cs_expander) (mask-layer mode).
  LayerStack(Mode mode, std::vector<StackLayer> layers, bool magic_compat, double microns_per_unit)
      : mode_(mode), layers_(std::move(layers)), magic_compat_(magic_compat), microns_per_unit_(microns_per_unit) {}
  /// Layout-layer mode with the h and t of every tile type (index =
  /// tech::TileType), also of types that are in no display style.
  LayerStack(std::vector<StackLayer> layers, std::vector<DrawHeight> type_heights, bool magic_compat,
             double microns_per_unit)
      : mode_(Mode::layout_layer), layers_(std::move(layers)), type_heights_(std::move(type_heights)),
        magic_compat_(magic_compat), microns_per_unit_(microns_per_unit) {}

  [[nodiscard]] Mode mode() const noexcept { return mode_; }
  [[nodiscard]] std::span<const StackLayer> layers() const noexcept { return layers_; }
  [[nodiscard]] std::size_t size() const noexcept { return layers_.size(); }
  [[nodiscard]] bool magic_compat() const noexcept { return magic_compat_; }
  [[nodiscard]] double microns_per_unit() const noexcept { return microns_per_unit_; }

  /// The Q-MISC-72 branch point: layout-layer mode, h and t in internal
  /// units (both settings); mask-layer mode, `render` h and t as stored
  /// (magic-compat ON) or divided by µm per internal unit (OFF). A layer
  /// with no source, or an out-of-range id, draws flat (h = t = 0).
  [[nodiscard]] DrawHeight draw_height(LayerId layer) const;
  /// The physical height and thickness, independent of the compat setting:
  /// layout-layer mode h and t times µm per internal unit, mask-layer mode
  /// the `render` values (µm by PDK convention). Nullopt for a layer with
  /// ValueSource::none or an out-of-range id.
  [[nodiscard]] std::optional<PhysicalHeight> physical_height_um(LayerId layer) const;

  /// Layout-layer mode: the h and t a tile is drawn with, whatever style
  /// draws it: those of its type, or for a split tile those of its left
  /// type on both triangles (ExtGetZAxis, ExtTech.c:584-602); h = t = 0
  /// for space and in mask-layer mode.
  [[nodiscard]] DrawHeight tile_height(const db::TileView& tile) const;

  /// The first entry of `type` drawn with display style `style`.
  [[nodiscard]] std::optional<LayerId> find(tech::TileType type, std::size_t style) const;

 private:
  Mode mode_ = Mode::layout_layer;
  std::vector<StackLayer> layers_;
  std::vector<DrawHeight> type_heights_;  // layout-layer mode, index = tech::TileType
  bool magic_compat_ = true;
  double microns_per_unit_ = 0.0;
};

/// The layout-layer stack: every (tile type, display style) pair of
/// `styles` in style-table order, with h and t from `heights` (h = t = 0
/// and ValueSource::none when it has no style). `heights` must already be
/// in internal units (HeightView::rescaled). `microns_per_unit`: µm per
/// internal unit.
[[nodiscard]] LayerStack build_layout_stack(const tech::Technology& tech, const extract::HeightView& heights,
                                            const display::TypeStyleMap& styles, bool magic_compat,
                                            double microns_per_unit);

/// The mask-layer stack of `style` (the current output style, rescaled to
/// the internal grid). Each layer takes the last of its `render`
/// statements whose style reference resolves in `styles`
/// (StyleTable::find_layout_style: a style number or an exact long name);
/// each `render` line whose reference does not resolve is reported once
/// per build to `sink` (VIEW3D-STACK-002, Magic's "Error:  Bad render
/// style for CIF layer.", once per line as in Magic) and that statement is
/// ignored. Without a style table (headless) the
/// last statement's height and thickness apply with the pale-highlight
/// style.
[[nodiscard]] LayerStack build_mask_stack(const mask::OutputStyle& style, const display::StyleTable* styles,
                                          bool magic_compat, diag::DiagnosticSink& sink);

/// VIEW3D-STACK-001 for a mask-layer stack whose non-temporary mask layers
/// of `style` have no `render` height (named in style order); nullopt when
/// there are none or the stack is in layout-layer mode. Cross-sections and
/// export from mask-layer mode leave those layers out and emit it.
[[nodiscard]] std::optional<diag::Diagnostic> missing_height_warning(const LayerStack& stack,
                                                                     const mask::OutputStyle& style);

}  // namespace hefesto::view3d
