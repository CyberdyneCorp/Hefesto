// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The correctly rounded atan and the fringe fraction of Open Question 3 (a)
// (openspec/changes/m7-extraction/design.md D4, D22; tasks.md 6.4): bit for
// bit against a 300-bit reference on every leg, and pinned fringe_fraction
// values. Owner: EXTCAP.

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>

#include "hefesto/extract/cap/caps.hpp"
#include "util/cr_atan.hpp"

namespace {

using hefesto::extract::fringe_fraction;
using hefesto::util::cr_atan;

std::uint64_t bits(double v) {
  return std::bit_cast<std::uint64_t>(v);
}

}  // namespace

TEST_CASE("cr_atan matches the correctly rounded reference bit for bit",
          "[cap:extract-capacitance][req:Fringe-halo sideoverlap]") {
  std::ifstream in(std::string{HEFESTO_TEST_DATA_DIR} + "/atan_reference.txt");
  REQUIRE(in.good());
  std::string xs;
  std::string ys;
  int count = 0;
  int mismatches = 0;
  while (in >> xs >> ys) {
    const double x = std::strtod(xs.c_str(), nullptr);
    const double want = std::strtod(ys.c_str(), nullptr);
    const double got = cr_atan(x);
    if (bits(got) != bits(want)) {
      ++mismatches;
      UNSCOPED_INFO("x=" << xs << " want=" << ys);
    }
    ++count;
  }
  CHECK(count == 4000);
  CHECK(mismatches == 0);
}

TEST_CASE("cr_atan special values", "[cap:extract-capacitance][req:Fringe-halo sideoverlap]") {
  CHECK(bits(cr_atan(0.0)) == bits(0.0));
  CHECK(bits(cr_atan(-0.0)) == bits(-0.0));
  CHECK(std::isnan(cr_atan(std::numeric_limits<double>::quiet_NaN())));
  CHECK(cr_atan(std::numeric_limits<double>::infinity()) == 0x1.921fb54442d18p+0);
  CHECK(cr_atan(-std::numeric_limits<double>::infinity()) == -0x1.921fb54442d18p+0);
  CHECK(cr_atan(1.0) == 0x1.921fb54442d18p-1);
  CHECK(cr_atan(-1.0) == -0x1.921fb54442d18p-1);
}

TEST_CASE("fringe_fraction is 0.6366 * atan(mult * d) with pinned values",
          "[cap:extract-capacitance][req:Fringe-halo sideoverlap]") {
  // Multipliers are Magic's float(cap * 0.02) (exts_overlapMult), widened.
  CHECK(static_cast<double>(static_cast<float>(20.0 * 0.02)) == 0.4000000059604645);
  CHECK(fringe_fraction(0.4000000059604645, 10) == 0x1.b022cd9ae6d89p-1);
  CHECK(fringe_fraction(0.4000000059604645, 0) == 0.0);
  CHECK(fringe_fraction(0.4000000059604645, 3) == 0x1.1d8aaa65ab966p-1);
  CHECK(fringe_fraction(0.11999999731779099, 7) == 0x1.c770f53415693p-2);
  CHECK(fringe_fraction(0.20000000298023224, 1) == 0x1.015b162dea677p-3);
  CHECK(fringe_fraction(0.6000000238418579, 10) == 0x1.ca27da25d5bfcp-1);
  CHECK(fringe_fraction(0.25, 25) == 0x1.cc45aa3d18206p-1);
  CHECK(fringe_fraction(0.18000000715255737, 1000) == 0x1.fe2c603d1bc16p-1);
  // The constant is 0.6366, not 2/pi: the far limit is 0.6366 * pi/2.
  CHECK(fringe_fraction(1.0, 1e300) == 0.6366 * 0x1.921fb54442d18p+0);
}
