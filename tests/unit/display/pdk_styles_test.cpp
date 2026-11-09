// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The pinned sky130A technology resolved against Magic's distributed `mos`
// OpenGL display styles (capabilities display-style-files, tech-styles-section,
// hefesto-rendering). Skipped when the PDK cache (.cache/pdks) or the Magic
// reference sources (build/ref/magic) are absent.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "hefesto/diag/sink.hpp"
#include "hefesto/display/scene.hpp"
#include "hefesto/display/styles.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/tech/loader.hpp"
#include "reference_data.hpp"

namespace display = hefesto::display;
namespace fs = std::filesystem;

namespace {

std::optional<fs::path> sky130_tech() {
  return hefesto::test::pinned_sky130a_tech();  // the exact pinned version only
}

}  // namespace

TEST_CASE("sky130A loads its mos display styles and every styled layer resolves",
          "[cap:display-style-files][req:Style loading triggered by styletype]"
          "[cap:hefesto-rendering][req:Style-file-faithful 2D drawing]") {
  const auto tech_file = sky130_tech();
  const fs::path magic = fs::path(HEFESTO_SOURCE_DIR) / "build" / "ref" / "magic";
  if (!tech_file || !fs::exists(magic / "scmos" / "mos.OpenGL.dstyle")) {
    SKIP("sky130A tech (.cache/pdks) or Magic sources (build/ref/magic) not available");
  }
  hefesto::platform::LocalFileSystem files;
  hefesto::diag::CollectingSink sink;
  const auto tech = hefesto::tech::load_technology(files, *tech_file, {}, sink);
  REQUIRE(tech.ok());
  REQUIRE(tech.value()->styles().style_family == "mos");

  display::StyleLoadOptions options;
  options.library_dirs = {magic / "scmos", magic / "graphics"};
  hefesto::diag::CollectingSink style_sink;
  const auto set = display::load_style_set(files, *tech.value(), options, style_sink);
  REQUIRE(set.ok());
  CHECK_FALSE(style_sink.has_errors());
  CHECK(set.value().display_class == display::DisplayClass::opengl);
  CHECK(set.value().cursors.size() == 24);

  const auto palette = display::layer_palette(*tech.value(), set.value());
  CHECK(palette.size() > 20);
  bool met1 = false;
  for (const auto& swatch : palette) {
    met1 = met1 || swatch.name == "metal1";
    CHECK_FALSE(swatch.styles.empty());
  }
  CHECK(met1);
}
