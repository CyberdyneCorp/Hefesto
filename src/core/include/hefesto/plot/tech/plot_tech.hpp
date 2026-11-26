// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `plot` technology section (capability plot-output "Technology plot
// section structure", "PostScript style lines", "PNM style lines", "Raster
// printer and legacy plot devices"; openspec/changes/m10-plot-route/design.md
// D4, D6, "Area contracts" PLOTTECH).
//
// tech::Technology keeps the parsed section (Technology::plot()); the loader
// calls read_plot_section once per load, after the types are final, on the
// last `plot` section of the file (a repeated section is read again from the
// start, as Magic's PlotTechInit runs again). Only the `postscript` and `pnm`
// styles are kept; `gremlin`, `versatec`, `colorversatec` and `pixels` are
// accepted and ignored (Q-MISC-51 (a)).
//
// Owner: PLOTTECH.

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/types.hpp"

namespace hefesto::display {
struct StyleSet;
}

namespace hefesto::tech {
class Technology;
struct RawSection;
}  // namespace hefesto::tech

namespace hefesto::plot {

/// A 9-field `postscript` line: a stipple pattern (Magic's PSPattern).
struct PsPattern {
  int index = 0;
  std::array<std::uint64_t, 8> rows{};  // `%08lx` fields (unsigned long), top row first
};

/// A 5-field `postscript` line: a CMYK colour (Magic's PSColor).
struct PsColor {
  int index = 0;
  std::array<std::uint8_t, 4> cmyk{};  // each field & 0xff
};

/// Fill of a layer style: a pattern index, or one of Magic's three
/// built-in fills (plotPS.c CROSS, BORDER, SOLID).
inline constexpr int kPsCross = -1;
inline constexpr int kPsBorder = -2;
inline constexpr int kPsSolid = -3;

/// A 3-field `postscript` line: the types drawn with a colour and a fill
/// (Magic's PSStyle). `types` is expanded through the layer-composition
/// sets and restricted to the user-visible types (DBUserLayerBits).
struct PsStyle {
  tech::TypeMask types;
  int color = 0;
  int stipple = 0;  // a pattern index, kPsCross, kPsBorder or kPsSolid
};

/// An RGB colour of the PNM tables (Magic's pnmcolor).
struct Rgb {
  std::uint8_t r = 0xff;
  std::uint8_t g = 0xff;
  std::uint8_t b = 0xff;

  friend bool operator==(const Rgb&, const Rgb&) = default;
};

/// How PNM paints a user type (Magic's pstyle): the write mask of the
/// display styles blended into it and the blended colour.
struct PnmPaint {
  std::uint32_t wmask = 0;
  Rgb color;

  friend bool operator==(const PnmPaint&, const PnmPaint&) = default;
};

/// The parsed `plot` section.
struct PlotTech {
  /// The `postscript` lists in Magic's order: each line is prepended, so
  /// the last line read comes first.
  std::vector<PsPattern> ps_patterns;
  std::vector<PsColor> ps_colors;
  std::vector<PsStyle> ps_styles;
  /// PNM paint per tile type, indexed by type number (the user types;
  /// empty without a `plot` section).
  std::vector<PnmPaint> pnm_paint;
  /// False when the technology has no `plot` section: `plot pnm` prints
  /// Magic's warning and uses the display-derived defaults.
  bool has_section = false;
  /// Magic's Init_Error: a PNM colour map or style file failed to load;
  /// `plot pnm` refuses until the next technology load.
  bool init_failed = false;
};

/// Where the PNM colours come from (design D6): the session's loaded
/// display style set, or (headless, or none loaded) the `7bit` class loaded
/// for the plot; the `color` and `dstyle` files are searched in
/// `search_dirs` (current directory, the technology's directory, then the
/// bundled assets/plot/).
struct StyleSource {
  const display::StyleSet* session = nullptr;  // not owned; nullptr = none loaded
  platform::FileSystem* files = nullptr;       // not owned; nullptr = no file access
  std::vector<platform::Path> search_dirs;
};

/// Reads the `plot` section `section` (nullptr: the technology has none)
/// against `tech`, reporting Magic's tech-load messages to `sink`.
[[nodiscard]] PlotTech read_plot_section(const tech::RawSection* section, const tech::Technology& tech,
                                         const StyleSource& styles, diag::DiagnosticSink& sink);

}  // namespace hefesto::plot
