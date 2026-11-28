// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// StyleView as a view of ExtStyle (openspec/changes/m7-extraction/design.md
// D7, D20, tasks.md 4.7; M6's openspec/changes/archive/2026-11-22-m6-lef-
// def-oasis/design.md D4, D10): from_section reads the style with the one
// reader, reporting the types subset, and takes the sets from the parsed
// style.
// Owner: EXTTECH (M7 design D7); M6's EXTSUB wrote the first subset.

#include "hefesto/extract/tech/style_view.hpp"

#include <algorithm>
#include <utility>

#include "hefesto/extract/tech/style.hpp"
#include "style_reader.hpp"

namespace hefesto::extract {

namespace {

StyleView read(const tech::RawSection& section, const tech::Technology& tech, diag::DiagnosticSink& sink,
               std::optional<std::string_view> style) {
  detail::ReadOptions options;
  options.style = style;
  options.report = detail::Subset::types;
  std::optional<detail::StyleTables> tables = detail::read_style_tables(section, tech, sink, options);
  if (!tables) {
    return StyleView{};
  }
  detail::apply_non_electrical(tables->style, tables->mentioned, tech.user_type_end());
  return StyleView::from_style(tables->style);
}

}  // namespace

StyleView StyleView::from_section(const tech::RawSection& section, const tech::Technology& tech,
                                  diag::DiagnosticSink& sink) {
  return read(section, tech, sink, std::nullopt);
}

StyleView StyleView::from_section(const tech::RawSection& section, const tech::Technology& tech,
                                  diag::DiagnosticSink& sink, std::string_view style) {
  return read(section, tech, sink, style);
}

StyleView StyleView::from_technology(const tech::Technology& tech, diag::DiagnosticSink& sink) {
  const auto& sections = tech.raw_sections();
  const auto it = std::find_if(sections.rbegin(), sections.rend(),
                               [](const tech::RawSection& s) { return s.name == "extract"; });
  if (it == sections.rend()) {
    return StyleView{};
  }
  return from_section(*it, tech, sink);
}

StyleView StyleView::from_style(const ExtStyle& style) {
  StyleTypes types;
  types.style = style.name;
  types.gate = style.gate_types();
  types.diff = style.diff_types();
  types.active = style.active_types();
  types.substrate = style.substrate_types();
  return StyleView{std::move(types)};
}

}  // namespace hefesto::extract
