// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Bus and vector notation (capability schematic-netlisting "Bus and vector
// notation"; tasks.md 8.1).
//
// Owner: NETLIST.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/schem/net/bus.hpp"

namespace schem = hefesto::schem;
using Bits = std::vector<std::string>;

namespace {

std::string joined(std::string_view name) {
  return schem::join_bits(schem::expand_bus(name), ",");
}

}  // namespace

TEST_CASE("Descending bus", "[cap:schematic-netlisting][req:Bus and vector notation]") {
  const auto bus = schem::parse_bus("A[3:0]");
  CHECK(bus.bits == Bits{"A[3]", "A[2]", "A[1]", "A[0]"});
  CHECK(bus.width() == 4);
  CHECK_FALSE(bus.syntax_error);
}

TEST_CASE("Stepped and bracketless forms", "[cap:schematic-netlisting][req:Bus and vector notation]") {
  CHECK(joined("D[7:0:2]") == "D[7],D[5],D[3],D[1]");
  CHECK(joined("Q[2..0]") == "Q2,Q1,Q0");
  CHECK(joined("Q[0..4..2]") == "Q0,Q2,Q4");
  CHECK(joined("A[0:5:3]") == "A[0],A[3]");  // stops before passing j
}

TEST_CASE("Repetition", "[cap:schematic-netlisting][req:Bus and vector notation]") {
  CHECK(joined("2*(a,b)") == "a,b,a,b");
  CHECK(joined("(a,b)*2") == "a,a,b,b");
  CHECK(joined("3*x") == "x,x,x");
}

TEST_CASE("Bus replacement characters", "[cap:schematic-netlisting][req:Bus and vector notation]") {
  CHECK(schem::replace_bus_chars(joined("A[1:0]"), "<>") == "A<1>,A<0>");
  CHECK(schem::replace_bus_chars("A[1]", "<") == "A[1]");  // only exactly two characters apply
}

TEST_CASE("bus notation: lists, index lists, trailers and repeats with extension",
          "[cap:schematic-netlisting][req:Bus and vector notation]") {
  CHECK(joined("a,b[1:0]") == "a,b[1],b[0]");
  CHECK(joined("a, b") == "a,b");
  CHECK(joined("A[3:2,0]") == "A[3],A[2],A[0]");
  CHECK(joined("A[1:0]_x") == "A[1]_x,A[0]_x");
  CHECK(joined("A[0:1:4:2]") == "A[0],A[1],A[4],A[5]");
  CHECK(joined("A[1:0:10:2]") == "A[1],A[0],A[11],A[10]");
  CHECK(joined("A[2:2]") == "A[2]");
}

TEST_CASE("bus notation: plain names and $ names are returned unchanged",
          "[cap:schematic-netlisting][req:Bus and vector notation]") {
  CHECK(schem::expand_bus("DATA[3]") == Bits{"DATA[3]"});
  CHECK(schem::expand_bus("$x,y") == Bits{"$x,y"});
  CHECK(schem::expand_bus("") == Bits{""});
  CHECK(schem::parse_bus("A[3]").width() == 1);
}

TEST_CASE("bus notation: a literal index is kept with unknown width",
          "[cap:schematic-netlisting][req:Bus and vector notation]") {
  const auto bus = schem::parse_bus("A[n],B");
  CHECK(bus.bits == Bits{"A[n]", "B"});
  CHECK_FALSE(bus.width_known);
  CHECK_FALSE(bus.syntax_error);
}

TEST_CASE("bus notation: a syntax error keeps the string with unknown width",
          "[cap:schematic-netlisting][req:Bus and vector notation]") {
  for (const char* bad : {"A[1:0", "(a,b", "a,,b", "A[1:x]", "A[1:0..2]", "A[0:4:0]", "A[0:1:2:0]", "a*", "a,b)"}) {
    INFO(bad);
    const auto bus = schem::parse_bus(bad);
    CHECK(bus.syntax_error);
    CHECK(bus.bits == Bits{bad});
    CHECK_FALSE(bus.width_known);
    CHECK(bus.width() == 1);
  }
}

// Regression (fuzz_schem_bus, CI run 44867374318): a repetition count of 0
// expanded to no name at all, breaking "an expansion always gives at least
// the input back". A count is at least 1; 0 is a syntax error, like a step
// or rep of 0 in an index.
TEST_CASE("bus notation: a repetition count of 0 is a syntax error",
          "[cap:schematic-netlisting][req:Bus and vector notation]") {
  for (const char* bad : {"a*0", "0*a", "(a,b)*0", "2*a*0", "3*(a,_,d)*0", "x,(a)*00"}) {
    INFO(bad);
    const auto bus = schem::parse_bus(bad);
    CHECK(bus.syntax_error);
    CHECK(bus.bits == Bits{bad});
    CHECK_FALSE(bus.width_known);
  }
  CHECK(joined("a*1") == "a");
  CHECK(joined("1*(a,b)") == "a,b");
}

TEST_CASE("bus notation: hostile sizes are refused without allocating them",
          "[cap:schematic-netlisting][req:Bus and vector notation]") {
  for (const char* big : {"A[0:9223372036854775807]", "A[-9223372036854775807:9223372036854775807]",
                          "1000000*(1000000*a)", "(a,b)*99999999999999999999", "A[0:0:1:9223372036854775807]",
                          "A[0:1048576]"}) {
    INFO(big);
    const auto bus = schem::parse_bus(big);
    CHECK(bus.too_large);
    CHECK(bus.bits == Bits{big});
  }
  CHECK(schem::parse_bus("A[1048575:0]").width() == schem::kMaxBusBits);

  const auto overflow = schem::parse_bus("A[9223372036854775807:9223372036854775806:9223372036854775807:2]");
  CHECK(overflow.syntax_error);  // the repeated values would overflow

  std::string deep(10000, '(');
  deep += "a";
  deep += std::string(10000, ')');
  deep += ",b";
  CHECK(schem::parse_bus(deep).too_large);
}

// Regression: each leading `n*` recursed once, so a long run of them in a
// hostile name overflowed the stack. They are now read in a loop.
TEST_CASE("bus notation: a long run of leading repeat counts does not recurse",
          "[cap:schematic-netlisting][req:Bus and vector notation]") {
  std::string many;
  for (int i = 0; i < 300000; ++i) many += "1*";
  many += "a";
  CHECK(schem::parse_bus(many).bits == Bits{"a"});
  CHECK(joined("2*3*a") == "a,a,a,a,a,a");
  CHECK(joined("2 * 2*(a,b)") == "a,b,a,b,a,b,a,b");
  CHECK(schem::parse_bus("1024*1024*a").width() == schem::kMaxBusBits);
  CHECK(schem::parse_bus("1024*1025*a").too_large);
  CHECK(schem::parse_bus("1048576*1048576*1048576*a").too_large);
  CHECK(schem::parse_bus("2*3*(a").syntax_error);
}
