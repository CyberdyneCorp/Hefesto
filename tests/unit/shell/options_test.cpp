// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "hefesto/app/startup.hpp"
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
  CHECK_FALSE(r.options->capture_running);
  CHECK(parse({"--capture", "card.ppm", "--capture-running"}).options->capture_running);
  CHECK_FALSE(parse({"--font-dir", "/x"}).options);  // replaced by the bundled fonts (--asset-dir)
}

TEST_CASE("a bad startup option prints Magic's usage line once", "[cap:app-startup][req:Command-line options]") {
  // Regression: the app printed the diagnostic (whose text already ends with
  // the usage line) and then the usage line again.
  const std::vector<std::string_view> words{"--"};
  const auto parsed = hefesto::app::parse_startup_options(words);
  REQUIRE_FALSE(parsed);
  const std::string text = hefesto::shell::startup_option_error(parsed.error());
  const std::string_view usage = hefesto::app::startup_usage_line();
  const auto first = text.find(usage);
  REQUIRE(first != std::string::npos);
  CHECK(text.find(usage, first + 1) == std::string::npos);
  // A diagnostic without it still gets it.
  const std::string other = hefesto::shell::startup_option_error(hefesto::diag::make("APP-OPTION-001", "bad"));
  CHECK(other.find(usage) != std::string::npos);
}

TEST_CASE("the run option takes a command line", "[cap:hefesto-app-ux]") {
  const std::vector<std::string_view> args{"--run", "load inv; zoom 2"};
  const auto parsed = hefesto::shell::parse_shell_options(args);
  REQUIRE(parsed.options);
  CHECK(parsed.options->run == "load inv; zoom 2");
}

TEST_CASE("the drag option injects a left-button drag between window points",
          "[cap:hefesto-app-ux][req:Panel layout and focus cycling]") {
  const std::vector<std::string_view> args{"--drag", "120,96.5,1400,300"};
  const auto parsed = hefesto::shell::parse_shell_options(args);
  REQUIRE(parsed.options);
  REQUIRE(parsed.options->drag.has_value());
  CHECK(parsed.options->drag->x0 == 120.0F);
  CHECK(parsed.options->drag->y0 == 96.5F);
  CHECK(parsed.options->drag->x1 == 1400.0F);
  CHECK(parsed.options->drag->y1 == 300.0F);
  for (const char* bad : {"1,2,3", "1,2,3,4,5", "a,b,c,d", "", "1,,2,3"}) {
    CHECK_FALSE(hefesto::shell::parse_drag_option(bad).has_value());
  }
}

TEST_CASE("the pinch option injects a trackpad pinch at a window point",
          "[cap:hefesto-input-and-touch][req:Desktop canvas gestures]") {
  const std::vector<std::string_view> args{"--pinch", "800,450.5,2.5"};
  const auto parsed = hefesto::shell::parse_shell_options(args);
  REQUIRE(parsed.options);
  REQUIRE(parsed.options->pinch.has_value());
  CHECK(parsed.options->pinch->x == 800.0F);
  CHECK(parsed.options->pinch->y == 450.5F);
  CHECK(parsed.options->pinch->factor == 2.5);
  for (const char* bad : {"800,450", "800,450,0", "800,450,-1", "a,b,c", "1,2,3,4", "", "1,,2"}) {
    CHECK_FALSE(hefesto::shell::parse_pinch_option(bad).has_value());
  }
  const std::vector<std::string_view> wrong{"--pinch", "1,2"};
  CHECK_FALSE(hefesto::shell::parse_shell_options(wrong).options);
}

TEST_CASE("captures can show the menu bar in the window, an open menu and a utility window",
          "[cap:hefesto-app-ux][req:Desktop platform integration through Raven]") {
  const std::vector<std::string_view> args{"--menu-bar-in-window", "--press", "385,63", "--capture-window", "pcell"};
  const auto parsed = parse(args);
  REQUIRE(parsed.options);
  CHECK(parsed.options->menu_bar_in_window);
  REQUIRE(parsed.options->press.has_value());
  CHECK(parsed.options->press->x0 == 385.0F);
  CHECK(parsed.options->press->y0 == 63.0F);
  CHECK(parsed.options->press->x1 == 385.0F);
  CHECK(parsed.options->capture_window == "pcell");
  // Defaults: the system bar on macOS, the main window captured.
  const auto plain = parse({});
  REQUIRE(plain.options);
  CHECK_FALSE(plain.options->menu_bar_in_window);
  CHECK_FALSE(plain.options->press.has_value());
  CHECK(plain.options->capture_window == "main");
  for (const char* name : {"main", "pcell", "texthelper", "reorder"}) CHECK(is_capture_window(name));
  CHECK_FALSE(parse({"--capture-window", "console"}).options);
  for (const char* bad : {"1", "1,2,3", "a,b", ""}) CHECK_FALSE(parse_press_option(bad).has_value());
}
