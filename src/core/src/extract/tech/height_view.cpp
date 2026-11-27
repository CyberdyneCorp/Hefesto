// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// HeightView as a view of ExtStyle (openspec/changes/m7-extraction/
// design.md D7, D20, tasks.md 4.7; M10's openspec/changes/m10-3d-view/
// design.md D5): from_section reads the style with the one reader
// (style_reader.cpp), reporting the height subset, and converts it with
// ExtStyle's micron conversion; rescaled() is ExtStyle::rescale, the one
// grid-rescale path.
//
// Owner: EXTTECH (M7 design D7); M10's TECH wrote the first height subset.

#include "hefesto/extract/tech/height_view.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "hefesto/extract/tech/style.hpp"
#include "hefesto/mask/style/style.hpp"
#include "rule_context.hpp"
#include "style_reader.hpp"

namespace hefesto::extract {

namespace {

// The dscale of a tech load: the first cifoutput style as read is current.
float load_dscale(const tech::Technology& tech) {
  const std::optional<mask::OutputStyle> first = mask::read_first_output_style(tech);
  return HeightView::microns_per_lambda(first ? &*first : nullptr);
}

HeightView read(const tech::RawSection& section, const tech::Technology& tech, diag::DiagnosticSink& sink,
                std::optional<std::string_view> style, float dscale) {
  detail::ReadOptions options;
  options.style = style;
  options.report = detail::Subset::heights;
  std::optional<detail::StyleTables> tables = detail::read_style_tables(section, tech, sink, options);
  return tables ? HeightView::from_style(detail::micron_convert(tables->style, dscale)) : HeightView{};
}

}  // namespace

float HeightView::microns_per_lambda(const mask::OutputStyle* style) noexcept {
  if (style == nullptr) {
    return 1.0F;
  }
  return static_cast<float>(10 * style->scale.scale_factor) / static_cast<float>(style->scale.expander * 1000);
}

HeightView HeightView::from_section(const tech::RawSection& section, const tech::Technology& tech,
                                    diag::DiagnosticSink& sink) {
  return read(section, tech, sink, std::nullopt, load_dscale(tech));
}

HeightView HeightView::from_section(const tech::RawSection& section, const tech::Technology& tech,
                                    diag::DiagnosticSink& sink, std::string_view style) {
  return read(section, tech, sink, style, load_dscale(tech));
}

HeightView HeightView::from_section(const tech::RawSection& section, const tech::Technology& tech,
                                    diag::DiagnosticSink& sink, std::optional<std::string_view> style,
                                    float microns_per_lambda) {
  return read(section, tech, sink, style, microns_per_lambda);
}

HeightView HeightView::from_technology(const tech::Technology& tech, diag::DiagnosticSink& sink) {
  const auto& sections = tech.raw_sections();
  const auto it = std::find_if(sections.rbegin(), sections.rend(),
                               [](const tech::RawSection& s) { return s.name == "extract"; });
  if (it == sections.rend()) {
    return HeightView{};
  }
  return from_section(*it, tech, sink);
}

HeightView HeightView::from_style(const ExtStyle& style) {
  StyleHeights out;
  out.style = style.name;
  out.plane_order = style.plane_order;
  out.plane_ordering = style.plane_ordering;
  out.units_microns = style.units_microns;
  out.units_per_lambda = style.units_per_lambda;
  out.distance_scale = style.distance_scale;
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    const bool from_line = style.height_lines.test(static_cast<tech::TileType>(t));
    out.heights[t] = TypeHeight{style.heights[t].height, style.heights[t].thickness,
                                from_line ? HeightSource::height_line : HeightSource::defaulted};
  }
  return HeightView{std::move(out)};
}

// ExtStyle::rescale on the view's heights and units-per-lambda.
HeightView HeightView::rescaled(std::int32_t scale_n, std::int32_t scale_d) const {
  if (!current_) {
    return *this;
  }
  ExtStyle style;
  style.units_per_lambda = current_->units_per_lambda;
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    style.heights[t] = TypeHeightThickness{current_->heights[t].height, current_->heights[t].thickness};
  }
  style = style.rescale(scale_n, scale_d, compat::CompatSettings{});
  StyleHeights out = *current_;
  out.units_per_lambda = style.units_per_lambda;
  for (std::size_t t = 0; t < tech::kMaxTileTypes; ++t) {
    out.heights[t].height = style.heights[t].height;
    out.heights[t].thickness = style.heights[t].thickness;
  }
  return HeightView{std::move(out)};
}

}  // namespace hefesto::extract
