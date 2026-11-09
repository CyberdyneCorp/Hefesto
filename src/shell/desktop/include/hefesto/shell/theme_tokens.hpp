// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The forge color tokens of hefesto-design-system ("Color tokens and themes"),
// as data. The Raven theme is built from these values in the shell's Raven
// layer; keeping them here, toolkit-free, lets tests check the values and
// their contrast.

#pragma once

#include <cstdint>
#include <string_view>

namespace hefesto::shell {

struct Rgba {
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
  float a = 1.0F;

  /// From 0xRRGGBB.
  static constexpr Rgba hex(std::uint32_t rgb) noexcept {
    return Rgba{static_cast<float>((rgb >> 16U) & 0xFFU) / 255.0F, static_cast<float>((rgb >> 8U) & 0xFFU) / 255.0F,
                static_cast<float>(rgb & 0xFFU) / 255.0F, 1.0F};
  }
};

enum class ThemeMode : std::uint8_t { dark, light };

struct ThemeTokens {
  Rgba surface0;
  Rgba surface1;
  Rgba surface2;
  Rgba border;
  Rgba text_primary;
  Rgba text_secondary;
  Rgba accent;
  Rgba canvas;
  Rgba success;
  Rgba warning;
  Rgba danger;
  Rgba info;

  // Mockup-derived shades (docs/design/mockups/Desktop-Layout.dc.html,
  // Main.dc.html) that the spec's token table does not name yet. Recorded as
  // a spec/mockup difference in docs/design/m1-ui-conformance.md; the dark
  // values are the mockup's, the light ones are derived to keep WCAG 4.5:1.
  Rgba bar;             // top bar, tool rail, command bar, rulers  (#15181C)
  Rgba input;           // inputs, segmented control, status bar    (#0F1114)
  Rgba row_selected;    // selected row / active tab                (#2A2F37)
  Rgba control_border;  // button and input outline                 (#3D434E)
  Rgba text_tertiary;   // section labels, hints, meta text         (#8A919D)
  Rgba icon;            // tool icons, secondary-strong text        (#C9CED6)
  Rgba accent_text;     // text on the ember accent                 (#1A0F06)
  Rgba accent_soft;     // active tool icon, active row meta        (#FFB27A)
  Rgba success_text;    // status text on a 14 % tinted chip
  Rgba warning_text;
  Rgba danger_text;
  Rgba info_text;
  Rgba accent_hover;    // link / primary hover (Main.dc.html a:hover #FFA866)
  Rgba accent_pressed;  // primary pressed (derived)
};

[[nodiscard]] const ThemeTokens& theme_tokens(ThemeMode mode) noexcept;

/// The tokens for everything drawn on the layout canvas (rulers, overlay
/// text, zoom control, minimap): the canvas stays `#0B0C0E` in both themes,
/// so its overlay always uses the dark tokens, whatever `mode` is.
[[nodiscard]] const ThemeTokens& canvas_overlay_tokens(ThemeMode mode) noexcept;

/// WCAG 2.x contrast ratio between two opaque colors (1 to 21).
[[nodiscard]] double contrast_ratio(const Rgba& a, const Rgba& b) noexcept;

[[nodiscard]] std::string_view to_string(ThemeMode mode) noexcept;

}  // namespace hefesto::shell
