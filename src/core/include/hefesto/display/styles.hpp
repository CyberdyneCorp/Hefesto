// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Display styles: color maps (.cmap), display-style tables (.dstyle),
// stipples, cursor glyphs, and the resolution of the technology's styles
// section against them (capabilities display-style-files and
// tech-styles-section "Type to style mapping"; OpenGL variant per X-5).
// (Magic ref: graphics/grCMap.c, graphics/grDStyle.c, graphics/grGlyphs.c,
// dbwind/DBWdisplay.c)
//
// Owner: VIEWER (openspec/changes/m1-core-viewer/design.md, D2).

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/diag/outcome.hpp"
#include "hefesto/diag/sink.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/technology.hpp"

namespace hefesto::display {

// --- display class ("Dstyle file name and lookup", X-5) --------------------

/// Which Magic display-file variant is read. OpenGL is Hefesto's default.
enum class DisplayClass : std::uint8_t { opengl, bit7, bit24, bw };

/// The `D`/`C` part of the file names: "OpenGL", "7bit", "24bit", "bw".
[[nodiscard]] std::string_view display_class_name(DisplayClass display_class) noexcept;
/// The valid bit-plane mask of the class (OpenGL and 24bit: 24 planes).
[[nodiscard]] std::int32_t display_plane_mask(DisplayClass display_class) noexcept;

// --- color map ("Color map file grammar", "Color map file writing") -------

struct ColorEntry {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  std::string name;  // empty if unnamed

  friend bool operator==(const ColorEntry&, const ColorEntry&) = default;
};

struct ColorMap {
  std::vector<ColorEntry> entries;  // index = color number

  /// Index of the first entry named exactly `name` (GrNameToColor).
  [[nodiscard]] std::optional<std::size_t> find(std::string_view name) const;
};

/// The largest color map a .cmap may declare (indices 0 .. 2^24 - 1).
inline constexpr std::size_t kMaxColorMapEntries = std::size_t{1} << 24;

/// Reads a .cmap (two passes, run filling). `file_name` is the name used in
/// messages ("F.cmap"). Blank lines are skipped (Q-UI-7). A map that would
/// hold more than `max_entries` entries is refused before it is allocated.
[[nodiscard]] diag::Outcome<ColorMap> parse_color_map(std::string_view text, std::string_view file_name,
                                                      bool magic_compat,
                                                      std::size_t max_entries = kMaxColorMapEntries);
/// One "%d %d %d %d[ %s]\n" line per run of equal RGB (GrSaveCMap).
[[nodiscard]] std::string write_color_map(const ColorMap& map);

// --- display-style table ("Style table model" ... "Outline and fill") ----

enum class FillKind : std::uint8_t { solid, cross, outline, stipple, grid };

/// One row of the style table.
struct DisplayStyle {
  std::int32_t mask = 0;          // write mask, or opacity 0..127 on OpenGL displays
  std::int32_t color = 0;         // color-map index
  std::int32_t outline = 0;       // outline pattern (8 significant bits)
  FillKind fill = FillKind::solid;
  std::int32_t stipple = 0;       // stipple number
  char short_name = ' ';          // 7-bit
  std::optional<std::string> long_name;  // absent: never matches a lookup by name (Q-UI-5)
  std::int32_t ordinal = 0;       // the `num` column
  bool defined = true;            // false for a slot no file line filled (count mismatch)

  friend bool operator==(const DisplayStyle&, const DisplayStyle&) = default;
};

/// An 8x8 stipple: rows[0] is the bottom pixel row, MSB is the leftmost
/// pixel (OpenGL orientation, X-5).
using Stipple = std::array<std::uint8_t, 8>;

/// Fixed internal style slots 0..51 ("Fixed internal style slots").
inline constexpr std::size_t kFixedStyleCount = 52;
inline constexpr std::size_t kStyleTransparent = 0;
inline constexpr std::size_t kStyleBox = 1;
inline constexpr std::size_t kStyleMediumHighlights = 2;
inline constexpr std::size_t kStyleOutlineHighlights = 6;
inline constexpr std::size_t kStyleLabels = 12;
inline constexpr std::size_t kStylePorts = 13;
inline constexpr std::size_t kStylePortConnection = 14;
inline constexpr std::size_t kStyleCellBox = 15;
inline constexpr std::size_t kStyleGrid = 16;
inline constexpr std::size_t kStyleOrigin = 18;
inline constexpr std::size_t kStyleBackground = 23;
inline constexpr std::size_t kStylePaleLabels = 27;
inline constexpr std::size_t kStylePalePorts = 28;
inline constexpr std::size_t kStyleCrosshair = 44;

struct StyleTable {
  std::vector<DisplayStyle> styles;  // index = table index (file order, see spec)
  std::size_t layout_count = 0;      // N: layout styles at 52..52+N-1, pale at 52+N..
  std::vector<Stipple> stipples;
  int version = 5;                   // format version of the file
  /// GrStyleNames: the style whose short name is the character (earliest
  /// in the file wins); 0 (transparent) for unused characters.
  std::array<std::size_t, 128> by_short_name{};

  [[nodiscard]] std::size_t pale_index(std::size_t layout_index) const noexcept {
    return layout_index + layout_count;
  }
  /// The stipple pattern `number`, all zero when undefined.
  [[nodiscard]] Stipple stipple(std::int32_t number) const noexcept;
  /// "Type to style mapping": the table index of the first layout style
  /// whose ordinal equals the integer reference, or whose long name equals
  /// the name reference; std::nullopt when none (DBWTechParseStyle).
  [[nodiscard]] std::optional<std::size_t> find_layout_style(std::string_view ref) const;
};

/// GrGetStyleFromName: the first table index whose long name is `name`
/// (any index, fixed or technology style); nullopt when none.
[[nodiscard]] std::optional<std::size_t> style_from_name(const StyleTable& table, std::string_view name);

/// Parses a .dstyle file. Color names are resolved through `colors`;
/// `plane_mask` is the display's valid bit-plane mask. Warnings that do not
/// fail the load go to `sink`; a failed load returns the error and leaves
/// no table ("A failed load SHALL leave no style table").
[[nodiscard]] diag::Outcome<StyleTable> parse_dstyle(std::string_view text, std::string_view file_name,
                                                     const ColorMap& colors, bool magic_compat,
                                                     diag::DiagnosticSink& sink,
                                                     std::int32_t plane_mask = 0xFFFFFF, int default_version = 5);

/// "Dstyle file name and lookup" / "Color map file grammar" / "Style file
/// search path": the files of family `family` for `display_class`.
struct StyleFiles {
  platform::Path dstyle;
  bool dstyle_is_v5 = false;  // found as `.dstyle5` (parsed as version 5, Q-UI-1)
  std::optional<platform::Path> cmap;  // none for monochrome displays
  platform::Path glyphs;
};

/// Searches `search_dirs` in order (the caller puts "." first, then the
/// styletype directories, then the system library path). Fails with
/// Magic's "Couldn't open ..." message when a file is missing.
[[nodiscard]] diag::Outcome<StyleFiles> find_style_files(platform::FileSystem& fs, std::string_view family,
                                                         DisplayClass display_class,
                                                         const std::vector<platform::Path>& search_dirs,
                                                         std::string_view monitor = "std");

// --- cursor glyphs ("Glyph file grammar", "Cursor glyph indices") ---------

struct Glyph {
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::int32_t hot_x = 0;
  std::int32_t hot_y = 0;
  /// Style-table index per pixel: pixel (x, y) is pixels[x + width * y],
  /// y = 0 the bottom row (the file lists rows from the top).
  std::vector<std::size_t> pixels;

  [[nodiscard]] std::size_t pixel(std::int32_t x, std::int32_t y) const {
    return pixels.at(static_cast<std::size_t>(x + width * y));
  }
};
/// Reads a .glyphs file; characters map to styles through `table`'s short
/// names. "Extra lines" is a warning on `sink`.
[[nodiscard]] diag::Outcome<std::vector<Glyph>> parse_glyphs(std::string_view text, std::string_view file_name,
                                                             const StyleTable& table, diag::DiagnosticSink& sink);

/// Cursor glyph indices ("Cursor glyph indices").
enum class Cursor : std::uint8_t {
  normal = 0, star = 1, corner_ll = 2, corner_lr = 3, corner_ul = 4, corner_ur = 5,
  box_ll = 6, box_lr = 7, box_ul = 8, box_ur = 9, move_ll = 10, move_lr = 11, move_ul = 12, move_ur = 13,
  resize_ll = 14, resize_lr = 15, resize_ul = 16, resize_ur = 17, netlist = 18, wiring = 19,
  route = 20, rsim = 21, pick = 22,
};

// --- type -> style mapping (tech-styles-section) ---------------------------

/// For each tile type, the style-table indices it is drawn with, in order.
struct TypeStyleMap {
  std::vector<std::vector<std::size_t>> styles_of_type;  // index = tech::TileType

  [[nodiscard]] const std::vector<std::size_t>& of(tech::TileType type) const;
  /// DBWStyleToTypesTbl: the types bound to table index `style`.
  [[nodiscard]] tech::TypeMask types_of(std::size_t style) const;
};

/// Resolves the technology's raw styles section against `table` ("Type to
/// style mapping", "Contact style propagation"). Invalid references are
/// reported to `sink` and skipped.
[[nodiscard]] TypeStyleMap resolve_type_styles(const tech::Technology& tech, const StyleTable& table,
                                               diag::DiagnosticSink& sink);
/// Same, for an explicit styles section (tests, or a section read apart).
[[nodiscard]] TypeStyleMap resolve_type_styles(const tech::Technology& tech, const tech::StylesSection& section,
                                               const StyleTable& table, diag::DiagnosticSink& sink);

/// Everything the viewer needs to draw a technology's layers.
struct StyleSet {
  DisplayClass display_class = DisplayClass::opengl;
  ColorMap colors;
  StyleTable table;
  TypeStyleMap types;
  std::vector<Glyph> cursors;
};

struct StyleLoadOptions {
  DisplayClass display_class = DisplayClass::opengl;  // X-5 default
  bool magic_compat = true;
  std::string monitor = "std";
  /// The system library path entries searched after "." and the styletype
  /// directories (Magic: ". $CAD_ROOT/magic/sys $CAD_ROOT/magic/sys/current").
  std::vector<platform::Path> library_dirs;
};

/// "Style loading triggered by styletype": loads the color map, the dstyle
/// file and the cursor glyphs of `tech`'s styletype family and resolves
/// the type bindings. Headless sessions may skip this: a technology loads
/// without display files (Q-TECH-24).
[[nodiscard]] diag::Outcome<StyleSet> load_style_set(platform::FileSystem& fs, const tech::Technology& tech,
                                                     const StyleLoadOptions& options, diag::DiagnosticSink& sink);

}  // namespace hefesto::display
