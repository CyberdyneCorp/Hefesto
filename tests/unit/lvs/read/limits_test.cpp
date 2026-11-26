// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Decision LVS-READ-025 (approved under the standing rule for crafted-input
// limits): a read stops with LVS-READ-025 after 65536 .INCLUDE reads or
// after 64 times the logical-line limit of included bytes; the include
// depth counts the top file; the device limit is counted per netlist
// database. Each boundary is pinned in both netgen-compat modes, since the
// limits apply in both.
//
// Owner: LVSREAD.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/lvs/net/codes.hpp"
#include "hefesto/lvs/read/codes.hpp"
#include "read_fixture.hpp"

namespace {

using lvs_test::Reading;
namespace lvs = hefesto::lvs;
namespace codes = hefesto::lvs::read_codes;

constexpr const char* kLimits = "[cap:lvs-netlist-input][req:Input limits]";

/// A top file with `count` .INCLUDE lines of one small file.
void include_many(Reading& r, std::size_t count) {
  std::string top;
  top.reserve(count * 16);
  for (std::size_t i = 0; i < count; ++i) {
    top += ".include e.sp\n";
  }
  r.add_file("/w/top.sp", top);
  r.add_file("/w/e.sp", "* e\n");
}

}  // namespace

TEST_CASE("65536 include reads are allowed and the next one stops the read", kLimits) {
  for (const bool compat : {true, false}) {
    Reading ok;
    ok.options.netgen_compat = compat;
    include_many(ok, 65536);
    CHECK(ok.read_file("/w/top.sp").ok());

    Reading over;
    over.options.netgen_compat = compat;
    include_many(over, 65537);
    const auto read = over.read_file("/w/top.sp");
    REQUIRE_FALSE(read.ok());
    CHECK(read.error().code == codes::kIncludeCount);
    CHECK(read.error().text == "more than 65536 include reads in one netlist at /w/top.sp line 65537");
  }
}

TEST_CASE("Included bytes stop at 64 times the logical-line limit", kLimits) {
  // With a 1 KiB logical-line limit the include bytes may reach 64 KiB.
  for (const bool compat : {true, false}) {
    const std::string block = "*" + std::string(1022, 'x') + "\n";  // 1024 bytes
    for (const std::size_t times : {std::size_t{64}, std::size_t{65}}) {
      Reading r;
      r.options.netgen_compat = compat;
      r.options.limits.logical_line = 1024;
      std::string top;
      for (std::size_t i = 0; i < times; ++i) {
        top += ".include block.sp\n";
      }
      r.add_file("/w/top.sp", top);
      r.add_file("/w/block.sp", block);
      const auto read = r.read_file("/w/top.sp");
      if (times == 64) {
        CHECK(read.ok());
      } else {
        REQUIRE_FALSE(read.ok());
        CHECK(read.error().code == codes::kIncludeCount);
        CHECK(read.error().text == "more than 65536 bytes of include files in one netlist at /w/top.sp line 65");
      }
    }
  }
}

TEST_CASE("The include depth counts the top file", kLimits) {
  // Depth 3 is the top file plus two nested includes.
  for (const bool compat : {true, false}) {
    Reading r;
    r.options.netgen_compat = compat;
    r.options.limits.include_depth = 3;
    r.add_file("/w/a.sp", ".include b.sp\n");
    r.add_file("/w/b.sp", ".include c.sp\n");
    r.add_file("/w/c.sp", "R1 x y 1k\n");
    CHECK(r.read_file("/w/a.sp").ok());

    Reading deeper;
    deeper.options.netgen_compat = compat;
    deeper.options.limits.include_depth = 3;
    deeper.add_file("/w/a.sp", ".include b.sp\n");
    deeper.add_file("/w/b.sp", ".include c.sp\n");
    deeper.add_file("/w/c.sp", ".include d.sp\n");
    deeper.add_file("/w/d.sp", "R1 x y 1k\n");
    const auto read = deeper.read_file("/w/a.sp");
    REQUIRE_FALSE(read.ok());
    CHECK(read.error().code == codes::kIncludeDepth);
    CHECK(read.error().text == "include depth limit of 3 exceeded in /w/c.sp line 1");
  }
}

TEST_CASE("The device limit is counted per netlist database", kLimits) {
  for (const bool compat : {true, false}) {
    Reading shared;
    shared.options.netgen_compat = compat;
    shared.options.limits.devices = 3;
    REQUIRE(shared.read("R1 a b 1k\nR2 a b 1k\n", "one.spice").ok());
    const auto read = shared.read("R3 a b 1k\nR4 a b 1k\n", "two.spice");
    REQUIRE_FALSE(read.ok());
    CHECK(read.error().code == lvs::net_codes::kDeviceLimit);
    CHECK(read.error().text == "the device limit of 3 per netlist database is exceeded in two.spice line 2");

    // Another database counts from zero.
    Reading other;
    other.options.netgen_compat = compat;
    other.options.limits.devices = 3;
    CHECK(other.read("R3 a b 1k\nR4 a b 1k\n", "two.spice").ok());
  }
}
