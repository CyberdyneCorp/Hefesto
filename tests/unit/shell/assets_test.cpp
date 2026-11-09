// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The bundled UI assets (Inter, JetBrains Mono under OFL-1.1, icons), how the
// app finds them, and source checks of the design-system rules that a
// window-free test can see: every text role on the forge type scale, no
// color literal in the shell's drawing code, an accessible name for every
// icon of the tool rail.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <string>

#include "hefesto/shell/assets.hpp"
#include "hefesto/shell/design_tokens.hpp"
#include "hefesto/shell/view_models.hpp"

using namespace hefesto::shell;
namespace fs = std::filesystem;

namespace {

const fs::path kSource = HEFESTO_SOURCE_DIR;

std::string read(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("Inter and JetBrains Mono are bundled with their OFL licenses",
          "[cap:hefesto-design-system][req:Typography]") {
  const fs::path assets = kSource / "assets";
  for (const FontFace face : {FontFace{FontFamily::ui, 400}, FontFace{FontFamily::ui, 500}, FontFace{FontFamily::ui, 600},
                              FontFace{FontFamily::ui, 700}, FontFace{FontFamily::mono, 400},
                              FontFace{FontFamily::mono, 500}}) {
    const fs::path file = assets / font_file(face);
    INFO(file.string());
    REQUIRE(fs::is_regular_file(file));
    const std::string bytes = read(file);
    REQUIRE(bytes.size() > 4);
    CHECK(bytes.compare(0, 4, std::string("\0\1\0\0", 4)) == 0);  // TrueType sfnt version 1.0
  }
  for (const char* license : {"fonts/Inter-OFL.txt", "fonts/JetBrainsMono-OFL.txt"}) {
    const std::string text = read(assets / license);
    CHECK(text.find("SIL OPEN FONT LICENSE Version 1.1") != std::string::npos);
  }
}

TEST_CASE("the asset folder is found next to the app or in the source tree",
          "[cap:hefesto-design-system][req:Typography]") {
  const auto dirs = asset_search_dirs("/override", "/opt/hefesto/bin", "/src/assets");
  REQUIRE(dirs.size() == 5);
  CHECK(dirs[0] == fs::path("/override"));
  CHECK(dirs[1] == fs::path("/opt/hefesto/bin/assets"));
  CHECK(dirs[2] == fs::path("/opt/hefesto/bin/../Resources/assets"));
  CHECK(dirs[3] == fs::path("/opt/hefesto/bin/../share/hefesto/assets"));
  CHECK(dirs[4] == fs::path("/src/assets"));
  CHECK(asset_search_dirs("", "", "").empty());
  CHECK(find_asset_dir({"/nonexistent", kSource / "assets"}) == kSource / "assets");
  CHECK_FALSE(find_asset_dir({"/nonexistent"}).has_value());
}

TEST_CASE("every text role uses the forge families, sizes and weights",
          "[cap:hefesto-design-system][req:Typography]") {
  const std::set<float> sizes = {28.0F, 20.0F, 15.0F, 13.0F, 12.0F, 11.0F};
  for (const TextRole role : kAllTextRoles) {
    const TextSpec spec = text_spec(role);
    CHECK(sizes.count(spec.size_px) == 1);
    CHECK(spec.face.weight >= 400);
    CHECK(spec.face.weight <= 700);
    CHECK(spec.face.weight % 100 == 0);
    if (spec.face.family == FontFamily::mono) CHECK(spec.face.weight <= 500);  // bundled mono weights
  }
  CHECK(text_spec(TextRole::mono_small).face.family == FontFamily::mono);  // status bar coordinates
  CHECK(text_spec(TextRole::section_label).uppercase);
  CHECK(font_file({FontFamily::ui, 600}) == kUiFontSemiBoldFile);
  CHECK(font_file({FontFamily::mono, 400}) == kMonoFontFile);
}

TEST_CASE("the shell's drawing code has no color literals",
          "[cap:hefesto-design-system][req:Color tokens and themes]") {
  // Colors come from theme_tokens.cpp (forge tokens) or the PDK display
  // styles; the Raven layer and the painters use only those.
  const std::regex literal(R"(Rgba::hex\(|Rgba\{\s*0x|#[0-9A-Fa-f]{6}\b)");
  for (const char* file : {"raven_ui.cpp", "screen_paint.cpp", "main_window.cpp", "side_panels.cpp", "pdk_panel.cpp",
                           "layout_canvas.cpp"}) {
    INFO(file);
    const std::string text = read(kSource / "src/shell/desktop/src" / file);
    REQUIRE_FALSE(text.empty());
    CHECK_FALSE(std::regex_search(text, literal));
  }
}

TEST_CASE("every tool icon is bundled and every icon-only button has a name",
          "[cap:hefesto-design-system][req:Icons]") {
  for (const ToolButton& tool : tool_rail()) {
    INFO(std::string(tool.icon));
    CHECK(fs::is_regular_file(kSource / "assets/icons" / (std::string(tool.icon) + ".svg")));
    CHECK_FALSE(tool.label.empty());
    CHECK_FALSE(tool.tooltip.empty());
  }
}
