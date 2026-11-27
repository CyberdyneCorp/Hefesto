// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Private parts of the `plot` section reader (plot/tech/plot_tech.hpp):
// the per-style line handlers the state machine of plot_tech.cpp calls.
// Ported from Magic VLSI 8.3.694 plot/plotPS.c (PlotPSTechLine) and
// plot/plotPNM.c (PlotPNMTechInit, PlotPNMTechLine, PlotPNMSetDefaults,
// PlotPNMTechFinal, PlotLoadColormap, PlotLoadStyles, PNMColorBlend,
// PNMColorIndexAndBlend), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: PLOTTECH.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/plot/tech/plot_tech.hpp"
#include "hefesto/tech/technology.hpp"
#include "tech/build_state.hpp"

namespace hefesto::plot::detail {

/// PNMColorBlend: per channel v = P - 127 + floor(C / 2), clamped below at
/// 0 (`have` is C, `put` is P).
[[nodiscard]] Rgb blend(Rgb have, Rgb put) noexcept;

/// One line of a `postscript` block (PlotPSTechLine): prepends to the
/// lists of `out`, reports through `rep`.
void postscript_line(std::span<const std::string> tokens, const tech::Technology& tech, tech::Reporter& rep,
                     PlotTech& out);

/// A display style as PlotPNMTechLine reads it from GrStyleTable: its long
/// name (absent: never found by name), write mask and colour-map index.
struct DisplayStyleRef {
  std::optional<std::string> name;
  std::uint32_t wmask = 0;
  int color = 0;
};

/// The display style set that `default`, `draw <type>` and `draw <type>
/// <style>` read: Magic's GrStyleTable, DBWStyleToTypes and colour map.
struct DisplayStyles {
  std::vector<DisplayStyleRef> table;   // every style, table index order
  std::size_t first_layout = 0;         // TECHBEGINSTYLES
  std::size_t layout_count = 0;         // DBWNumStyles
  std::vector<tech::TypeMask> layout_types;  // DBWStyleToTypes(j), j < layout_count
  std::vector<Rgb> colors;              // the display colour map (GrGetColor)
};

/// A style of a plot-only dstyle file (Magic's Dstyles entry).
struct PnmStyle {
  std::string name;  // long name
  std::uint32_t wmask = 0;
  Rgb color;
};

/// The `color` file parse (PlotLoadColormap over fgets chunks of 256):
/// at most 128 `r g b` lines, `#` lines and blank lines skipped. On a
/// format error `ok` is false and `colors` holds the entries read before it
/// (Magic keeps them).
struct ColormapParse {
  std::vector<Rgb> colors;
  bool ok = true;
};
[[nodiscard]] ColormapParse parse_pnm_colormap(std::string_view text);

/// The `dstyle` file parse (PlotLoadStyles): the `display_styles` lines
/// `ord mask color outline fill stipple shortname longname`, at most
/// `max_styles` of them. A colour index inside `colormap` takes that
/// colour, any other is looked up in `display_colors` (black outside it).
/// On a format error `ok` is false and `styles` holds the entries read
/// before it.
struct DstyleParse {
  std::vector<PnmStyle> styles;
  bool ok = true;
};
[[nodiscard]] DstyleParse parse_pnm_dstyles(std::string_view text, std::span<const Rgb> colormap,
                                            std::span<const Rgb> display_colors, std::size_t max_styles);

/// The largest `color` or `dstyle` file read (Q-MISC-56, both modes): only
/// 128 colour lines or one line per display style are used, so a larger
/// file is a format error and is never read past this size.
inline constexpr std::size_t kMaxPlotFileBytes = std::size_t{1} << 20U;

/// The bundled copy of `name` (assets/plot/: Magic's scmos display and
/// colour files, unmodified, embedded at build time), or nullopt.
[[nodiscard]] std::optional<std::string_view> bundled_file(std::string_view name) noexcept;

/// The display styles for the PNM colours of `tech` (design D6): the
/// session's set when there is one, else the `7bit` class of the
/// technology's style type from `source.search_dirs`, else the bundled
/// `mos.7bit.dstyle` and `mos.7bit.std.cmap` (style type `mos` only), else
/// an empty table.
[[nodiscard]] DisplayStyles display_styles_for(const tech::Technology& tech, const StyleSource& source);

/// The `style pnm` handler (PlotPNMTechInit, PlotPNMTechLine,
/// PlotPNMTechFinal).
class PnmHandler {
 public:
  PnmHandler(const tech::Technology& tech, const StyleSource& source, tech::Reporter& rep);

  void line(std::span<const std::string> tokens);
  /// PlotPNMTechFinal: drops the plot-only tables and applies `default`
  /// when no type received a colour; fills `out.pnm_paint` and
  /// `out.init_failed`.
  void finish(PlotTech& out);

 private:
  void load_colormap(std::span<const std::string> tokens);
  void load_dstyles(std::span<const std::string> tokens);
  void set_defaults();
  void draw_default(tech::TileType type);
  void draw_with_style(const std::string& type_name, const std::string& style_name);
  void map(std::span<const std::string> tokens);
  [[nodiscard]] std::optional<tech::TileType> user_type(std::string_view name) const;
  [[nodiscard]] Rgb index_color(int index) const;
  [[nodiscard]] PnmPaint index_and_blend(PnmPaint have, const DisplayStyleRef& style) const;
  [[nodiscard]] const DisplayStyles& display();
  [[nodiscard]] std::optional<std::string> find_file(const std::string& name) const;
  void plain_error(std::string_view code, std::string text);

  const tech::Technology& tech_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  const StyleSource& source_;     // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  tech::Reporter& rep_;           // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
  std::vector<PnmPaint> paint_;   // PaintStyles, index = tile type below user_type_end()
  std::vector<Rgb> colormap_;     // PNMcolors (plot-only, `color`)
  std::vector<PnmStyle> dstyles_;  // Dstyles (plot-only, `dstyle`)
  std::optional<DisplayStyles> display_;
  bool init_failed_ = false;
};

}  // namespace hefesto::plot::detail
