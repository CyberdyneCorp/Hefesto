// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The non-color forge tokens of hefesto-design-system, as toolkit-free
// data the Raven layer applies (colors are in theme_tokens.hpp):
//   "Typography"                         type scale, Inter / JetBrains Mono
//   "Spacing, radius, elevation, motion and density"
//   "Status is never color-only"         shape + label for every status
// Reference: docs/design/mockups/Main.dc.html (tokens and components) and
// Desktop-Layout.dc.html (layout editor screen). Gaps between these values
// and what Raven v0.2.2 can draw are recorded in
// docs/design/m1-ui-conformance.md.
//
// Owner: SHELL (openspec/changes/m1-core-viewer/design.md, D2 and D6).

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "hefesto/shell/theme_tokens.hpp"

namespace hefesto::shell {

// --- typography ----------------------------------------------------------

enum class FontFamily : std::uint8_t { ui, mono };  // Inter, JetBrains Mono

/// Bundled font files, relative to the app's asset directory (assets/fonts/,
/// OFL-1.1, notices in THIRD_PARTY_NOTICES.md).
/// Inter 4.1 and JetBrains Mono 2.304 (assets/fonts/README.md has the
/// release URLs and SHA-256 of the archives).
inline constexpr std::string_view kUiFontFile = "fonts/Inter-Regular.ttf";
inline constexpr std::string_view kUiFontMediumFile = "fonts/Inter-Medium.ttf";
inline constexpr std::string_view kUiFontSemiBoldFile = "fonts/Inter-SemiBold.ttf";
inline constexpr std::string_view kUiFontBoldFile = "fonts/Inter-Bold.ttf";
inline constexpr std::string_view kMonoFontFile = "fonts/JetBrainsMono-Regular.ttf";
inline constexpr std::string_view kMonoFontMediumFile = "fonts/JetBrainsMono-Medium.ttf";

/// One loaded face: family plus weight (400, 500, 600, 700; mono 400, 500).
struct FontFace {
  FontFamily family = FontFamily::ui;
  int weight = 400;
  friend constexpr bool operator==(const FontFace&, const FontFace&) = default;
};

/// The bundled file of a face (the nearest bundled weight).
[[nodiscard]] constexpr std::string_view font_file(FontFace face) noexcept {
  if (face.family == FontFamily::mono) {
    return face.weight >= 500 ? kMonoFontMediumFile : kMonoFontFile;
  }
  if (face.weight >= 700) return kUiFontBoldFile;
  if (face.weight >= 600) return kUiFontSemiBoldFile;
  if (face.weight >= 500) return kUiFontMediumFile;
  return kUiFontFile;
}

enum class TypeStyle : std::uint8_t { display, title, heading, body, label, mono };

struct TypeSpec {
  FontFamily family = FontFamily::ui;
  float size_px = 13.0F;
  int weight = 400;
  bool uppercase = false;
  float tracking_em = 0.0F;
};

/// The type scale of "Typography".
[[nodiscard]] constexpr TypeSpec type_spec(TypeStyle style) noexcept {
  switch (style) {
    case TypeStyle::display:
      return {FontFamily::ui, 28.0F, 700, false, 0.0F};
    case TypeStyle::title:
      return {FontFamily::ui, 20.0F, 600, false, 0.0F};
    case TypeStyle::heading:
      return {FontFamily::ui, 15.0F, 600, false, 0.0F};
    case TypeStyle::body:
      return {FontFamily::ui, 13.0F, 400, false, 0.0F};
    case TypeStyle::label:
      return {FontFamily::ui, 11.0F, 600, true, 0.07F};
    case TypeStyle::mono:
      return {FontFamily::mono, 12.0F, 400, false, 0.0F};
  }
  return {};
}

/// Every text role the desktop screens draw, with the family, size and
/// weight the Desktop-Layout mockup gives it. Sizes and weights stay on the
/// forge scale (sizes of "Typography" plus the mockup's 12 px UI text,
/// weights 400-700); text_roles_follow_type_scale() is unit-tested.
enum class TextRole : std::uint8_t {
  cell_title,      // top bar cell name               Inter 13 / 600
  cell_subtitle,   // PDK · version · magic-compat    Inter 11 / 400
  tab_active,      // segmented view tabs             Inter 12 / 600
  tab,             //                                 Inter 12 / 500
  chip,            // DRC / LVS status chips          Inter 12 / 500
  button,          // Extract, Simulate, Ask agent    Inter 13 / 500
  section_label,   // LAYERS, HIERARCHY, SELECTION    Inter 11 / 600 caps
  body,            // layer names, panel text         Inter 13 / 400
  body_strong,     // active layer name               Inter 13 / 600
  meta,            // plane names, hints              Inter 11 / 400
  heading,         // inspector selection title       Inter 15 / 600
  small,           // inspector keys, buttons         Inter 12 / 400
  mono,            // hierarchy, inspector values     JetBrains Mono 12 / 400
  mono_small,      // status bar, rulers, hints       JetBrains Mono 11 / 400
  command_prompt,  // the "%" of the command bar      JetBrains Mono 12 / 500
};

struct TextSpec {
  FontFace face;
  float size_px = 13.0F;
  bool uppercase = false;
};

[[nodiscard]] constexpr TextSpec text_spec(TextRole role) noexcept {
  constexpr FontFamily ui = FontFamily::ui;
  constexpr FontFamily mono = FontFamily::mono;
  switch (role) {
    case TextRole::cell_title: return {{ui, 600}, 13.0F, false};
    case TextRole::cell_subtitle: return {{ui, 400}, 11.0F, false};
    case TextRole::tab_active: return {{ui, 600}, 12.0F, false};
    case TextRole::tab: return {{ui, 500}, 12.0F, false};
    case TextRole::chip: return {{ui, 500}, 12.0F, false};
    case TextRole::button: return {{ui, 500}, 13.0F, false};
    case TextRole::section_label: return {{ui, 600}, 11.0F, true};
    case TextRole::body: return {{ui, 400}, 13.0F, false};
    case TextRole::body_strong: return {{ui, 600}, 13.0F, false};
    case TextRole::meta: return {{ui, 400}, 11.0F, false};
    case TextRole::heading: return {{ui, 600}, 15.0F, false};
    case TextRole::small: return {{ui, 400}, 12.0F, false};
    case TextRole::mono: return {{mono, 400}, 12.0F, false};
    case TextRole::mono_small: return {{mono, 400}, 11.0F, false};
    case TextRole::command_prompt: return {{mono, 500}, 12.0F, false};
  }
  return {};
}

inline constexpr std::array<TextRole, 15> kAllTextRoles = {
    TextRole::cell_title, TextRole::cell_subtitle, TextRole::tab_active, TextRole::tab,
    TextRole::chip,       TextRole::button,        TextRole::section_label, TextRole::body,
    TextRole::body_strong, TextRole::meta,         TextRole::heading,     TextRole::small,
    TextRole::mono,       TextRole::mono_small,    TextRole::command_prompt};

// --- spacing, radius, elevation, motion, density -------------------------

inline constexpr std::array<int, 6> kSpacing = {4, 8, 12, 16, 24, 32};  // px
inline constexpr int kRadiusControl = 4;   // desktop controls
inline constexpr int kRadiusPanel = 8;     // panels and cards
inline constexpr int kPanelBorderPx = 1;   // panels: border only, no shadow
inline constexpr int kPopoverShadowOffsetY = 10;   // 8-12 px
inline constexpr int kPopoverShadowBlur = 28;      // 24-32 px
inline constexpr float kPopoverShadowAlpha = 0.5F;  // 45-55 % black
inline constexpr int kMotionHoverMs = 120;  // ease-out; 0 with reduced motion
inline constexpr int kMotionPanelMs = 200;
inline constexpr int kControlHeightMin = 28;  // desktop density
inline constexpr int kControlHeightMax = 32;

// --- Desktop-Layout screen metrics (Desktop-Layout.dc.html) -------------

inline constexpr int kTopBarHeight = 48;
inline constexpr int kAppMarkSize = 26;
inline constexpr int kToolRailWidth = 52;
inline constexpr int kToolButtonSize = 36;
inline constexpr int kLeftPanelWidth = 240;
inline constexpr int kInspectorWidth = 284;
inline constexpr int kCommandBarHeight = 40;
inline constexpr int kStatusBarHeight = 26;
inline constexpr int kLayerRowHeight = 32;
inline constexpr int kSwatchWidth = 18;
inline constexpr int kSwatchHeight = 14;
inline constexpr int kRulerSize = 20;
inline constexpr int kMinimapWidth = 168;
inline constexpr int kMinimapHeight = 112;
inline constexpr int kCanvasInset = 16;   // minimap and zoom control from the canvas edges
inline constexpr int kZoomButtonSize = 32;
inline constexpr int kChipHeight = 26;
inline constexpr int kButtonHeight = 32;
inline constexpr int kInputHeight = 30;
inline constexpr int kConsoleHeight = 160;  // Console / Diagnostics (not in the mockup)

// --- status --------------------------------------------------------------

enum class StatusKind : std::uint8_t { success, warning, error, info };

/// Shape glyph paired with every status color (never color only).
[[nodiscard]] constexpr std::string_view status_glyph(StatusKind kind) noexcept {
  switch (kind) {
    case StatusKind::success:
      return "●";  // ●
    case StatusKind::warning:
      return "▲";  // ▲
    case StatusKind::error:
      return "■";  // ■
    case StatusKind::info:
      return "◆";  // ◆
  }
  return "";
}

/// The semantic color token of a status.
[[nodiscard]] inline Rgba status_color(const ThemeTokens& tokens, StatusKind kind) noexcept {
  switch (kind) {
    case StatusKind::success:
      return tokens.success;
    case StatusKind::warning:
      return tokens.warning;
    case StatusKind::error:
      return tokens.danger;
    case StatusKind::info:
      return tokens.info;
  }
  return tokens.text_primary;
}

/// The text color of a status (chips, status bar, PDK states).
[[nodiscard]] inline Rgba status_text_color(const ThemeTokens& tokens, StatusKind kind) noexcept {
  switch (kind) {
    case StatusKind::success:
      return tokens.success_text;
    case StatusKind::warning:
      return tokens.warning_text;
    case StatusKind::error:
      return tokens.danger_text;
    case StatusKind::info:
      return tokens.info_text;
  }
  return tokens.text_secondary;
}

/// Opacity of a status chip's tint: the status color at 14 % over the bar.
inline constexpr float kChipTintAlpha = 0.14F;

/// `fg` at opacity `alpha` composited over the opaque `bg` (sRGB blending,
/// as the painter does), for contrast checks of tinted backgrounds.
[[nodiscard]] constexpr Rgba composite_over(const Rgba& fg, float alpha, const Rgba& bg) noexcept {
  return Rgba{alpha * fg.r + (1.0F - alpha) * bg.r, alpha * fg.g + (1.0F - alpha) * bg.g,
              alpha * fg.b + (1.0F - alpha) * bg.b, 1.0F};
}

}  // namespace hefesto::shell
