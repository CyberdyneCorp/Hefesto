// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Painting of the Desktop-Layout screen elements
// (docs/design/mockups/Desktop-Layout.dc.html), one function per mockup
// element, with values from the forge tokens (theme_tokens.hpp,
// design_tokens.hpp) and content from the view models. Layer colors come
// only from LayerRow (the PDK display styles), never from the theme.
// Raven-free: drawing goes through ui::Painter.

#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "hefesto/display/scene.hpp"
#include "hefesto/shell/canvas_overlay.hpp"
#include "hefesto/shell/drc_models.hpp"
#include "hefesto/shell/view_models.hpp"
#include "raven_ui.hpp"

namespace hefesto::shell::paint {

struct WidgetState {
  bool hovered = false;
  bool focused = false;
  bool enabled = true;
  bool active = false;
};

enum class ButtonStyle : std::uint8_t { secondary, primary, ghost };

void app_mark(ui::Painter& p, float x, float y, float size, const ThemeTokens& t);
/// The cell name and the PDK line, left-aligned at `x`, vertically centered in `h`.
void header_title(ui::Painter& p, float x, float h, const HeaderText& text, const ThemeTokens& t);
/// The segmented Layout / Schematic / 3D / Waveforms control; `labels[0]` is active.
void segmented(ui::Painter& p, float w, float h, const std::vector<std::string_view>& labels, const ThemeTokens& t);
[[nodiscard]] float segmented_width(const ui::Runtime& rt, const std::vector<std::string_view>& labels);
void chip(ui::Painter& p, float w, float h, const StatusChip& chip, const ThemeTokens& t);
[[nodiscard]] float chip_width(const ui::Runtime& rt, const StatusChip& chip);
void button(ui::Painter& p, float w, float h, std::string_view label, std::string_view icon, ButtonStyle style,
            WidgetState s, const ThemeTokens& t);
[[nodiscard]] float button_width(const ui::Runtime& rt, std::string_view label, bool with_icon);
void tool(ui::Painter& p, float w, float h, const ToolButton& tool, WidgetState s, const ThemeTokens& t);
void section_label(ui::Painter& p, float x, float y, std::string_view text, const ThemeTokens& t);
void layer_row(ui::Painter& p, float w, float h, const LayerRow& row, WidgetState s, const ThemeTokens& t);
void hierarchy_row(ui::Painter& p, float w, float h, const HierarchyRow& row, WidgetState s, const ThemeTokens& t);
void inspector(ui::Painter& p, float w, float h, const InspectorView& view, const ThemeTokens& t);
/// The shapes of the status bar's message, problem count and DRC state (their text colors).
struct StatusBarKinds {
  StatusKind message = StatusKind::info;
  StatusKind problems = StatusKind::error;
  StatusKind drc = StatusKind::info;
};
void status_bar(ui::Painter& p, float w, float h, const StatusBarText& text, const StatusBarKinds& kinds,
                const ThemeTokens& t);
void fill_rect(ui::Painter& p, float w, float h, const Rgba& c);

/// What the canvas overlay draws on top of the layout (rulers, bitmap label
/// text, subcell names, the box, zoom control, minimap). Everything that is
/// layout geometry (fills, outlines, contact crosses, triangles, glyphs of
/// font labels, label rectangles, cell boxes) is drawn by the renderer.
struct OverlayState {
  render::Camera camera;
  double pixel_ratio = 1.0;
  double fit_pixels_per_unit = 0.0;
  UnitScale scale;
  std::optional<db::Rect> box;
  /// The edit cell's bounding box in root coordinates when the window shows
  /// it (medium highlights for a box outside it, "Box display").
  std::optional<db::Rect> edit_bbox;
  /// Colors of the highlight styles 1 (box), 2 (medium) and 6 (outline
  /// highlights) from the display styles: white in Magic's styles.
  std::array<Rgba, 3> highlight{Rgba{1.0F, 1.0F, 1.0F, 1.0F}, Rgba{1.0F, 1.0F, 1.0F, 1.0F},
                                Rgba{1.0F, 1.0F, 1.0F, 1.0F}};
  db::Rect bounds;  // the root cell's bounding box (minimap); null: empty
  std::vector<MinimapFill> minimap;  // the layout thumbnail (minimap_thumbnail), minimap pixels
  std::vector<display::SceneLabel> labels;  // bitmap label text (glyphs of font labels are on the GPU)
  std::vector<display::SceneCell> cells;
  /// Highlight text and point crosses (sized in pixels); the rest of the
  /// highlights, the box included, is drawn by the renderer.
  std::vector<display::HighlightItem> marks;
  /// A Paint / Label / Measure sweep in progress (dashed preview).
  std::optional<db::Rect> sweep;
  /// The tag on the error tile the last `drc find` put the box on.
  std::optional<DrcCanvasTag> drc_tag;
  std::string empty_hint;  // shown when nothing is loaded
};

/// Draws with canvas_overlay_tokens(mode): the canvas is dark in both themes.
void canvas_overlay(ui::Painter& p, float w, float h, const OverlayState& state, ThemeMode mode);

}  // namespace hefesto::shell::paint
