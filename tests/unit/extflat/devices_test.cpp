// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ext-flattening-read, M7 round 2: device records of every class (pinned to
// NL-FLAT-04's committed input and the device order of its captured
// netlist), `parameters` records (NL-FLAT-05; Q-NETLIST-8, Q-NETLIST-9),
// the `resist` offset (NL-FLAT-09; Q-NETLIST-12), the `vsource` record
// (Q-NETLIST-51) and the `.res.ext` overlay. Owner: EXTFLAT.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "ext_fixture.hpp"

namespace extflat = hefesto::extflat;
using extflat::test::device_places;
using extflat::test::ExtFixture;
using extflat::test::GoldenCase;
using extflat::test::node_named;
using extflat::test::nltest_options;
using extflat::test::terminal_nodes;
using Catch::Matchers::WithinAbs;
using Strings = std::vector<std::string>;

namespace {

const extflat::FlatDevice* device_at(const extflat::Netlist& n, int x) {
  for (const auto& d : n.devices) {
    if (d.local_area.x0 == x) {
      return &d;
    }
  }
  return nullptr;
}

}  // namespace

TEST_CASE("NL-FLAT-04: every device class is read and visited in device-table order",
          "[cap:ext-flattening-read][req:Device records]") {
  GoldenCase g{"NL-FLAT-04"};
  const auto n = g.flat("top", extflat::test::ExtFixture::netlister_flags(), {}, nltest_options());
  REQUIRE(n.ok());
  // The order of the captured top.spice: keys "<x>x<y>_<t>" (coordinates
  // times locScale 5) with nfet 13, npn 17, rppd 16, mimcapm 20, dn 11 and
  // the unknown names from 21. In file coordinates: 0 140 12 70 20 6 60 90
  // 40 110 80 130 100 30 150 120 85 50.
  CHECK(device_places(n.value()) == Strings{"0x0",   "700x0", "60x0",  "350x0", "100x0", "30x0",
                                            "300x0", "450x0", "200x0", "550x0", "400x0", "650x0",
                                            "500x0", "150x0", "750x0", "600x0", "425x0", "250x0"});
  CHECK(g.sink.snapshot().empty());

  const auto* mos = device_at(n.value(), 0);
  REQUIRE(mos != nullptr);
  CHECK(mos->device_class == "mosfet");
  CHECK(mos->model == "nfet");
  CHECK(mos->length == 2);
  CHECK(mos->width == 4);
  CHECK(mos->substrate == "GND!");
  CHECK(terminal_nodes(*mos) == Strings{"g", "s", "d"});
  CHECK(mos->terminals[0].length == 8);

  const auto* fet = device_at(n.value(), 60);  // legacy fet: L = gate/2, W = (s + d)/2
  REQUIRE(fet != nullptr);
  CHECK(fet->device_class == "fet");
  CHECK(fet->device_area == 8);
  CHECK(fet->perimeter == 12);
  CHECK(fet->length == 4);
  CHECK(fet->width == 4);

  const auto* bjt = device_at(n.value(), 100);
  REQUIRE(bjt != nullptr);
  CHECK(bjt->substrate == "GND!");
  CHECK(terminal_nodes(*bjt) == Strings{"b", "e", "c"});

  const auto* res = device_at(n.value(), 150);  // devres None: the value, no substrate
  REQUIRE(res != nullptr);
  CHECK_THAT(static_cast<double>(res->res), WithinAbs(25.5, 1e-6));
  CHECK(res->substrate.empty());
  CHECK(terminal_nodes(*res) == Strings{"rb", "ra", "rc"});

  const auto* rppd = device_at(n.value(), 200);  // modelled: L W, a "None" substrate
  REQUIRE(rppd != nullptr);
  CHECK(rppd->length == 10);
  CHECK(rppd->width == 2);
  CHECK(rppd->substrate.empty());
  CHECK(terminal_nodes(*rppd) == Strings{"rb2", "ra", "rc"});

  const auto* cap = device_at(n.value(), 250);
  REQUIRE(cap != nullptr);
  CHECK_THAT(static_cast<double>(cap->cap), WithinAbs(12.5, 1e-6));
  CHECK(terminal_nodes(*cap) == Strings{"ct", "cb"});

  const auto* diode = device_at(n.value(), 400);
  REQUIRE(diode != nullptr);
  CHECK(diode->device_class == "diode");
  CHECK(terminal_nodes(*diode) == Strings{"k", "GND!"});

  const auto* msub = device_at(n.value(), 600);  // undeclared parameters kept verbatim, newest first
  REQUIRE(msub != nullptr);
  CHECK(msub->device_class == "msubckt");
  CHECK(msub->params == Strings{"w=4", "l=2"});
  CHECK(msub->substrate == "GND!");
  CHECK(terminal_nodes(*msub) == Strings{"g", "s", "d"});
  CHECK(msub->terminals[0].node_index != extflat::kNoNode);
  CHECK(n.value().nodes()[msub->terminals[0].node_index].name == "g");
}

TEST_CASE("a device record lands on an existing device by tile type and replaces its terminals",
          "[cap:ext-flattening-read][req:Device records]") {
  ExtFixture f;
  f.file("top",
         "node a 0 0 0 0 m1\n"
         "device mosfet nfet 0 0 1 1 2 4 GND! a 0 0 a 0 0 a 0 0\n"
         "device mosfet nfet 0 0 1 1 2 4 GND! a 0 0 b 0 0 c 0 0\n"
         "device bjt npn 0 0 1 1 2 4 GND! a 0 0 b 0 0 c 0 0\n");
  auto options = nltest_options();
  const auto n = f.flat("top", ExtFixture::netlister_flags(), {}, options);
  REQUIRE(n.ok());
  REQUIRE(n.value().devices.size() == 2);
  // The duplicate keeps one device with the new terminals; the bjt has its
  // own tile type (17) and so its own key.
  for (const auto& d : n.value().devices) {
    CHECK(terminal_nodes(d) == Strings{"a", "b", "c"});
  }
  CHECK(f.texts("EXTFLAT-READ-012").empty());
}

TEST_CASE("an unknown device name never lands on another device",
          "[cap:ext-flattening-read][req:Device records]") {
  ExtFixture f;
  f.file("top",
         "device subckt foo 0 0 1 1 a 0 0 b 0 0\n"
         "device subckt foo 0 0 1 1 a 0 0 b 0 0 c 0 0\n");
  const auto n = f.flat("top", ExtFixture::netlister_flags(), {}, nltest_options());
  REQUIRE(n.ok());
  CHECK(n.value().devices.size() == 2);
  CHECK(f.texts("EXTFLAT-READ-012").empty());
}

TEST_CASE("an incompatible device at an occupied key is rejected",
          "[cap:ext-flattening-read][req:Device records]") {
  ExtFixture f;
  f.file("top",
         "device mosfet nfet 0 0 1 1 2 4 GND! a 0 0 b 0 0 c 0 0\n"
         "device mosfet nfet 0 0 1 1 2 4 GND! a 0 0 b 0 0\n");
  const auto n = f.flat("top", ExtFixture::netlister_flags(), {}, nltest_options());
  REQUIRE(n.ok());
  CHECK(n.value().devices.size() == 1);
  CHECK(f.texts("EXTFLAT-READ-012") ==
        Strings{"Device mosfet nfet at (0, 0) overlaps device with incompatible number of terminals (2 vs. 3)!"});
}

TEST_CASE("device coordinates and a/p/l/w parameters scale with locScale",
          "[cap:ext-flattening-read][req:Device records]") {
  ExtFixture f;
  f.file("top",
         "scale 1000 1000 10\n"
         "parameters nfet a=ad p=pd l=l w=w\n"
         "device subckt nfet 3 3 4 4 a=10 p=7 l=3 w=5 x 0 0\n");
  auto options = ExtFixture::netlister_options();
  options.units_per_lambda = 5.0F;  // locScale = 2
  const auto n = f.flat("top", ExtFixture::netlister_flags(), {}, options);
  REQUIRE(n.ok());
  REQUIRE(n.value().devices.size() == 1);
  const auto& d = n.value().devices[0];
  CHECK(d.local_area == hefesto::db::Rect{6, 6, 8, 8});
  CHECK(d.device_area == 40);
  CHECK(d.perimeter == 14);
  CHECK(d.length == 6);
  CHECK(d.width == 10);
  CHECK(d.params.empty());
}

TEST_CASE("a non-numeric unmodelled resistor value is an error and reads as 0",
          "[cap:ext-flattening-read][req:Device records]") {
  ExtFixture f;
  f.file("top", "device devres None 0 0 1 1 abc a 0 0 b 0 0\n");
  const auto n = f.flat("top");
  REQUIRE(n.ok());
  REQUIRE(n.value().devices.size() == 1);
  CHECK(n.value().devices[0].res == 0.0F);
  CHECK(f.texts("EXTFLAT-READ-002") == Strings{"/w/top.ext, line 1: Error: expected resistance value, got abc"});
}

// Regression (Q-NETLIST-51, approved 2026-11-22, fix in both modes): EFread.c
// sets no token count for a `vsource` record. The record parses with the
// default rule: its value, then terminal triples, no substrate.
TEST_CASE("a vsource record parses its value and terminals in both modes",
          "[cap:ext-flattening-read][req:Device records]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ExtFixture f;
    f.file("top", "device vsource None 0 0 1 1 0.0 dummy 0 0 a 0 0 b 0 0\n");
    auto options = ExtFixture::netlister_options();
    options.magic_compat = compat;
    const auto n = f.flat("top", ExtFixture::netlister_flags(), {}, options);
    REQUIRE(n.ok());
    REQUIRE(n.value().devices.size() == 1);
    const auto& d = n.value().devices[0];
    CHECK(d.device_class == "vsource");
    CHECK(d.substrate.empty());
    CHECK(terminal_nodes(d) == Strings{"dummy", "a", "b"});
    CHECK(f.sink.snapshot().empty());
  }
}

TEST_CASE("NL-FLAT-05: parameters are declared newest first, scale and offset parsed",
          "[cap:ext-flattening-read][req:Parameters records]") {
  GoldenCase g{"NL-FLAT-05"};
  const auto n = g.flat("top", ExtFixture::netlister_flags(), {}, nltest_options());
  REQUIRE(n.ok());
  const auto it = n.value().device_params.find("nfet");
  REQUIRE(it != n.value().device_params.end());
  using P = extflat::DeviceParam;
  // `parameters nfet l=l w=w a1=as p1=ps x=xpos*0.5 l1=lt+2`, reversed
  // (Q-NETLIST-9).
  CHECK(it->second == std::vector<P>{P{"l1", "lt", 1.0, 2}, P{"x", "xpos", 0.5, 0}, P{"p1", "ps", 1.0, 0},
                                     P{"a1", "as", 1.0, 0}, P{"w", "w", 1.0, 0}, P{"l", "l", 1.0, 0}});
  // The first device puts `l1=4` where a mosfet's substrate goes, so its
  // parameters start at `foo=1` and the terminals do not divide by 3: Magic
  // drops it with the message of the captured stderr.txt.
  REQUIRE(n.value().devices.size() == 1);
  const auto* d = device_at(n.value(), 100);
  REQUIRE(d != nullptr);
  CHECK(d->params.empty());
  CHECK(d->length == 2);
  CHECK(d->width == 8);
  const auto errors = g.texts("EXTFLAT-READ-002");
  REQUIRE(errors.size() == 1);
  CHECK(errors[0].ends_with("/top.ext, line 11: Incomplete terminal description for device"));
}

TEST_CASE("an undeclared parameter and an l<n> parameter are kept verbatim, newest first",
          "[cap:ext-flattening-read][req:Parameters records]") {
  ExtFixture f;
  f.file("top",
         "parameters nfet l=l w=w l1=lt+2\n"
         "device subckt nfet 0 0 1 1 l1=4 foo=1 l=2 w=3 a1=7 g 0 0 s 0 0\n");
  const auto n = f.flat("top");
  REQUIRE(n.ok());
  REQUIRE(n.value().devices.size() == 1);
  const auto& d = n.value().devices[0];
  // l1 matches the declared `l1` but is kept verbatim (l<n>, n != 0); foo
  // and a1 are not declared.
  CHECK(d.params == Strings{"a1=7", "foo=1", "l1=4"});
  CHECK(d.length == 2);
  CHECK(d.width == 3);
}

// Q-NETLIST-8 (compat) carries a tier-1 guard in both modes: Magic leaves
// parm_offset uninitialised with a `*` scale; it is 0 here.
TEST_CASE("a `*` scale parameter has offset 0 in both modes (Q-NETLIST-8)",
          "[cap:ext-flattening-read][req:Parameters records]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ExtFixture f;
    f.file("top", "parameters rpoly l=len*0.5 w=wid*1 a=area+3 p=per-2 bad\n");
    auto options = ExtFixture::netlister_options();
    options.magic_compat = compat;
    const auto n = f.flat("top", ExtFixture::netlister_flags(), {}, options);
    REQUIRE(n.ok());
    using P = extflat::DeviceParam;
    CHECK(n.value().device_params.at("rpoly") ==
          std::vector<P>{P{"p", "per", 1.0, -2}, P{"a", "area", 1.0, 3}, P{"w", "wid", 1.0, 0}, P{"l", "len", 0.5, 0}});
    CHECK(f.texts("EXTFLAT-READ-002") ==
          Strings{"/w/top.ext, line 1: Bad parameter assignment \"bad\" for device \"rpoly\""});
  }
}

TEST_CASE("the first parameters declaration of a type wins; instance parameters keep their token",
          "[cap:ext-flattening-read][req:Parameters records]") {
  ExtFixture f;
  f.file("top",
         "parameters nfet l=l\n"
         "parameters nfet w=w\n"
         "parameters :cell bad x=1 y=2*3\n");
  const auto n = f.flat("top");
  REQUIRE(n.ok());
  using P = extflat::DeviceParam;
  CHECK(n.value().device_params.at("nfet") == std::vector<P>{P{"l", "l", 1.0, 0}});
  CHECK(n.value().device_params.at(":cell") == std::vector<P>{P{"02", "y=2", 3.0, 0}, P{"01", "x=1", 1.0, 0}});
}

TEST_CASE("NL-FLAT-09: a resist value carries Magic's +0.5 with magic-compat ON (Q-NETLIST-12)",
          "[cap:ext-flattening-read][req:Use, port, cap, resist, attr and connect records]") {
  GoldenCase g{"NL-FLAT-09"};
  const auto n = g.flat("top", ExtFixture::netlister_flags(), {}, nltest_options());
  REQUIRE(n.ok());
  REQUIRE(n.value().resists.size() == 1);
  CHECK(n.value().resists[0] == extflat::FlatResist{"a", "b", 1234.5});
}

TEST_CASE("a resist value has no offset with magic-compat OFF (Q-NETLIST-12)",
          "[cap:ext-flattening-read][req:Use, port, cap, resist, attr and connect records]") {
  ExtFixture f;
  f.file("top", "scale 1 1 1\nnode a 0 0 0 0 m1\nnode b 0 0 1 0 m1\nresist a b 1234\n");
  auto options = ExtFixture::netlister_options();
  options.magic_compat = false;
  const auto n = f.flat("top", ExtFixture::netlister_flags(), {}, options);
  REQUIRE(n.ok());
  REQUIRE(n.value().resists.size() == 1);
  CHECK(n.value().resists[0] == extflat::FlatResist{"a", "b", 1234.0});
}

TEST_CASE("node, merge, subcap and cap values scale by cscale; area/perimeter accumulate",
          "[cap:ext-flattening-read][req:Node and substrate records]") {
  ExtFixture f;
  f.file("top",
         "scale 1000 10 5\n"
         "resistclasses 100\n"
         "node a 0 7 0 0 m1 4 8\n"
         "node a 0 3 10 0 m1 1 2\n"
         "node b 0 0 5 5 m1 0 0\n"
         "subcap b 2\n"
         "attr a 0 0 1 1 m1 \"ext:x=1\"\n");
  const auto n = f.flat("top");
  REQUIRE(n.ok());
  const auto* a = node_named(n.value(), "a");
  REQUIRE(a != nullptr);
  CHECK(a->raw_cap == 100.0F);
  CHECK(a->area_perim == std::vector<std::pair<std::int32_t, std::int32_t>>{{5, 10}});
  CHECK(a->attributes == Strings{"ext:x=1"});
  const auto* b = node_named(n.value(), "b");
  REQUIRE(b != nullptr);
  CHECK(b->raw_cap == 20.0F);
}

TEST_CASE("the .res.ext overlay is read into the same definition and filters deferred equivs",
          "[cap:ext-flattening-read][req:Resistance-extraction overlay]") {
  ExtFixture f;
  f.file("top",
         "scale 1000 1000 5\n"
         "port P1 0 0 0 1 1 m1\n"
         "port P2 1 5 0 6 1 m1\n"
         "port Q1 2 9 0 10 1 m1\n"
         "port Q2 3 12 0 13 1 m1\n"
         "equiv P1 P2\n"
         "equiv Q1 Q2\n");
  (void)f.fs.write_atomic("/w/top.res.ext", "rnode \"x.n0\" 0 0 3 0 m1\nresist \"P1\" \"x.n0\" 12.3\n");
  auto options = ExtFixture::netlister_options();
  options.resist_overlay = true;
  options.subcircuits = true;
  extflat::FlattenOptions fo;
  fo.subcircuits = extflat::FlattenOptions::Subcircuits::root_cleared;
  const auto n = f.flat("top", ExtFixture::netlister_flags(), fo, options);
  REQUIRE(n.ok());
  // P1 and P2 stay apart (P1 is on the overlay's network); Q1/Q2 merge
  // after the overlay, as a plain equiv with its warning.
  CHECK(node_named(n.value(), "P1") != nullptr);
  CHECK(node_named(n.value(), "P2") != nullptr);
  CHECK((node_named(n.value(), "Q1") == nullptr) != (node_named(n.value(), "Q2") == nullptr));
  REQUIRE(n.value().resists.size() == 1);
  CHECK(n.value().resists[0].a == "P1");
  CHECK(n.value().resists[0].b == "x.n0");
  // The overlay has no scale record: it keeps the .ext file's rscale 1000.
  CHECK_THAT(n.value().resists[0].resistance, WithinAbs(12300.5, 1e-2));
  CHECK(f.texts("EXTFLAT-READ-006") == Strings{"Warning:  Ports \"Q1\" and \"Q2\" are electrically shorted."});
}

TEST_CASE("without overlays the shorted ports merge with the warning",
          "[cap:ext-flattening-read][req:Resistance-extraction overlay]") {
  ExtFixture f;
  f.file("top", "port P1 0 0 0 1 1 m1\nport P2 1 5 0 6 1 m1\nequiv P1 P2\n");
  auto options = ExtFixture::netlister_options();
  options.subcircuits = true;
  extflat::FlattenOptions fo;
  fo.subcircuits = extflat::FlattenOptions::Subcircuits::root_cleared;
  const auto n = f.flat("top", ExtFixture::netlister_flags(), fo, options);
  REQUIRE(n.ok());
  CHECK(n.value().nodes().size() == 1);
  CHECK(f.texts("EXTFLAT-READ-006") == Strings{"Warning:  Ports \"P1\" and \"P2\" are electrically shorted."});
}

TEST_CASE("read_res_overlay returns the overlay's records as tokens",
          "[cap:ext-flattening-read][req:Resistance-extraction overlay]") {
  ExtFixture f;
  (void)f.fs.write_atomic("/w/top.res.ext", "# comment\nrnode \"a b\" 0 0 1 2 m1\nresist a b 5\n");
  const auto records = extflat::read_res_overlay(f.ctx, "top", {});
  REQUIRE(records.ok());
  REQUIRE(records.value().size() == 2);
  CHECK(records.value()[0].tokens == Strings{"rnode", "a b", "0", "0", "1", "2", "m1"});
  CHECK(records.value()[1].line == 3);
  const auto missing = extflat::read_res_overlay(f.ctx, "none", {});
  REQUIRE_FALSE(missing.ok());
  CHECK(missing.error().code == "EXTFLAT-READ-001");
}

TEST_CASE("short resistor builds a zero-ohm None resistor between shorted ports",
          "[cap:ext-flattening-read][req:Equivalence, merge and kill records]") {
  ExtFixture f;
  f.file("top", "port A 0 0 0 1 1 m1\nport B 1 5 0 6 1 m1\nequiv A B\n");
  auto options = ExtFixture::netlister_options();
  options.subcircuits = true;
  extflat::FlattenOptions fo;
  fo.subcircuits = extflat::FlattenOptions::Subcircuits::root_cleared;
  options.shorts = extflat::ReadOptions::PortShorts::resistor;
  const auto n = f.flat("top", ExtFixture::netlister_flags(), fo, options);
  REQUIRE(n.ok());
  REQUIRE(n.value().devices.size() == 1);
  CHECK(n.value().devices[0].device_class == "devres");
  CHECK(n.value().devices[0].model == "None");
  CHECK(terminal_nodes(n.value().devices[0]) == Strings{"dummy", "A", "B"});
  CHECK(node_named(n.value(), "A") != nullptr);
  CHECK(node_named(n.value(), "B") != nullptr);
}

// Q-NETLIST-31 (compat) and Q-NETLIST-60: terminal 0 of every short device
// in a cell is the shared node `dummy` with magic-compat ON; with it OFF
// each device has a node of its own, `dummy#<n>` numbered per cell, that no
// record's name reaches (a user net `dummy` stays apart).
TEST_CASE("short devices share dummy with magic-compat ON and get dummy#<n> with it OFF",
          "[cap:ext-flattening-read][req:Equivalence, merge and kill records]") {
  for (const bool compat : {true, false}) {
    INFO("magic-compat " << compat);
    ExtFixture f;
    f.file("top", "port A 0 0 0 1 1 m1\nport B 1 5 0 6 1 m1\nport C 2 10 0 11 1 m1\nport D 3 15 0 16 1 m1\n"
                  "equiv A B\nequiv C D\nnode dummy 0 0 20 0 m1\n");
    auto options = ExtFixture::netlister_options();
    options.subcircuits = true;
    options.magic_compat = compat;
    options.shorts = extflat::ReadOptions::PortShorts::resistor;
    extflat::FlattenOptions fo;
    fo.subcircuits = extflat::FlattenOptions::Subcircuits::root_cleared;
    const auto n = f.flat("top", ExtFixture::netlister_flags(), fo, options);
    REQUIRE(n.ok());
    REQUIRE(n.value().devices.size() == 2);
    // The devices come in def_devs hash order.
    std::vector<Strings> terms{terminal_nodes(n.value().devices[0]), terminal_nodes(n.value().devices[1])};
    std::sort(terms.begin(), terms.end(), [](const Strings& a, const Strings& b) { return a[1] < b[1]; });
    if (compat) {
      CHECK(terms == std::vector<Strings>{{"dummy", "A", "B"}, {"dummy", "C", "D"}});
    } else {
      CHECK(terms == std::vector<Strings>{{"dummy#0", "A", "B"}, {"dummy#1", "C", "D"}});
      CHECK(node_named(n.value(), "dummy") != nullptr);
    }
  }
}
