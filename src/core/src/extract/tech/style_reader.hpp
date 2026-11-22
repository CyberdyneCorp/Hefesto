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
// M10 TECH extends the same pass with the 3D height subset that HeightView
// exposes ("Units, lambda, cscale and rscale" (`units`, `lambda`), "Plane
// ordering", "Layer height and thickness defaults", "Micron conversion" of
// heights; openspec/changes/m10-3d-view/design.md D5): one reader, two
// views. Each view reports only the messages of its own subset, so reading
// both never reports a line twice.
//
// Owner: EXTSUB (openspec/changes/m6-lef-def-oasis/design.md D10); the
// height subset: TECH (openspec/changes/m10-3d-view/design.md D3).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/tech/height_view.hpp"
#include "hefesto/extract/tech/style.hpp"
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

/// Which messages a pass reports: one view's subset, or every message of
/// the full reader (load_styles; EXTTECH's handlers run only then).
enum class Subset : std::uint8_t { types, heights, all };

struct ReadOptions {
  std::optional<std::string_view> style;  // nullopt: the first declared style
  Subset report = Subset::types;
  /// dscale of "Micron conversion": µm per lambda of the current cifoutput
  /// style at the unscaled grid (1.0 without one).
  float microns_per_lambda = 1.0F;
  /// The magic-compat setting of a full read (Subset::all).
  bool magic_compat = true;
};

/// Both tables of one style, from one pass.
struct StyleTables {
  StyleTypes types;
  StyleHeights heights;
  /// A full read only (Subset::all): the parsed style before micron
  /// conversion (its active types before the non-electrical ones are
  /// removed) and the types its lines mention, connected types included.
  ExtStyle style;
  tech::TypeMask mentioned;
};

/// The pass behind read_style and HeightView; nullopt as read_style.
[[nodiscard]] std::optional<StyleTables> read_style_tables(const tech::RawSection& section,
                                                           const tech::Technology& tech, diag::DiagnosticSink& sink,
                                                           const ReadOptions& options);

}  // namespace hefesto::extract::detail
