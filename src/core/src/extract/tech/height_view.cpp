// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// HeightView over the extract-tech-section reader (openspec/changes/
// m10-3d-view/design.md D5): the height tables of detail::read_style_tables
// (style_reader.cpp), the one reader StyleView also reads, and
// ExtTechScale's rescale of h and t (task 3.2).
//
// Owner: TECH (design D3).

#include "hefesto/extract/tech/height_view.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "hefesto/mask/style/style.hpp"
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
  options.microns_per_lambda = dscale;
  std::optional<detail::StyleTables> tables = detail::read_style_tables(section, tech, sink, options);
  return tables ? HeightView{std::move(tables->heights)} : HeightView{};
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

HeightView HeightView::rescaled(std::int32_t scale_n, std::int32_t scale_d) const {
  if (!current_ || scale_n <= 0 || scale_d <= 0 || scale_n == scale_d) {
    return *this;
  }
  StyleHeights out = *current_;
  const auto n = static_cast<float>(scale_n);
  const auto d = static_cast<float>(scale_d);
  out.units_per_lambda = out.units_per_lambda * n / d;
  for (TypeHeight& th : out.heights) {
    th.height *= d;
    th.height /= n;
    th.thickness *= d;
    th.thickness /= n;
  }
  return HeightView{std::move(out)};
}

}  // namespace hefesto::extract
