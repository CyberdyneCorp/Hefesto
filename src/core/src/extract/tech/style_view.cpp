// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// StyleView over the extraction-style subset of the `extract` section
// reader (openspec/changes/archive/2026-11-22-m6-lef-def-oasis/design.md D4, D10).
// Owner: EXTSUB (design D2).

#include "hefesto/extract/tech/style_view.hpp"

#include <algorithm>
#include <utility>

#include "style_reader.hpp"

namespace hefesto::extract {

namespace {

StyleView view_of(std::optional<StyleTypes> types) {
  return types ? StyleView{std::move(*types)} : StyleView{};
}

}  // namespace

StyleView StyleView::from_section(const tech::RawSection& section, const tech::Technology& tech,
                                  diag::DiagnosticSink& sink) {
  return view_of(detail::read_style(section, tech, sink, std::nullopt));
}

StyleView StyleView::from_section(const tech::RawSection& section, const tech::Technology& tech,
                                  diag::DiagnosticSink& sink, std::string_view style) {
  return view_of(detail::read_style(section, tech, sink, style));
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

}  // namespace hefesto::extract
