// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// HeightView over the extract-tech-section reader (openspec/changes/
// m10-3d-view/design.md D5). Scaffold stubs (M10 task 2.3): every entry
// point answers "no extraction style", which the layer stack treats as
// h = t = 0 for every type (view-3d "Layer height and thickness for the
// layout-layer view": no extract style). TECH replaces them in task 3.1 by
// extending detail::read_style's pass (style_reader.cpp), never with a
// second parser.
//
// Owner: TECH (design D3).

#include "hefesto/extract/tech/height_view.hpp"

namespace hefesto::extract {

HeightView HeightView::from_section(const tech::RawSection& /*section*/, const tech::Technology& /*tech*/,
                                    diag::DiagnosticSink& /*sink*/) {
  return HeightView{};
}

HeightView HeightView::from_section(const tech::RawSection& /*section*/, const tech::Technology& /*tech*/,
                                    diag::DiagnosticSink& /*sink*/, std::string_view /*style*/) {
  return HeightView{};
}

HeightView HeightView::from_technology(const tech::Technology& /*tech*/, diag::DiagnosticSink& /*sink*/) {
  return HeightView{};
}

HeightView HeightView::rescaled(std::int32_t /*n*/, std::int32_t /*d*/) const {
  return *this;
}

}  // namespace hefesto::extract
