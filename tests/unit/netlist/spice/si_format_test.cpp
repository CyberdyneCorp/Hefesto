// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// esSIvalue (capability netlist-spice-options "SI value formatting"). The
// pinned values are the boundaries Magic printed in NL-SPICE-03 and the
// spec's scenarios; they must hold on every CI leg (Linux GCC, macOS
// Clang), so they also pin the single-precision steps.

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "hefesto/netlist/spice/settings.hpp"

namespace spice = hefesto::netlist::spice;

TEST_CASE("SI values follow esSIvalue's suffixes and five decimals",
          "[cap:netlist-spice-options][req:SI value formatting]") {
  CHECK(spice::format_si(1.8e-7) == "0.18u");
  CHECK(spice::format_si(0.0) == "0");
  CHECK(spice::format_si(1500.0) == "1.5k");
  CHECK(spice::format_si(0.01) == "10m");
}

TEST_CASE("SI values at Magic's boundaries are pinned", "[cap:netlist-spice-options][req:SI value formatting]") {
  // Capacitor values of NL-SPICE-03 (fF x 1e-15) and its MOSFET widths
  // (1e-6 x w x 0.01).
  const std::vector<std::pair<double, std::string>> pinned{
      {1.0E-15 * 0.0001, "0"},     {1.0E-15 * static_cast<double>(99.99F), "99.99f"}, {1.0E-15 * 100, "0.1p"},
      {1.0E-15 * 100010, "0.10001n"}, {1.0E-15 * 1e8, "100n"},  {1.0E-15 * 1e13, "10m"},
      {1.0E-15 * 9.999e17, "0.9999k"}, {1.0E-15 * 1e18, "1k"},  {1.0E-15 * 1e24, "1G"},
      {1.0E-6 * 1 * 0.01, "10n"},  {1.0E-6 * 2 * 0.01, "20n"},  {1.0E-6 * 20 * 0.01, "0.2u"},
      {1.0E-6 * 200 * 0.01, "2u"}, {1.0E-6 * 2000 * 0.01, "20u"}, {1.0E-6 * 20000 * 0.01, "0.2m"},
      {1.0E-7, "100n"},            {1.0E-6 * 0.05 * 8, "0.4u"}, {3.0E-15, "3f"},
      {1.0E-12 * 30 / 3.5, "8.57143p"}, {1.0E-6 * 16 / 3.0, "5.33333u"}, {-2.5e-12, "-2.5p"},
      {-1.0E-19, "-0"},            {1.0E-8, "10n"},            {1.0E-6 * 5 / 100.0, "50n"},
  };
  for (const auto& [value, text] : pinned) {
    INFO(value);
    CHECK(spice::format_si(value) == text);
  }
}

TEST_CASE("SI formatting of an infinite value terminates", "[cap:netlist-spice-options][req:SI value formatting]") {
  // Regression: Magic's nDecimals divides an infinite value by ten forever
  // (a hang); Hefesto prints printf's text in both modes.
  CHECK(spice::format_si(std::numeric_limits<double>::infinity()) == "inf");
  CHECK(spice::format_si(-std::numeric_limits<double>::infinity()) == "-inf");
  CHECK(spice::format_si(1.0E40) == "inf");  // beyond float: infinite
}
