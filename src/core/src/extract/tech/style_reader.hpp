// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The extraction-style subset of the `extract` section reader (capability
// extract-tech-section "Extract section line grammar and keyword
// matching", "Extraction styles and variants", "Style defaults",
// "Substrate declaration", "Device declaration parsing" (class and types),
// "Non-electrical types", "Antenna keywords parse-level reference"
// (`tiedown`); ExtTech.c ExtTechLine, ExtTechFinal). It walks the raw
// section as ExtTechLine does for one style and keeps the type sets that
// StyleView exposes.
//
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/tech/style_view.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract::detail {

/// The style names a section declares, in declaration order (variants
/// expanded), as ExtTechLine records them.
[[nodiscard]] std::vector<std::string> declared_styles(const tech::RawSection& section);

/// The type sets of `style` (the first declared style when nullopt;
/// "default" with the defaults when the section declares none). Nullopt
/// when `style` is given and not declared.
[[nodiscard]] std::optional<StyleTypes> read_style(const tech::RawSection& section, const tech::Technology& tech,
                                                   diag::DiagnosticSink& sink,
                                                   const std::optional<std::string_view>& style);

}  // namespace hefesto::extract::detail
