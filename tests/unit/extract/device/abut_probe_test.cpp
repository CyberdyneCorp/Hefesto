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

#include <algorithm>
#include <string>
#include <vector>

#include "../../../../src/core/src/extract/device/device_internal.hpp"
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

// A deliberate multi-type gate line (sky130's `nfet,scnfet` style): one
// record lists two gate types and no other line names either type. The
// probe's `pfet` stands in for the second type; one transistor's gate is
// nfet over its lower half and pfet over its upper half, with one ndiff
// source and drain along its whole width.
std::unique_ptr<Case> multi_type_gate(bool magic_compat) {
  std::string tech = probe_tech("probe");
  const std::string lines =
      "    device mosfet cmos nfet,pfet ndiff,pdiff None GND!\n"
      "    device mosfet nmos nfet ndiff None GND!\n"
      "    device mosfet pmos pfet pdiff None VDD!\n";
  const std::size_t at = tech.find(lines);
  REQUIRE(at != std::string::npos);
  tech.replace(at, lines.size(), "    device mosfet nmos nfet,pfet ndiff None GND!\n");
  const std::string mag =
      "magic\ntech exttest\ntimestamp 100\n"
      "<< nfet >>\nrect 0 0 2 4\n<< pfet >>\nrect 0 4 2 8\n"
      "<< ndiff >>\nrect -3 0 0 8\nrect 2 0 5 8\n"
      "<< poly >>\nrect 0 -2 2 0\nrect 0 8 2 10\n"
      "<< labels >>\nrlabel ndiff -2 4 -2 4 0 s\nrlabel ndiff 3 4 3 4 0 d\nrlabel poly 1 -1 1 -1 0 g\n<< end >>\n";
  Options o;
  o.magic_compat = magic_compat;
  return extract_texts(tech, mag, o);
}

TEST_CASE("Q-EXTRACT-41: a multi-type gate line with one record is one device (magic-compat ON)",
          "[cap:extract-devices][req:Device record selection]") {
  const auto c = multi_type_gate(true);
  // The walk covers the representative tile's gate node only (the pfet
  // half, unlabelled): W is 4 and the nfet half is lost, as in EX-P03.
  CHECK(c->regions.size() == 1);
  CHECK(c->lines() ==
        std::vector<std::string>{R"(device mosfet nmos 0 0 1 1 2 4 "GND!" "@0_4" 2 0 "s" 4 12,11 "d" 4 12,11)"});
}

TEST_CASE("Q-EXTRACT-41: a multi-type gate line with one record splits per type (magic-compat OFF)",
          "[cap:extract-devices][req:Device record selection]") {
  // As implemented in R4: OFF splits the device connectivity per gate type
  // even when one record lists both types, so one transistor becomes two.
  const auto c = multi_type_gate(false);
  CHECK(c->regions.size() == 2);
  CHECK(c->lines() == std::vector<std::string>{
                          R"(device mosfet nmos 0 0 1 1 2 4 "GND!" "g" 2 0 "s" 4 12,11 "d" 4 12,11)",
                          R"(device mosfet nmos 0 4 1 5 2 4 "GND!" "@0_4" 2 0 "s" 4 12,11 "d" 4 12,11)"});
}

TEST_CASE("Q-EXTRACT-36 (f): a record outside the tile type's list ends the match search after one turn",
          "[cap:extract-devices][req:Device record selection]") {
  // extDevFindMatch walks from deventry's successor and wraps to the head of
  // exts_device[t] (ExtBasic.c:3467-3474): a record from another type's list
  // is never met again, so the walk never ends when no candidate agrees.
  // The probe style's `pmos` line names pfet only; asked for nfet with a
  // pdiff terminal matched, neither nfet record (nmos: ndiff; cmos:
  // ndiff,pdiff) has the same terminal types.
  namespace device_detail = hefesto::extract::device_detail;
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    const auto c = probe("probe", "abut", compat);
    const tech::TileType nfet = type_of(*c->tech, "nfet");
    const tech::TileType pfet = type_of(*c->tech, "pfet");
    const auto pfets = c->style.records_for(pfet);
    const auto pmos = std::find_if(pfets.begin(), pfets.end(), [](const auto* p) { return p->model == "pmos"; });
    REQUIRE(pmos != pfets.end());
    const auto nfets = c->style.records_for(nfet);
    REQUIRE(std::find(nfets.begin(), nfets.end(), *pmos) == nfets.end());

    extract::DeviceInputs in;
    extract::DeviceResult out;
    device_detail::Context ctx{in, *c->cell, *c->tech, c->style, {}, out, {}};
    device_detail::TransRec tr;
    tr.rec = *pmos;
    tr.match = device_detail::kMatchTerm;
    tr.terms[0].node = 0;
    CHECK(device_detail::find_match(ctx, tr, *pmos, nfet) == nullptr);
    tr.match = 0;  // nothing to agree on: the next record, the head of nfet's list
    CHECK(device_detail::find_match(ctx, tr, *pmos, nfet) == nfets.front());
  }
}

}  // namespace
}  // namespace hefesto::test::dev
