// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The one `extract` section reader (capability extract-tech-section;
// ExtTech.c ExtTechLine, ExtTechFinal): it walks the raw section as
// ExtTechLine does for one style, dispatching each line through the
// keyword table of openspec/changes/m7-extraction/design.md D7, and builds
// the style's ExtStyle. StyleView and HeightView are views of that
// ExtStyle (M7 task 4.7); each view reports only the messages of its own
// subset, so reading both never reports a line twice, and load_styles
// reports them all.
//
// Owner: EXTTECH (openspec/changes/m7-extraction/design.md D7), on the
// reader M6's EXTSUB (archive/2026-11-22-m6-lef-def-oasis/design.md D10)
// and M10's TECH (m10-3d-view/design.md D3, D5) started.

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/extract/tech/style.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::extract::detail {

/// The style names a section declares, in declaration order (variants
/// expanded), as ExtTechLine records them.
[[nodiscard]] std::vector<std::string> declared_styles(const tech::RawSection& section);

/// Which messages a pass reports: one view's subset, or every message of
/// the full reader (load_styles). Every pass runs every keyword handler;
/// the subset only filters the messages.
enum class Subset : std::uint8_t { types, heights, all };

struct ReadOptions {
  std::optional<std::string_view> style;  // nullopt: the first declared style
  Subset report = Subset::types;
  /// The magic-compat setting of the read.
  bool magic_compat = true;
};

/// One style, parsed.
struct StyleTables {
  /// The parsed style before micron conversion, with its active types
  /// before the non-electrical ones are removed (apply_non_electrical).
  ExtStyle style;
  /// The types its lines mention, connected types included
  /// (allExtractTypes of this style).
  tech::TypeMask mentioned;
};

/// The pass behind StyleView, HeightView and load_styles: ExtTechLine for
/// `options.style` (the first declared style when nullopt; "default" with
/// the defaults when the section declares none), then the parse half of
/// extTechFinalStyle. Nullopt when `options.style` is given and not
/// declared.
[[nodiscard]] std::optional<StyleTables> read_style_tables(const tech::RawSection& section,
                                                           const tech::Technology& tech, diag::DiagnosticSink& sink,
                                                           const ReadOptions& options);

/// ExtTechFinal's non-electrical set: the user types below `user_type_end`
/// that `mentioned` leaves out, recorded in `style.non_electrical` and
/// removed from its active types.
void apply_non_electrical(ExtStyle& style, const tech::TypeMask& mentioned, std::size_t user_type_end);

}  // namespace hefesto::extract::detail
