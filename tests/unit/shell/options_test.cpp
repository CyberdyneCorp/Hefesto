// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors

#include <catch2/catch_test_macros.hpp>
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
