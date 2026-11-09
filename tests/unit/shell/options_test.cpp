// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/shell/options.hpp"

using namespace hefesto::shell;

namespace {

ParseResult parse(std::vector<std::string_view> args) {
  return parse_shell_options(args);
}

}  // namespace

TEST_CASE("desktop options parse", "[cap:hefesto-architecture][req:App shells per platform]") {
  const ParseResult r = parse({"--theme", "light", "--rects", "10000000", "--bench-frames", "600", "--no-vsync"});
  REQUIRE(r.options);
  CHECK(r.options->theme == ThemeMode::light);
  CHECK(r.options->rects == 10'000'000);
  CHECK(r.options->bench_frames == 600);
  CHECK_FALSE(r.options->vsync);

  CHECK(parse({}).options->theme == ThemeMode::dark);
  CHECK(parse({"--help"}).options->help);

  const ParseResult smoke = parse({"--smoke-frames", "30", "--expect-window-system", "wayland"});
  REQUIRE(smoke.options);
  CHECK(smoke.options->smoke_frames == 30);
  CHECK(smoke.options->expect_window_system == "wayland");
  CHECK_FALSE(parse({"--smoke-frames", "-1"}).options);
}

TEST_CASE("bad desktop options are reported", "[cap:hefesto-architecture][req:App shells per platform]") {
  CHECK_FALSE(parse({"--theme", "blue"}).options);
  CHECK_FALSE(parse({"--rects"}).options);
  CHECK_FALSE(parse({"--rects", "12x"}).options);
  CHECK_FALSE(parse({"--width", "0"}).options);
  CHECK_FALSE(parse({"--frobnicate", "1"}).error.empty());
}

TEST_CASE("desktop options pass Magic startup words to app-startup",
          "[cap:app-startup][req:Command-line options]") {
  const ParseResult r = parse({"-T", "sky130A", "--theme", "dark", "-norcfile", "inv_1.mag", "--console"});
  REQUIRE(r.options);
  const std::vector<std::string> expected = {"-T", "sky130A", "-norcfile", "inv_1.mag"};
  CHECK(r.options->startup_args == expected);
  CHECK(r.options->show_console);
  CHECK_FALSE(r.options->show_pdk_manager);
}

TEST_CASE("desktop asset, capture and panel options parse", "[cap:hefesto-architecture][req:App shells per platform]") {
  const ParseResult r =
      parse({"--asset-dir", "/opt/assets", "--capture", "out.ppm", "--capture-frames", "5", "--pdk-manager"});
  REQUIRE(r.options);
  CHECK(r.options->asset_dir == "/opt/assets");
  CHECK(r.options->capture == "out.ppm");
  CHECK(r.options->capture_frames == 5);
  CHECK(r.options->show_pdk_manager);
  CHECK_FALSE(parse({"--capture-frames", "0"}).options);
  CHECK_FALSE(parse({"--font-dir", "/x"}).options);  // replaced by the bundled fonts (--asset-dir)
}
