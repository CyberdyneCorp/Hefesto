// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EX-P03 (tests/data/extract/device/abut_probe): an nfet and a pfet touching
// in one device region (the `cmos` line lists both gate types, so the device
// connectivity joins them). Pinned against the capture from Magic.
// Owner: EXTDEV (written by EXTHIER, M7 round 4).

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <string>

#include "device_fixture.hpp"

namespace hefesto::test::dev {
namespace {

std::string probe_dir() {
  return std::string(HEFESTO_SOURCE_DIR) + "/tests/data/extract/device/abut_probe/EXTRACT/cases/EX-P03/input/";
}

// The probe's technology with only style `name` left (the fixture reads the
// first style).
std::string probe_tech(const std::string& name) {
  std::string text = read_text(probe_dir() + "exttest.tech");
  const std::size_t first = text.find("style probe\n");
  const std::size_t second = text.find("style probeid\n");
  REQUIRE(first != std::string::npos);
  REQUIRE(second != std::string::npos);
  if (name == "probe") {
    const std::size_t end = text.find("end\n", second);
    return text.substr(0, second) + text.substr(end);
  }
  return text.substr(0, first) + text.substr(second);
}

std::unique_ptr<Case> probe(const std::string& style, const std::string& cell, bool magic_compat) {
  Options o;
  o.magic_compat = magic_compat;
  o.cell = cell;
  return extract_texts(probe_tech(style), read_text(probe_dir() + cell + ".mag"), o);
}

TEST_CASE("EX-P03 probe: the Hefesto records", "[.probe][cap:extract-devices][req:Device record selection]") {
  for (const auto& [style, cell] : {std::pair{"probe", "abut"}, std::pair{"probe", "abutrev"}, std::pair{"probeid", "abutid"}}) {
    for (const bool compat : {true, false}) {
      const auto c = probe(style, cell, compat);
      std::cout << cell << " compat=" << compat << " regions=" << c->regions.size() << "\n";
      for (const auto& l : c->lines()) {
        std::cout << "  " << l << "\n";
      }
    }
  }
}

}  // namespace
}  // namespace hefesto::test::dev
