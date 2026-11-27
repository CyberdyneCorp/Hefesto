// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `pnm` style lines of the `plot` section (capability plot-output "PNM
// style lines", "Platform mechanism of plot output"; m10-plot-route design
// D6). Keywords are matched by prefix (strncmp) in Magic's order: color,
// dstyle, default, draw, map; any other line is ignored. Fixed in both
// modes: a bare `map` line is reported and ignored (Q-MISC-50 (1)); type
// numbers outside the user types and colour indices outside the colour
// maps are never used to index a table (Magic reads out of bounds there);
// a `color` or `dstyle` file larger than kMaxPlotFileBytes is a format
// error and is read no further (Q-MISC-56).
//
// Ported from Magic VLSI 8.3.694 plot/plotPNM.c (PlotPNMTechInit,
// PlotPNMTechLine, PlotPNMSetDefaults, PlotPNMTechFinal, PNMColorBlend,
// PNMColorIndexAndBlend), Copyright (C) 1985, 1990 Regents of the
// University of California (see THIRD_PARTY_NOTICES.md).
//
// Owner: PLOTTECH.

#include <algorithm>
#include <string>
#include <utility>

#include "plot/tech/reader.hpp"

namespace hefesto::plot::detail {

namespace {

constexpr std::string_view kBareMap = "PLOT-TECH-004";
constexpr std::string_view kFileError = "PLOT-TECH-005";
constexpr std::string_view kUnknownName = "PLOT-TECH-006";

std::uint8_t clamp_channel(int value) noexcept {
  return value < 0 ? std::uint8_t{0} : static_cast<std::uint8_t>(value);
}

/// Magic computes in `short` and then stores `(unsigned char)r`.
std::uint8_t blend_channel(int have, int put) noexcept {
  return clamp_channel(static_cast<std::int16_t>(put - 127 + have / 2));
}

bool starts_with(std::string_view text, std::string_view prefix) noexcept {
  return text.substr(0, prefix.size()) == prefix;
}

bool is_direct_path(const std::string& name) {
  return starts_with(name, "/") || starts_with(name, "./") || starts_with(name, "../");
}

}  // namespace

Rgb blend(Rgb have, Rgb put) noexcept {
  return Rgb{blend_channel(have.r, put.r), blend_channel(have.g, put.g), blend_channel(have.b, put.b)};
}

PnmHandler::PnmHandler(const tech::Technology& tech, const StyleSource& source, tech::Reporter& rep)
    : tech_(tech), source_(source), rep_(rep), paint_(tech.user_type_end()) {}

void PnmHandler::line(std::span<const std::string> tokens) {
  const std::string& keyword = tokens.front();
  if (starts_with(keyword, "color")) {
    load_colormap(tokens);
  } else if (starts_with(keyword, "dstyle")) {
    load_dstyles(tokens);
  } else if (starts_with(keyword, "default")) {
    set_defaults();
  } else if (starts_with(keyword, "draw")) {
    if (tokens.size() == 2) {
      if (const auto type = user_type(tokens[1])) {
        draw_default(*type);
      }
    } else if (tokens.size() == 3) {
      draw_with_style(tokens[1], tokens[2]);
    }
  } else if (starts_with(keyword, "map")) {
    map(tokens);
  }
}

void PnmHandler::finish(PlotTech& out) {
  colormap_.clear();
  dstyles_.clear();
  // No `draw` or `map` gave a type a write mask: use the display styles.
  bool any = false;
  for (std::size_t t = 1; t < paint_.size() && !any; ++t) {
    any = paint_[t].wmask != 0;
  }
  if (!any) {
    set_defaults();
  }
  out.pnm_paint = paint_;
  out.init_failed = init_failed_;
}

std::optional<tech::TileType> PnmHandler::user_type(std::string_view name) const {
  const int type = tech::name_type(tech_.data(), name);
  if (type < 0 || static_cast<std::size_t>(type) >= paint_.size()) {
    return std::nullopt;
  }
  return static_cast<tech::TileType>(type);
}

const DisplayStyles& PnmHandler::display() {
  if (!display_) {
    display_ = display_styles_for(tech_, source_);
  }
  return *display_;
}

Rgb PnmHandler::index_color(int index) const {
  // PNMColorIndexAndBlend: the plot colour map when one is loaded and the
  // index is in it, else the display colour map (GrGetColor). Indices
  // outside both are black (Magic reads unset values or out of bounds).
  if (index >= 0 && static_cast<std::size_t>(index) < colormap_.size()) {
    return colormap_[static_cast<std::size_t>(index)];
  }
  if (display_ && index >= 0 && static_cast<std::size_t>(index) < display_->colors.size()) {
    return display_->colors[static_cast<std::size_t>(index)];
  }
  return Rgb{0, 0, 0};
}

PnmPaint PnmHandler::index_and_blend(PnmPaint have, const DisplayStyleRef& style) const {
  have.wmask |= style.wmask;
  have.color = blend(have.color, index_color(style.color));
  return have;
}

void PnmHandler::draw_default(tech::TileType type) {
  const DisplayStyles& styles = display();
  for (std::size_t j = 0; j < styles.layout_count; ++j) {
    if (styles.layout_types[j].test(type)) {
      paint_[type] = index_and_blend(paint_[type], styles.table[styles.first_layout + j]);
    }
  }
}

void PnmHandler::set_defaults() {
  for (std::size_t t = 1; t < paint_.size(); ++t) {
    draw_default(static_cast<tech::TileType>(t));
  }
}

void PnmHandler::draw_with_style(const std::string& type_name, const std::string& style_name) {
  const std::optional<tech::TileType> type = user_type(type_name);
  if (!type) {
    plain_error(kUnknownName, "Unknown magic layer \"" + type_name + "\" for PNM plot.");
    return;
  }
  const PnmPaint saved = paint_[*type];
  PnmPaint fresh;  // white, no write mask
  bool found = false;
  if (!dstyles_.empty()) {
    for (const PnmStyle& style : dstyles_) {
      if (style.name == style_name) {
        fresh.wmask |= style.wmask;
        fresh.color = blend(fresh.color, style.color);
        found = true;
      }
    }
  } else {
    const DisplayStyles& styles = display();
    const auto it = std::find_if(styles.table.begin(), styles.table.end(),
                                 [&](const DisplayStyleRef& s) { return s.name == style_name; });
    if (it != styles.table.end()) {
      fresh = index_and_blend(fresh, *it);
      found = true;
    } else {
      plain_error(kUnknownName, "Unknown drawing style \"" + style_name + "\" for PNM plot.");
    }
  }
  paint_[*type] = found ? fresh : saved;
}

void PnmHandler::map(std::span<const std::string> tokens) {
  if (tokens.size() < 2) {
    rep_.emit(kBareMap, "\"map\" lines need at least one type");  // Q-MISC-50 (1)
    return;
  }
  const std::optional<tech::TileType> target = user_type(tokens[1]);
  if (!target) {
    return;
  }
  for (std::size_t j = 2; j < tokens.size(); ++j) {
    if (const std::optional<tech::TileType> source = user_type(tokens[j])) {
      paint_[*target].wmask |= paint_[*source].wmask;
      paint_[*target].color = blend(paint_[*target].color, paint_[*source].color);
    }
  }
}

std::optional<std::string> PnmHandler::find_file(const std::string& name) const {
  if (source_.files != nullptr) {
    // One byte more than the limit tells an oversized file from one at it.
    if (is_direct_path(name)) {
      auto text = source_.files->read_at_most(name, kMaxPlotFileBytes + 1);
      return text.ok() ? std::optional{std::move(text).value()} : std::nullopt;
    }
    for (const platform::Path& dir : source_.search_dirs) {
      auto text = source_.files->read_at_most(dir / name, kMaxPlotFileBytes + 1);
      if (text.ok()) {
        return std::move(text).value();
      }
    }
  }
  if (const auto bundled = bundled_file(name)) {
    return std::string{*bundled};
  }
  return std::nullopt;
}

void PnmHandler::plain_error(std::string_view code, std::string text) {
  rep_.sink().emit(diag::make(code, std::move(text)));  // TxError: no line context
}

void PnmHandler::load_colormap(std::span<const std::string> tokens) {
  const std::string name =
      tokens.size() == 1 ? tech_.styles().style_family.substr(0, 100) + ".7bit.mraster.cmap" : tokens[1];
  const std::optional<std::string> text = find_file(name);
  if (!text) {
    plain_error(kFileError, "Couldn't open colormap file \"" + name + "\"");
    init_failed_ = true;
    return;
  }
  ColormapParse parsed = text->size() > kMaxPlotFileBytes ? ColormapParse{{}, false} : parse_pnm_colormap(*text);
  colormap_ = std::move(parsed.colors);
  if (!parsed.ok) {
    init_failed_ = true;
    plain_error(kFileError, "Format error in colormap file");
  }
}

void PnmHandler::load_dstyles(std::span<const std::string> tokens) {
  const std::string name =
      tokens.size() == 1 ? tech_.styles().style_family.substr(0, 100) + ".7bit.mraster_dstyle" : tokens[1];
  const std::optional<std::string> text = find_file(name);
  if (!text) {
    plain_error(kFileError, "PNM plot: Could not open display style file");
    init_failed_ = true;
    return;
  }
  const DisplayStyles& styles = display();
  DstyleParse parsed = text->size() > kMaxPlotFileBytes
                           ? DstyleParse{{}, false}
                           : parse_pnm_dstyles(*text, colormap_, styles.colors, styles.layout_count);
  dstyles_ = std::move(parsed.styles);
  if (!parsed.ok) {
    init_failed_ = true;
    plain_error(kFileError, "Format error in display style file");
  }
}

}  // namespace hefesto::plot::detail
