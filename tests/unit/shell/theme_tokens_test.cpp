// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "hefesto/shell/design_tokens.hpp"
#include "hefesto/shell/theme_tokens.hpp"

using Catch::Approx;
using namespace hefesto::shell;

TEST_CASE("forge tokens have the specified values", "[cap:hefesto-design-system][req:Color tokens and themes]") {
  const ThemeTokens& dark = theme_tokens(ThemeMode::dark);
  const ThemeTokens& light = theme_tokens(ThemeMode::light);
  CHECK(dark.surface0.r == Approx(0x12 / 255.0));
  CHECK(dark.accent.g == Approx(0x8A / 255.0));
  CHECK(light.surface1.b == Approx(1.0));
  CHECK(light.accent.r == Approx(0xC2 / 255.0));
  // The canvas keeps its dark background in both themes (scenario "Theme switch").
  CHECK(dark.canvas.r == light.canvas.r);
  CHECK(dark.canvas.b == light.canvas.b);
}

TEST_CASE("text tokens meet WCAG 4.5:1 on every surface", "[cap:hefesto-design-system][req:Color tokens and themes]") {
  for (ThemeMode mode : {ThemeMode::dark, ThemeMode::light}) {
    const ThemeTokens& t = theme_tokens(mode);
    for (const Rgba* surface : {&t.surface0, &t.surface1, &t.surface2}) {
      CHECK(contrast_ratio(t.text_primary, *surface) >= 4.5);
      CHECK(contrast_ratio(t.text_secondary, *surface) >= 4.5);
    }
  }
}

TEST_CASE("contrast ratio spans 1 to 21", "[cap:hefesto-design-system][req:Color tokens and themes]") {
  CHECK(contrast_ratio(Rgba::hex(0x000000), Rgba::hex(0xFFFFFF)) == Approx(21.0));
  CHECK(contrast_ratio(Rgba::hex(0x777777), Rgba::hex(0x777777)) == Approx(1.0));
}

TEST_CASE("text drawn on the canvas keeps 4.5:1 in both themes",
          "[cap:hefesto-design-system][req:Color tokens and themes]") {
  // Regression: the canvas empty-state hint used the light theme's
  // text_secondary (#4E5663) on the always-dark canvas (#0B0C0E), about 2.6:1.
  for (const ThemeMode mode : {ThemeMode::dark, ThemeMode::light}) {
    const ThemeTokens& t = canvas_overlay_tokens(mode);
    const Rgba& canvas = theme_tokens(mode).canvas;
    CHECK(contrast_ratio(canvas, theme_tokens(ThemeMode::dark).canvas) == 1.0);  // dark in both themes
    for (const Rgba* text : {&t.text_primary, &t.text_secondary, &t.text_tertiary, &t.icon}) {
      CHECK(contrast_ratio(*text, canvas) >= 4.5);
      CHECK(contrast_ratio(*text, t.bar) >= 4.5);       // rulers
      CHECK(contrast_ratio(*text, t.surface2) >= 4.5);  // zoom control
    }
  }
  CHECK(contrast_ratio(theme_tokens(ThemeMode::light).text_secondary, theme_tokens(ThemeMode::light).canvas) < 4.5);
}

// Regression: the light status text tokens measured 4.0-4.2:1 on the 14 %
// chip tint over the bar (DRC/LVS chips) and 4.1-4.5:1 on the selected PDK row.
TEST_CASE("status text keeps 4.5:1 on chips and selected rows",
          "[cap:hefesto-design-system][req:Color tokens and themes]") {
  for (const ThemeMode mode : {ThemeMode::dark, ThemeMode::light}) {
    const ThemeTokens& t = theme_tokens(mode);
    for (const StatusKind kind : {StatusKind::success, StatusKind::warning, StatusKind::error, StatusKind::info}) {
      const Rgba text = status_text_color(t, kind);
      const Rgba chip = composite_over(status_color(t, kind), kChipTintAlpha, t.bar);
      CHECK(contrast_ratio(text, chip) >= 4.5);
      CHECK(contrast_ratio(text, t.row_selected) >= 4.5);  // PDK state on the selected row
      CHECK(contrast_ratio(text, t.input) >= 4.5);         // status bar
      CHECK(contrast_ratio(text, t.surface1) >= 4.5);      // PDK state on an unselected row
    }
    // The PDK row subtitle switches to text_secondary on a highlighted row.
    CHECK(contrast_ratio(t.text_secondary, t.row_selected) >= 4.5);
  }
}

// Regression: the unchecked PDK checkbox was outlined with control_border
// (1.9:1 dark, 1.6:1 light); its outline is now text_tertiary.
TEST_CASE("checkbox outline meets 3:1 non-text contrast", "[cap:hefesto-design-system][req:Color tokens and themes]") {
  for (const ThemeMode mode : {ThemeMode::dark, ThemeMode::light}) {
    const ThemeTokens& t = theme_tokens(mode);
    CHECK(contrast_ratio(t.text_tertiary, t.input) >= 3.0);
    CHECK(contrast_ratio(t.text_tertiary, t.surface1) >= 3.0);
  }
}

TEST_CASE("layer swatches composite to the same color in both themes",
          "[cap:hefesto-design-system][req:Layer colors come from the PDK, not the theme]") {
  // Regression: swatches were filled over the panel surface, so a translucent
  // layer fill (dnwell at 0.2-0.6 alpha) read (164,165,10) dark but (238,238,82) light.
  const Rgba dnwell{0.6F, 0.6F, 0.05F, 0.4F};
  const Rgba dark = composite_over(dnwell, swatch_backdrop(theme_tokens(ThemeMode::dark)));
  const Rgba light = composite_over(dnwell, swatch_backdrop(theme_tokens(ThemeMode::light)));
  CHECK(dark.r == Approx(light.r));
  CHECK(dark.g == Approx(light.g));
  CHECK(dark.b == Approx(light.b));
  const Rgba canvas = Rgba::hex(0x0B0C0E);
  CHECK(swatch_backdrop(theme_tokens(ThemeMode::light)).r == Approx(canvas.r));
}
