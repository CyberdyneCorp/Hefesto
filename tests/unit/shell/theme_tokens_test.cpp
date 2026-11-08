// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
