// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// `.ext` input is untrusted (design D21 resource caps, D23 M6-S8): use
// cycles, deep or wide hierarchies, huge arrays and subscript ranges,
// long lines, overflowing numbers and random bytes end the read with a
// diagnostic instead of a crash, a hang or undefined behaviour (Magic
// recurses forever on a cycle and overflows its argv on long lines).
// Owner: EXTSUB.

#include <catch2/catch_test_macros.hpp>

#include <random>
#include <string>
#include <vector>

#include "ext_fixture.hpp"

namespace extflat = hefesto::extflat;
using extflat::test::ExtFixture;
using extflat::test::names;
using Strings = std::vector<std::string>;

#define CAP_REQ "[cap:ext-flattening-read][req:Locating and recursively reading .ext files]"

// Regression: efFlatNodesStdCell recursed forever on a use cycle.
TEST_CASE("a use cycle fails the read with EXTFLAT-READ-010", CAP_REQ) {
  ExtFixture f;
  f.file("top", "use a a0 1 0 0 0 1 0\n");
  f.file("a", "use b b0 1 0 0 0 1 0\n");
  f.file("b", "use a a1 1 0 0 0 1 0\n");
  const auto n = f.read("top");
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().code == "EXTFLAT-READ-010");
  CHECK(n.error().native_only);
}

TEST_CASE("a cell that uses itself is a cycle", CAP_REQ) {
  ExtFixture f;
  f.file("top", "use top me 1 0 0 0 1 0\n");
  const auto n = f.read("top");
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().code == "EXTFLAT-READ-010");
}

TEST_CASE("a hierarchy deeper than the cap fails without exhausting the stack", CAP_REQ) {
  ExtFixture f;
  constexpr int kDepth = 3000;
  for (int i = 0; i < kDepth; ++i) {
    f.file("c" + std::to_string(i), "node n 0 0 0 0 m1\nuse c" + std::to_string(i + 1) + " u 1 0 0 0 1 0\n");
  }
  f.file("c" + std::to_string(kDepth), "node n 0 0 0 0 m1\n");
  const auto n = f.read("c0");
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().code == "EXTFLAT-READ-009");
}

TEST_CASE("a hierarchy that doubles at every level stops at the instance cap", CAP_REQ) {
  ExtFixture f;
  for (int i = 0; i < 40; ++i) {
    const std::string child = "c" + std::to_string(i + 1);
    f.file("c" + std::to_string(i), "use " + child + " l 1 0 0 0 1 0\nuse " + child + " r 1 0 0 0 1 0\n");
  }
  f.file("c40", "node n 0 0 0 0 m1\n");
  extflat::ReadOptions options;
  options.limits.max_instances = 10000;
  const auto n = f.read("c0", options);
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().code == "EXTFLAT-READ-009");
}

TEST_CASE("a huge array use and a huge subscript range stop at their caps", CAP_REQ) {
  ExtFixture f;
  f.file("top", "use c a[-2147483648:2147483647:1][0:0:0] 1 0 0 0 1 0\n");
  f.file("c", "node n 0 0 0 0 m1\n");
  extflat::ReadOptions options;
  options.limits.max_instances = 1000;
  const auto arr = f.read("top", options);
  REQUIRE_FALSE(arr.ok());
  CHECK(arr.error().code == "EXTFLAT-READ-009");

  ExtFixture g;
  g.file("top", "merge a[-2147483648:2147483647] b[-2147483648:2147483647]\n");
  extflat::ReadOptions conn;
  conn.limits.max_connections = 1000;
  const auto merged = g.read("top", conn);
  REQUIRE_FALSE(merged.ok());
  CHECK(merged.error().code == "EXTFLAT-READ-009");
}

TEST_CASE("a line longer than the cap fails the read", CAP_REQ) {
  ExtFixture f;
  f.file("top", "node " + std::string(5000, 'a') + " 0 0 0 0 m1\n");
  extflat::ReadOptions options;
  options.limits.max_line_bytes = 1024;
  const auto n = f.read("top", options);
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().code == "EXTFLAT-READ-009");
}

TEST_CASE("a file larger than the cap is not read", CAP_REQ) {
  ExtFixture f;
  f.file("top", std::string(4096, '#'));
  extflat::ReadOptions options;
  options.limits.max_file_bytes = 100;
  const auto n = f.read("top", options);
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().code == "EXTFLAT-READ-009");
}

TEST_CASE("the number of flat names is capped", CAP_REQ) {
  ExtFixture f;
  f.file("top", "use c u[0:99:1][0:0:0] 1 0 0 0 1 0\n");
  f.file("c", "node n 0 0 0 0 m1\n");
  extflat::ReadOptions options;
  options.limits.max_flat_names = 50;
  const auto n = f.read("top", options);
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().code == "EXTFLAT-READ-009");
}

TEST_CASE("overflowing coordinates and transforms saturate", CAP_REQ) {
  ExtFixture f;
  f.file("top", "use c u 2147483647 2147483647 2147483647 -2147483648 2147483647 2147483647\n"
                "node big 0 0 99999999999 -99999999999 m1\nscale 1 1 1e38\n");
  f.file("c", "node n 0 0 2147483647 2147483647 m1\n");
  extflat::ReadOptions options;
  options.units_per_lambda = 1e-38F;
  const auto n = f.read("top", options);
  REQUIRE(n.ok());
  CHECK(n.value().nodes().size() == 2);
}

TEST_CASE("random bytes and mutated records never crash the reader", CAP_REQ) {
  const std::vector<std::string> seeds = {
      "node a 0 0 0 0 m1\nequiv a b\nmerge a[0:1] b[0:1]\nkillnode a\n",
      "use c u[0:1:5][0:1:5] 1 0 0 0 1 0\nport P 0 0 0 1 1 m1\n",
      "device mosfet nfet 0 0 1 1 2 8 S g 4 0 s 8 0 d 8 0\nsubstrate VSUB 0 0 0 0 space\n",
      "device subckt Short 0 0 1 1 x=1 None a 0 0 b 0 0\nrnode r 0 0\nattr a 0 0 1 1 m1 t\n",
  };
  std::mt19937 rng{12345};
  std::uniform_int_distribution<int> byte{0, 255};
  for (int round = 0; round < 300; ++round) {
    ExtFixture f;
    std::string text = seeds[static_cast<std::size_t>(round) % seeds.size()];
    const int flips = 1 + round % 8;
    for (int i = 0; i < flips && !text.empty(); ++i) {
      text[static_cast<std::size_t>(byte(rng)) % text.size()] = static_cast<char>(byte(rng));
    }
    f.file("top", text);
    f.file("c", text);
    extflat::ReadOptions options;
    options.global_nets = {"a", "b", "VSUB"};
    options.limits.max_instances = 1000;
    options.limits.max_connections = 1000;
    (void)f.read("top", options);
  }
  SUCCEED();
}

// Regression: every visit of a full flatten (nodes, kills, caps, devices,
// resistors, subcircuits) added to one instance count, so a design with a
// sixth of max_instances instances failed with EXTFLAT-READ-009.
TEST_CASE("a hierarchy with exactly max_instances instances flattens", CAP_REQ) {
  ExtFixture f;
  f.file("top", "use c u[0:99:1][0:0:0] 1 0 0 0 1 0\n");
  f.file("c", "node n 0 0 0 0 m1\ndevice mosfet nfet 0 0 1 1 2 8 VSUB n 4 0 n 8 0 n 8 0\n");
  auto options = ExtFixture::netlister_options();
  options.limits.max_instances = 100;
  const auto full = f.flat("top", ExtFixture::netlister_flags(), {}, options);
  REQUIRE(full.ok());
  CHECK(full.value().devices.size() == 100);

  options.limits.max_instances = 99;
  const auto over = f.flat("top", ExtFixture::netlister_flags(), {}, options);
  REQUIRE_FALSE(over.ok());
  CHECK(over.error().code == "EXTFLAT-READ-009");
}

TEST_CASE("a hierarchy with exactly max_instances instances flattens one level", CAP_REQ) {
  ExtFixture f;
  f.file("top", "use c u[0:99:1][0:0:0] 1 0 0 0 1 0\n");
  f.file("c", "node n 0 0 0 0 m1\n");
  auto options = ExtFixture::netlister_options();
  options.limits.max_instances = 100;
  auto h = f.read_hier("top", options);
  REQUIRE(h.ok());
  const auto level = extflat::flatten_one_level(h.value(), "top", ExtFixture::netlister_flags(), {}, f.sink);
  CHECK(level.ok());
}

TEST_CASE("merge records expanding to exactly max_connections connections flatten", CAP_REQ) {
  ExtFixture f;
  f.file("top", "node a 0 0 0 0 m1\nnode b 0 0 0 0 m1\nmerge a[0:9] b[0:9]\n");
  auto options = ExtFixture::netlister_options();
  options.limits.max_connections = 10;
  CHECK(f.flat("top", ExtFixture::netlister_flags(), {}, options).ok());
}

// Regression (both modes): a `port` record's number was never bounded, and
// the SPICE writer sized its port list by the highest one: `port a
// 1000000000` allocated about 32 GB of strings, and 2147483647 overflowed
// the implicit numbering past it.
TEST_CASE("a port number past the cap fails the read with EXTFLAT-READ-009",
          "[cap:ext-flattening-read][req:Use, port, cap, resist, attr and connect records]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    extflat::ReadOptions options;
    options.magic_compat = compat;
    for (const char* number : {"2147483647", "1000000000", "1048577"}) {
      ExtFixture f;
      f.file("top", std::string{"port a "} + number + " 0 0 1 1 m1\n");
      const auto n = f.read("top", options);
      REQUIRE_FALSE(n.ok());
      CHECK(n.error().code == "EXTFLAT-READ-009");
      CHECK(n.error().text == std::string{"/w/top.ext, line 1: port number "} + number + " is larger than 1048576");
    }
    ExtFixture at;
    at.file("top", "port a 1048576 0 0 1 1 m1\nport b -5 0 0 1 1 m1\n");
    CHECK(at.read("top", options).ok());
  }
}

// Regression (both modes; fuzz seed regress-timeout-08653c19...): an array
// use was walked element by element until the instance cap, each element
// flattening its cell and warning about its merges, so a 10^10-element
// array took seconds to fail. An array larger than what is left of the
// cap is now refused before its first element.
TEST_CASE("an array use larger than the instance cap is refused before any element", CAP_REQ) {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ExtFixture f;
    f.file("top", "use leaf l[0:100000:1][0:100000:1] 1 0 0 0 1 0\n");
    f.file("leaf", "node a 1 0 0 0 m1\nmerge x/A y/A\nmerge y/A a\n");
    extflat::ReadOptions options;
    options.magic_compat = compat;
    const auto n = f.read("top", options);
    REQUIRE_FALSE(n.ok());
    CHECK(n.error().code == "EXTFLAT-READ-009");
    CHECK(n.error().text == "the flattened .ext hierarchy has more than 4194304 instances");
    CHECK(f.texts("EXTFLAT-READ-007").empty());  // no element was flattened

    ExtFixture full;  // both ranges at their widest: the count saturates
    full.file("top", "use leaf l[-2147483648:2147483647:1][-2147483648:2147483647:1] 1 0 0 0 1 0\n");
    full.file("leaf", "node a 1 0 0 0 m1\n");
    const auto wide = full.read("top", options);
    REQUIRE_FALSE(wide.ok());
    CHECK(wide.error().code == "EXTFLAT-READ-009");
  }
}

TEST_CASE("an array use exactly filling the instance cap flattens and one past it is refused", CAP_REQ) {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    extflat::ReadOptions options;
    options.magic_compat = compat;
    options.limits.max_instances = 100;
    ExtFixture f;
    f.file("top", "use leaf l[0:9:1][0:9:1] 1 0 0 0 1 0\n");
    f.file("leaf", "node a 1 0 0 0 m1\nmerge x/A a\n");
    const auto at = f.read("top", options);
    REQUIRE(at.ok());
    CHECK(names(at.value()).size() == 100);

    ExtFixture g;
    g.file("top", "use leaf l[0:9:1][0:9:1] 1 0 0 0 1 0\n");
    g.file("leaf", "node a 1 0 0 0 m1\nmerge x/A a\n");
    options.limits.max_instances = 99;
    const auto over = g.read("top", options);
    REQUIRE_FALSE(over.ok());
    CHECK(over.error().text == "the flattened .ext hierarchy has more than 99 instances");
    CHECK(g.texts("EXTFLAT-READ-007").empty());
  }
}

TEST_CASE("an array inside an array is refused once the outer elements spend the cap", CAP_REQ) {
  ExtFixture f;
  f.file("top", "use mid m[0:3:1][0:0:0] 1 0 0 0 1 0\n");
  f.file("mid", "use leaf l[0:9:1][0:0:0] 1 0 0 0 1 0\n");
  f.file("leaf", "node a 1 0 0 0 m1\n");
  extflat::ReadOptions options;
  options.limits.max_instances = 44;  // 4 + 4 * 10
  CHECK(f.read("top", options).ok());
  options.limits.max_instances = 43;
  const auto over = f.read("top", options);
  REQUIRE_FALSE(over.ok());
  CHECK(over.error().code == "EXTFLAT-READ-009");
}

TEST_CASE("an arrayed merge past the connection cap is refused before any pair", CAP_REQ) {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    extflat::ReadOptions options;
    options.magic_compat = compat;
    options.limits.max_connections = 9;
    ExtFixture f;
    f.file("top", "node a 0 0 0 0 m1\nmerge x[0:9] y[0:9]\n");
    const auto n = f.read("top", options);
    REQUIRE_FALSE(n.ok());
    CHECK(n.error().text == "the .ext merge records expand to more than 9 connections");
    CHECK(f.texts("EXTFLAT-READ-007").empty());  // no pair was looked up

    ExtFixture g;
    g.file("top", "node a 0 0 0 0 m1\nmerge x[0:2,0:2] y[0:2,0:2]\n");
    CHECK(g.read("top", options).ok());
    options.limits.max_connections = 8;
    CHECK_FALSE(g.read("top", options).ok());
  }
}
