// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The files the `pnm` style reads (capability plot-output "PNM style lines",
// "Platform mechanism of plot output"; m10-plot-route design D6, Q-MISC-44):
// the plot-only colour map and display-style file of `color` and `dstyle`
// lines, and the display style set the `default` and `draw` colours come
// from (the session's, else the `7bit` class, else the bundled copies).
//
// Ported from Magic VLSI 8.3.694 plot/plotPNM.c (PlotLoadColormap,
// PlotLoadStyles), Copyright (C) 1985, 1990 Regents of the University of
// California (see THIRD_PARTY_NOTICES.md).
//
// Owner: PLOTTECH.

#include <algorithm>
#include <utility>

#include "display/c_scan.hpp"
#include "hefesto/display/styles.hpp"
#include "plot/tech/reader.hpp"

namespace hefesto::plot::detail {

namespace {

namespace cscan = display::cscan;

constexpr std::size_t kFgetsSize = 256;      // fgets(buf, 256, inp)
constexpr std::size_t kMaxPlotColors = 128;  // mallocMagic(128 * PIXELSZ)

/// (unsigned char) of a value sscanf stored into an int.
std::uint8_t low_byte(std::int64_t value) noexcept {
  return static_cast<std::uint8_t>(static_cast<std::uint64_t>(value) & 0xFFU);
}

std::optional<Rgb> scan_rgb(std::string_view line) {
  cscan::Scanner scan(line);
  const auto r = scan.integer(10);
  const auto g = r ? scan.integer(10) : std::nullopt;
  const auto b = g ? scan.integer(10) : std::nullopt;
  if (!b) {
    return std::nullopt;
  }
  return Rgb{low_byte(*r), low_byte(*g), low_byte(*b)};
}

/// "%d %d %d %d %40s %d %c %126s": the mask, colour and long name.
struct DstyleLine {
  std::int64_t mask = 0;
  std::int64_t color = 0;
  std::string name;
};

std::optional<DstyleLine> scan_dstyle_line(std::string_view line) {
  cscan::Scanner scan(line);
  const auto ord = scan.integer(10);
  const auto mask = ord ? scan.integer(10) : std::nullopt;
  const auto color = mask ? scan.integer(10) : std::nullopt;
  const auto outline = color ? scan.integer(10) : std::nullopt;
  const auto fill = outline ? scan.word(40) : std::nullopt;
  const auto stipple = fill ? scan.integer(10) : std::nullopt;
  if (!stipple) {
    return std::nullopt;
  }
  scan.skip_space();
  const auto short_name = scan.character();
  const auto long_name = short_name ? scan.word(126) : std::nullopt;
  if (!long_name) {
    return std::nullopt;
  }
  return DstyleLine{*mask, *color, *long_name};
}

Rgb lookup(std::span<const Rgb> colors, std::int64_t index) {
  if (index >= 0 && static_cast<std::uint64_t>(index) < colors.size()) {
    return colors[static_cast<std::size_t>(index)];
  }
  return Rgb{0, 0, 0};  // GrGetColor leaves the values unset outside the map
}

DisplayStyles from_style_set(const display::StyleSet& set) {
  DisplayStyles out;
  for (const display::ColorEntry& c : set.colors.entries) {
    out.colors.push_back(Rgb{c.r, c.g, c.b});
  }
  for (const display::DisplayStyle& s : set.table.styles) {
    out.table.push_back(DisplayStyleRef{s.defined ? s.long_name : std::nullopt, static_cast<std::uint32_t>(s.mask),
                                        s.color});
  }
  out.first_layout = display::kFixedStyleCount;
  const std::size_t available = out.table.size() > out.first_layout ? out.table.size() - out.first_layout : 0;
  out.layout_count = std::min(set.table.layout_count, available);
  for (std::size_t j = 0; j < out.layout_count; ++j) {
    out.layout_types.push_back(set.types.types_of(out.first_layout + j));
  }
  return out;
}

/// The bundled `mos.7bit.dstyle` and `mos.7bit.std.cmap`, read as Magic's
/// null display reads them (GrLoadStyles, GrReadCMap), bound to `tech`'s
/// styles section.
std::optional<display::StyleSet> bundled_style_set(const tech::Technology& tech) {
  const auto cmap_text = bundled_file("mos.7bit.std.cmap");
  const auto dstyle_text = bundled_file("mos.7bit.dstyle");
  if (!cmap_text || !dstyle_text) {
    return std::nullopt;
  }
  const bool compat = tech.data().magic_compat;
  diag::NullSink quiet;
  auto map = display::parse_color_map(*cmap_text, "mos.7bit.std.cmap", compat);
  if (!map) {
    return std::nullopt;
  }
  display::StyleSet set;
  set.display_class = display::DisplayClass::bit7;
  set.colors = std::move(map).value();
  auto table = display::parse_dstyle(*dstyle_text, "mos.7bit.dstyle", set.colors, compat, quiet,
                                     display::display_plane_mask(display::DisplayClass::bit7), 5);
  if (!table) {
    return std::nullopt;
  }
  set.table = std::move(table).value();
  set.types = display::resolve_type_styles(tech, set.table, quiet);
  return set;
}

}  // namespace

ColormapParse parse_pnm_colormap(std::string_view text) {
  ColormapParse out;
  for (const std::string_view line : cscan::fgets_chunks(text, kFgetsSize)) {
    if (line.starts_with('#') || cscan::is_white(line, false)) {
      continue;
    }
    const std::optional<Rgb> color = out.colors.size() < kMaxPlotColors ? scan_rgb(line) : std::nullopt;
    if (!color) {
      out.ok = false;
      return out;
    }
    out.colors.push_back(*color);
  }
  return out;
}

namespace {

/// PlotLoadStyles' reading state: what the next non-comment line means.
class DstyleReader {
 public:
  DstyleReader(std::span<const Rgb> colormap, std::span<const Rgb> display_colors, std::size_t max_styles)
      : colormap_(colormap), display_colors_(display_colors), max_styles_(max_styles) {}

  /// Reads one fgets chunk; false when reading stops (an error or the
  /// table is full).
  bool line(std::string_view line) {
    if (line.starts_with('#')) {
      return true;
    }
    if (cscan::is_white(line, false)) {
      new_section_ = true;
      return true;
    }
    if (new_section_) {
      new_section_ = false;
      out_.ok = line.starts_with("display_styles");
      return out_.ok;
    }
    return style(line);
  }

  DstyleParse take() { return std::move(out_); }

 private:
  bool style(std::string_view line) {
    const std::optional<DstyleLine> parsed = scan_dstyle_line(line);
    if (!parsed || out_.styles.size() == max_styles_) {
      out_.ok = false;
      return false;
    }
    out_.styles.push_back(PnmStyle{parsed->name, static_cast<std::uint32_t>(parsed->mask), color(parsed->color)});
    return out_.styles.size() < max_styles_;
  }

  /// The plot colour map when the index is in it, else GrGetColor.
  [[nodiscard]] Rgb color(std::int64_t index) const {
    const bool in_plot_map = index >= 0 && static_cast<std::uint64_t>(index) < colormap_.size();
    return in_plot_map ? colormap_[static_cast<std::size_t>(index)] : lookup(display_colors_, index);
  }

  std::span<const Rgb> colormap_;
  std::span<const Rgb> display_colors_;
  std::size_t max_styles_;
  bool new_section_ = false;
  DstyleParse out_;
};

}  // namespace

DstyleParse parse_pnm_dstyles(std::string_view text, std::span<const Rgb> colormap,
                              std::span<const Rgb> display_colors, std::size_t max_styles) {
  DstyleReader reader(colormap, display_colors, max_styles);
  for (const std::string_view line : cscan::fgets_chunks(text, kFgetsSize)) {
    if (!reader.line(line)) {
      break;
    }
  }
  return reader.take();
}

DisplayStyles display_styles_for(const tech::Technology& tech, const StyleSource& source) {
  if (source.session != nullptr) {
    return from_style_set(*source.session);
  }
  const std::string& family = tech.styles().style_family;
  if (source.files != nullptr && !family.empty()) {
    display::StyleLoadOptions options;
    options.display_class = display::DisplayClass::bit7;  // Magic's null display (grMain.c)
    options.magic_compat = tech.data().magic_compat;
    options.library_dirs = source.search_dirs;
    diag::NullSink quiet;
    auto loaded = display::load_style_set(*source.files, tech, options, quiet);
    if (loaded.ok()) {
      return from_style_set(loaded.value());
    }
  }
  if (family == "mos") {
    if (const auto bundled = bundled_style_set(tech)) {
      return from_style_set(*bundled);
    }
  }
  return DisplayStyles{};
}

}  // namespace hefesto::plot::detail
