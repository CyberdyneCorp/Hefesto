// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// EX-P03 (tests/data/extract/device/abut_probe, capture-only): an nfet and a
// pfet touching in one device region (the `cmos` line lists both gate types,
// so the device connectivity joins them). Magic (golden-capture run
// 45553746645) writes one device for the region: the record and terminals
// of its representative tile's type and node (the upper pfet's), so the
// lower transistor is lost; with every record needing thkox and thkox over
// the nfet only, it prints `No matching device for pfet with no ID layer`
// and writes none, and `extract all` returns. magic-compat ON copies it;
// OFF makes each device type its own device (proposed Q-EXTRACT-41).
// Owner: EXTDEV (written by EXTHIER, M7 round 4).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "device_fixture.hpp"

namespace hefesto::test::dev {
namespace {

std::string probe_root() {
  return std::string(HEFESTO_SOURCE_DIR) + "/tests/data/extract/device/abut_probe/EXTRACT/cases/EX-P03/";
}

// The probe's technology with only style `name` left (the fixture reads the
// first style).
std::string probe_tech(const std::string& name) {
  const std::string text = read_text(probe_root() + "input/exttest.tech");
  const std::size_t first = text.find("style probe\n");
  const std::size_t second = text.find("style probeid\n");
  REQUIRE(first != std::string::npos);
  REQUIRE(second != std::string::npos);
  if (name == "probe") {
    return text.substr(0, second) + text.substr(text.find("end\n", second));
  }
  return text.substr(0, first) + text.substr(second);
}

std::unique_ptr<Case> probe(const std::string& style, const std::string& cell, bool magic_compat) {
  Options o;
  o.magic_compat = magic_compat;
  o.cell = cell;
  return extract_texts(probe_tech(style), read_text(probe_root() + "input/" + cell + ".mag"), o);
}

std::vector<std::string> captured_devices(const std::string& cell) {
  std::vector<std::string> out;
  std::istringstream is(read_text(probe_root() + "expected/collected/input/" + cell + ".ext"));
  for (std::string line; std::getline(is, line);) {
    if (line.rfind("device ", 0) == 0) {
      out.push_back(line);
    }
  }
  return out;
}

TEST_CASE("EX-P03: two device types in one region are one device of the representative tile (magic-compat ON)",
          "[cap:extract-devices][req:Device record selection]") {
  for (const std::string cell : {"abut", "abutrev"}) {
    INFO(cell);
    const auto c = probe("probe", cell, true);
    CHECK(c->regions.size() == 1);
    CHECK(c->lines() == captured_devices(cell));
  }
  CHECK(captured_devices("abut") ==
        std::vector<std::string>{R"(device mosfet pmos 0 0 1 1 2 4 "VDD!" "g2" 2 0 "s2" 4 12,14 "d2" 4 12,14)"});
}

TEST_CASE("EX-P03: two device types in one region are two devices (magic-compat OFF)",
          "[cap:extract-devices][req:Device record selection]") {
  const auto c = probe("probe", "abut", false);
  CHECK(c->regions.size() == 2);
  CHECK(c->lines() == std::vector<std::string>{
                          R"(device mosfet nmos 0 0 1 1 2 4 "GND!" "g1" 2 0 "s1" 4 12,14 "d1" 4 12,14)",
                          R"(device mosfet pmos 0 4 1 5 2 4 "VDD!" "g2" 2 0 "s2" 4 12,14 "d2" 4 12,14)"});
}

TEST_CASE("EX-P03: an ID layer over one of two device types ends the record search (both modes)",
          "[cap:extract-devices][req:Device record selection]") {
  CHECK(read_text(probe_root() + "expected/stdout.txt").find("extract abutid: returned") != std::string::npos);
  CHECK(captured_devices("abutid").empty());
  const auto on = probe("probeid", "abutid", true);
  CHECK(on->lines().empty());
  const auto off = probe("probeid", "abutid", false);
  CHECK(off->lines() == std::vector<std::string>{
                            R"(device mosfet nmos 0 0 1 1 2 4 "GND!" "g1" 2 0 "s1" 4 12,14 "d1" 4 12,14)"});
}

}  // namespace
}  // namespace hefesto::test::dev
