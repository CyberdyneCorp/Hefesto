// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string_view>

#include "hefesto/shell/design_tokens.hpp"

using namespace hefesto::shell;

TEST_CASE("type scale matches the forge typography", "[cap:hefesto-design-system][req:Typography]") {
  CHECK(type_spec(TypeStyle::display).size_px == 28.0F);
  CHECK(type_spec(TypeStyle::display).weight == 700);
  CHECK(type_spec(TypeStyle::body).size_px == 13.0F);
  CHECK(type_spec(TypeStyle::label).uppercase);
  CHECK(type_spec(TypeStyle::label).tracking_em >= 0.06F);
  CHECK(type_spec(TypeStyle::label).tracking_em <= 0.08F);
  CHECK(type_spec(TypeStyle::mono).family == FontFamily::mono);
  CHECK(type_spec(TypeStyle::mono).size_px == 12.0F);
}

TEST_CASE("spacing, radius and density follow the forge scale",
          "[cap:hefesto-design-system][req:Spacing, radius, elevation, motion and density]") {
  for (const int step : kSpacing) {
    CHECK(step % 4 == 0);
  }
  CHECK(kRadiusControl == 4);
  CHECK(kRadiusPanel == 8);
  CHECK(kControlHeightMin == 28);
  CHECK(kControlHeightMax == 32);
  CHECK(kMotionHoverMs == 120);
  CHECK(kMotionPanelMs == 200);
}

TEST_CASE("every status has a distinct shape", "[cap:hefesto-design-system][req:Status is never color-only]") {
  std::set<std::string_view> glyphs;
  for (const auto kind : {StatusKind::success, StatusKind::warning, StatusKind::error, StatusKind::info}) {
    CHECK_FALSE(status_glyph(kind).empty());
    glyphs.insert(status_glyph(kind));
  }
  CHECK(glyphs.size() == 4);
  CHECK(status_glyph(StatusKind::success) == "●");
}
