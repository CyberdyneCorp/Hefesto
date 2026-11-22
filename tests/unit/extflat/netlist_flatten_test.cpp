// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ext-flattening-hierarchy, M7 round 2: extflat::flatten (EFFlatBuild with
// the efFlatNodes path) and flatten_one_level over the committed NL-FLAT
// inputs (tests/golden/NETLIST/cases/NL-FLAT-*/input/), checked against the
// netlists and messages Magic 8.3.694 wrote for them (expected/), and over
// hand-written files. Owner: EXTFLAT.

#include <catch2/catch_test_macros.hpp>

#include <climits>

#include <set>
#include <string>
#include <vector>

#include "ext_fixture.hpp"
#include "flatten.hpp"

namespace extflat = hefesto::extflat;
using extflat::test::ExtFixture;
using extflat::test::GoldenCase;
using extflat::test::names;
using extflat::test::nltest_options;
using extflat::test::node_named;
using extflat::test::terminal_nodes;
using Strings = std::vector<std::string>;

namespace {

extflat::FlattenOptions cthresh0() {
  extflat::FlattenOptions o;
  o.cap_threshold = 0.0;
  return o;
}

}  // namespace

TEST_CASE("NL-FLAT-11: children's devices are visited in use-table order with their paths",
          "[cap:ext-flattening-hierarchy][req:Visit orders for output]") {
  GoldenCase g{"NL-FLAT-11"};
  const auto n = g.flat("top", ExtFixture::netlister_flags(), {}, nltest_options());
  REQUIRE(n.ok());
  // top.flat.spice: M1000 u1, M1001 u2, M1002 u3, M1003 ua, M1004 b10.
  Strings prefixes;
  for (const auto& d : n.value().devices) {
    prefixes.push_back(d.hier_prefix);
  }
  CHECK(prefixes == Strings{"u1", "u2", "u3", "ua", "b10"});
  CHECK(terminal_nodes(n.value().devices[0]) == Strings{"u1/in", "u1/out", "u1/out"});
  // The implicit substrate is flat as `GND!` only: `u1/GND!` does not
  // resolve, so the netlister matches the local name with defSubs.
  CHECK(n.value().devices[0].substrate == "u1/GND!");
  CHECK(n.value().devices[0].substrate_local == "GND!");
  CHECK(n.value().devices[0].substrate_index == extflat::kNoNode);
  // (0 0 2 4) times locScale 5, then u1's translation (20, 0), unscaled.
  CHECK(n.value().devices[0].area == hefesto::db::Rect{20, 0, 30, 20});
  CHECK(g.sink.snapshot().empty());
}

TEST_CASE("NL-FLAT-10: 40 and 120 devices are each visited once",
          "[cap:ext-flattening-hierarchy][req:Visit orders for output]") {
  GoldenCase g{"NL-FLAT-10"};
  for (const auto& [cell, count] : {std::pair{"top40", 40U}, std::pair{"top120", 120U}}) {
    INFO(cell);
    const auto n = g.flat(cell, ExtFixture::netlister_flags(), {}, nltest_options());
    REQUIRE(n.ok());
    std::set<std::int32_t> xs;
    for (const auto& d : n.value().devices) {
      xs.insert(d.local_area.x0);
    }
    CHECK(n.value().devices.size() == count);
    CHECK(xs.size() == count);
  }
}

TEST_CASE("NL-FLAT-12: unconnected vdd! nodes merge only when vdd! is a global name",
          "[cap:ext-flattening-hierarchy][req:Global name merging]") {
  GoldenCase g{"NL-FLAT-12"};
  SECTION("globals unset (top.globalon.spice): x1/vdd! and x2/vdd! stay apart") {
    const auto n = g.flat("top", ExtFixture::netlister_flags(), {}, nltest_options());
    REQUIRE(n.ok());
    CHECK(terminal_nodes(n.value().devices[0]) == Strings{"x1/o", "x1/vdd!", "x1/o"});
    CHECK(g.texts("EXTFLAT-READ-008").empty());
  }
  SECTION("globals(vdd!) (top.globalsarray.spice): one vdd! node and the stdout.txt text") {
    auto options = nltest_options();
    options.global_nets = {"vdd!"};
    const auto n = g.flat("top", ExtFixture::netlister_flags(), {}, options);
    REQUIRE(n.ok());
    CHECK(terminal_nodes(n.value().devices[0]) == Strings{"x1/o", "vdd!", "x1/o"});
    CHECK(terminal_nodes(n.value().devices[1]) == Strings{"x2/o", "vdd!", "x2/o"});
    CHECK(g.texts("EXTFLAT-READ-008") ==
          Strings{"*** Global name vdd! not fully connected:", "One portion contains the names:", "    x2/vdd!",
                  "The other portion contains the names:", "    x1/vdd!",
                  "I'm merging the two pieces into a single node, but you",
                  "should be sure eventually to connect them in the layout.", ""});
  }
  SECTION("EF_NONAMEMERGE (global off): no merging") {
    auto options = nltest_options();
    options.global_nets = {"vdd!"};
    const auto n =
        g.flat("top", ExtFixture::netlister_flags() | extflat::FlatFlags::no_name_merge, {}, options);
    REQUIRE(n.ok());
    CHECK(terminal_nodes(n.value().devices[0]) == Strings{"x1/o", "x1/vdd!", "x1/o"});
    CHECK(g.texts("EXTFLAT-READ-008").empty());
  }
}

TEST_CASE("NL-FLAT-13: a killed node drops its devices, couplings and node visit",
          "[cap:ext-flattening-hierarchy][req:Kills]") {
  GoldenCase g{"NL-FLAT-13"};
  const auto n = g.flat("top", ExtFixture::netlister_flags(), cthresh0(), nltest_options());
  REQUIRE(n.ok());
  // top.spice: only the device on a, no coupling, `C0 a 0 1f`.
  REQUIRE(n.value().devices.size() == 1);
  CHECK(terminal_nodes(n.value().devices[0]) == Strings{"a", "a", "a"});
  CHECK(n.value().caps.empty());
  CHECK(node_named(n.value(), "n1") == nullptr);
  CHECK(node_named(n.value(), "n2") == nullptr);
  REQUIRE(node_named(n.value(), "a") != nullptr);
  CHECK(node_named(n.value(), "a")->cap == 1000.0F);
}

TEST_CASE("NL-FLAT-14: a coupling to the substrate folds into ground capacitance",
          "[cap:ext-flattening-hierarchy][req:Coupling capacitance flattening]") {
  GoldenCase g{"NL-FLAT-14"};
  const auto n = g.flat("top", ExtFixture::netlister_flags(), cthresh0(), nltest_options());
  REQUIRE(n.ok());
  // top.spice: C0 b a 4f, C1 a VSUB 4f, C2 b VSUB 1f.
  REQUIRE(n.value().caps.size() == 1);
  CHECK(n.value().caps[0] == extflat::FlatCap{"a", "b", 4000.0});
  CHECK(node_named(n.value(), "a")->cap == 4000.0F);
  CHECK(node_named(n.value(), "b")->cap == 1000.0F);
  const auto* vsub = node_named(n.value(), "VSUB");
  REQUIRE(vsub != nullptr);
  CHECK(vsub->substrate);
  CHECK(vsub->cap == 0.0F);  // the global substrate passes 0 (a substrate record was read)
  CHECK_FALSE(n.value().substrate_compat);
}

TEST_CASE("couplings are written by terminal names, the smaller first (Q-NETLIST-1)",
          "[cap:ext-flattening-hierarchy][req:Coupling capacitance flattening]") {
  ExtFixture f;
  f.file("top",
         "node z 0 0 0 0 m1\nnode b 0 0 1 0 m1\nnode a 0 0 2 0 m1\n"
         "cap z b 2\ncap a z 3\ncap b z 5\ncap a a 9\ncap q a 7000\n");
  const auto n = f.flat("top", ExtFixture::netlister_flags(), cthresh0());
  REQUIRE(n.ok());
  CHECK(n.value().caps == std::vector<extflat::FlatCap>{{"a", "z", 3.0}, {"b", "z", 7.0}});
  // An unresolved name is reported at or above the threshold.
  CHECK(f.texts("EXTFLAT-READ-007") == Strings{"cap(1): no such node q"});
}

TEST_CASE("an unresolved coupling name below the threshold is silent",
          "[cap:ext-flattening-hierarchy][req:Coupling capacitance flattening]") {
  ExtFixture f;
  f.file("top", "node a 0 0 0 0 m1\ncap q a 1999\n");
  const auto n = f.flat("top");  // threshold 2 fF
  REQUIRE(n.ok());
  CHECK(f.texts("EXTFLAT-READ-007").empty());
}

TEST_CASE("NL-FLAT-15: the lumped resistance of a node sums its classes",
          "[cap:ext-flattening-hierarchy][req:Lumped node resistance]") {
  GoldenCase g{"NL-FLAT-15"};
  const auto n = g.flat("top", ExtFixture::netlister_flags(), {}, nltest_options());
  REQUIRE(n.ok());
  // top.sim: `R w 25` (25100 mohm: 5 x 5000 + 1 x 100).
  REQUIRE(node_named(n.value(), "w") != nullptr);
  CHECK(node_named(n.value(), "w")->resistance == 25100);
}

TEST_CASE("a square wire is one square of its class", "[cap:ext-flattening-hierarchy][req:Lumped node resistance]") {
  using PA = hefesto::extflat::detail::PerimArea;
  CHECK(hefesto::extflat::detail::node_resistance({PA{100, 40}}, {100}) == 100);
  CHECK(hefesto::extflat::detail::node_resistance({PA{0, 40}, PA{100, 40}}, {100, 7}) == 7);
  CHECK(hefesto::extflat::detail::node_resistance({PA{1, 4}}, {INT32_MAX}) == INT32_MAX);
}

TEST_CASE("NL-FLAT-16: legacy fet length and width, with ext:l= and ext:w= gate attributes",
          "[cap:ext-flattening-hierarchy][req:Device length and width]") {
  GoldenCase g{"NL-FLAT-16"};
  extflat::FlattenOptions fo;
  fo.symbols = {{"LL", 3}};
  const auto n = g.flat("top", ExtFixture::netlister_flags(), fo, nltest_options());
  REQUIRE(n.ok());
  // top.sim: `n g s 2 4`, `n g3 s d 3 6`, `n g2 s d 2 6`.
  std::vector<std::pair<std::int32_t, std::int32_t>> lw;
  for (const auto& d : n.value().devices) {
    lw.emplace_back(d.length, d.width);
  }
  CHECK(lw == std::vector<std::pair<std::int32_t, std::int32_t>>{{2, 4}, {3, 6}, {2, 6}});
  CHECK(n.value().devices[1].terminals[0].attributes == Strings{"ext:l=LL", "ext:w=6"});
}

// Regression: EFGetLengthAndWidth divides by a zero length (SIGFPE) for a
// 2-terminal fet whose perimeter and area give L = 0; fixed in both modes.
TEST_CASE("a 2-terminal fet with a zero length has width 0 instead of dividing by zero",
          "[cap:ext-flattening-hierarchy][req:Device length and width]") {
  for (const bool compat : {true, false}) {
    ExtFixture f;
    f.file("top", "fet nfet 0 0 1 1 5 3 GND! g 0 0 s 0 0\n");
    auto options = ExtFixture::netlister_options();
    options.magic_compat = compat;
    const auto n = f.flat("top", ExtFixture::netlister_flags(), {}, options);
    REQUIRE(n.ok());
    REQUIRE(n.value().devices.size() == 1);
    CHECK(n.value().devices[0].length == 0);
    CHECK(n.value().devices[0].width == 0);
  }
}

TEST_CASE("NL-FLAT-17: merged nodes keep the preferred canonical name",
          "[cap:ext-flattening-names][req:Name preference order]") {
  GoldenCase g{"NL-FLAT-17"};
  const auto n = g.flat("top", ExtFixture::netlister_flags(), {}, nltest_options());
  REQUIRE(n.ok());
  // top.spice: a, bb, VSS, b2 (Q-NETLIST-10: b2 beats a2 on a full tie).
  Strings gates;
  for (const auto& d : n.value().devices) {
    gates.push_back(d.terminals[0].node);
  }
  std::sort(gates.begin(), gates.end());
  CHECK(gates == Strings{"VSS", "a", "b2", "bb"});
}

TEST_CASE("resistors are visited children first, each cell's newest first",
          "[cap:ext-flattening-hierarchy][req:Visit orders for output]") {
  ExtFixture f;
  f.file("leaf", "node p 0 0 0 0 m1\nnode q 0 0 1 0 m1\nresist p q 10\n");
  f.file("top",
         "use leaf u1 1 0 0 0 1 0\n"
         "node a 0 0 0 0 m1\nnode b 0 0 1 0 m1\nnode c 0 0 2 0 m1\n"
         "resist a b 100\nresist b c 200\nresist a a 5\n");
  auto options = ExtFixture::netlister_options();
  options.magic_compat = false;
  const auto n = f.flat("top", ExtFixture::netlister_flags(), {}, options);
  REQUIRE(n.ok());
  CHECK(n.value().resists == std::vector<extflat::FlatResist>{{"u1/p", "u1/q", 10.0}, {"b", "c", 200.0}, {"a", "b", 100.0}});
}

TEST_CASE("the efFlatNodes path warns about connection names that do not resolve",
          "[cap:ext-flattening-hierarchy][req:Flat node construction]") {
  ExtFixture f;
  f.file("leaf", "node p 0 3 0 0 m1\n");
  f.file("top", "use leaf u1 1 0 0 0 1 0\nnode a 0 0 0 0 m1\nmerge u1/p a 2\nmerge u1/nope a\n");
  SECTION("flat mode") {
    const auto n = f.flat("top");
    REQUIRE(n.ok());
    CHECK(f.texts("EXTFLAT-READ-007") == Strings{"connect(1): no such node u1/nope"});
    // The merge's capacitance goes to its first node.
    REQUIRE(n.value().nodes().size() == 1);
    CHECK(n.value().nodes()[0].cap == 5.0F);  // aF
  }
  SECTION("one-level flattening (FLATNODE_HIER) is silent") {
    const auto h = f.read_hier("top");
    REQUIRE(h.ok());
    const auto n = extflat::flatten_one_level(h.value(), "top", ExtFixture::netlister_flags(), {}, f.sink);
    REQUIRE(n.ok());
    // Regression: the device-less pass visits the root's uses, never the
    // root, so its connections are neither warned about nor added twice.
    CHECK(f.texts("EXTFLAT-READ-007").empty());
    REQUIRE(n.value().nodes().size() == 1);
    CHECK(n.value().nodes()[0].cap == 2.0F);  // the merge's 2 aF once; u1's own 3 aF went with u1
  }
}

TEST_CASE("the efFlatNodes path descends into subcircuits; EF_NOFLATSUBCKT gives their ports only",
          "[cap:ext-flattening-hierarchy][req:Flat node construction]") {
  ExtFixture f;
  f.file("leaf", "port p 0 0 0 1 1 m1\nnode p 0 0 0 0 m1\nnode inner 0 0 1 1 m1\n");
  f.file("top", "use leaf u1 1 0 0 0 1 0\n");
  auto options = ExtFixture::netlister_options();
  options.subcircuits = true;
  const auto full = f.flat("top", ExtFixture::netlister_flags(), {}, options);
  REQUIRE(full.ok());
  CHECK(names(full.value()) == Strings{"u1/p", "u1/inner"});
  const auto std_cell =
      f.flat("top", ExtFixture::netlister_flags() | extflat::FlatFlags::no_flat_subckt, {}, options);
  REQUIRE(std_cell.ok());
  CHECK(names(std_cell.value()) == Strings{"u1/p"});
}

TEST_CASE("EF_WARNABSTRACT reports each instance of an abstract cell",
          "[cap:ext-flattening-hierarchy][req:Flat node construction]") {
  ExtFixture f;
  f.file("leaf", "abstract\nnode p 0 0 0 0 m1\n");
  f.file("top", "use leaf u1 1 0 0 0 1 0\nuse leaf u2 1 0 9 0 1 0\n");
  SECTION("with the flag (antennacheck)") {
    const auto n = f.flat("top", ExtFixture::netlister_flags() | extflat::FlatFlags::warn_abstract);
    REQUIRE(n.ok());
    CHECK(f.texts("EXTFLAT-READ-013") == Strings{"Error:  Cell leaf was extracted as an abstract view.",
                                                 "Error:  Cell leaf was extracted as an abstract view."});
  }
  SECTION("without it") {
    const auto n = f.flat("top");
    REQUIRE(n.ok());
    CHECK(f.texts("EXTFLAT-READ-013").empty());
  }
}

TEST_CASE("one-level flattening does not count child capacitance twice (FLATNODE_HIER)",
          "[cap:ext-flattening-hierarchy][req:One-level flattening for hierarchical output]") {
  ExtFixture f;
  f.file("leaf",
         "resistclasses 100\nport p 0 0 0 1 1 m1\nnode p 0 7 0 0 m1 10 20\n"
         "device mosfet nfet 0 0 1 1 2 4 GND! p 0 0 p 0 0 p 0 0\n");
  f.file("top", "resistclasses 100\nuse leaf u1 1 0 0 0 1 0\nnode a 0 3 0 0 m1 1 2\nmerge u1/p a 1\n");
  auto options = ExtFixture::netlister_options();
  options.subcircuits = true;
  const auto h = f.read_hier("top", options);
  REQUIRE(h.ok());
  const auto full = extflat::flatten(h.value(), ExtFixture::netlister_flags(), {}, f.sink);
  REQUIRE(full.ok());
  REQUIRE(node_named(full.value(), "a") != nullptr);
  CHECK(node_named(full.value(), "a")->cap == 11.0F);  // 7 + 3 + 1 aF
  bool nodevices = true;
  const auto level = extflat::flatten_one_level(h.value(), "top", ExtFixture::netlister_flags(), {}, f.sink, &nodevices);
  REQUIRE(level.ok());
  const auto* a = node_named(level.value(), "a");
  REQUIRE(a != nullptr);
  CHECK(a->cap == 4.0F);  // the child's 7 aF were written with the child
  CHECK(a->area_perim == std::vector<std::pair<std::int32_t, std::int32_t>>{{1, 2}});
  CHECK(level.value().devices.empty());  // the definition's own devices only
  CHECK_FALSE(nodevices);                // leaf has a device, so it is not absorbed
}

TEST_CASE("one-level flattening absorbs a device-less child",
          "[cap:ext-flattening-hierarchy][req:One-level flattening for hierarchical output]") {
  ExtFixture f;
  f.file("wire", "node w 0 0 0 0 m1\nnode w2 0 0 1 0 m1\nequiv w w2\ncap w x 4000\nnode x 0 0 2 0 m1\n");
  f.file("top", "use wire u1 1 0 0 0 1 0\nnode a 0 0 0 0 m1\nmerge u1/w a\n");
  const auto h = f.read_hier("top");
  REQUIRE(h.ok());
  bool nodevices = false;
  const auto level =
      extflat::flatten_one_level(h.value(), "top", ExtFixture::netlister_flags(), cthresh0(), f.sink, &nodevices);
  REQUIRE(level.ok());
  CHECK(nodevices);  // no devices, and its only use was absorbed
  CHECK(level.value().caps == std::vector<extflat::FlatCap>{{"a", "u1/x", 4000.0}});
}

TEST_CASE("one-level flattening reports a device on a killed node and keeps it",
          "[cap:ext-flattening-hierarchy][req:Kills]") {
  ExtFixture f;
  f.file("top",
         "node a 0 0 0 0 m1\nnode n1 0 0 1 0 m1\n"
         "device mosfet nfet 4 5 6 7 2 4 GND! a 0 0 n1 0 0 a 0 0\nkillnode n1\n");
  const auto h = f.read_hier("top");
  REQUIRE(h.ok());
  const auto level = extflat::flatten_one_level(h.value(), "top", ExtFixture::netlister_flags(), {}, f.sink);
  REQUIRE(level.ok());
  REQUIRE(level.value().devices.size() == 1);
  CHECK(level.value().devices[0].on_killed_node);
  CHECK(f.texts("EXTFLAT-READ-014") ==
        Strings{"Error:  Device at (4 5) is connected to one or more eliminated nodes."});
}

TEST_CASE("devices and resistors of subcircuits are not visited; subcircuit calls are",
          "[cap:ext-flattening-hierarchy][req:Visit orders for output]") {
  ExtFixture f;
  f.file("cell", "port p 0 0 0 1 1 m1\nnode p 0 0 0 0 m1\ndevice mosfet nfet 0 0 1 1 2 4 GND! p 0 0 p 0 0 p 0 0\n");
  f.file("mid", "use cell c1 1 0 0 0 1 0\n");
  f.file("top", "use mid m1 1 0 0 0 1 0\n");
  auto options = ExtFixture::netlister_options();
  options.subcircuits = true;
  const auto n = f.flat("top", ExtFixture::netlister_flags(), {}, options);
  REQUIRE(n.ok());
  CHECK(n.value().devices.empty());
  REQUIRE(n.value().subcircuits.size() == 1);
  CHECK(n.value().subcircuits[0] == extflat::FlatSubcircuit{"cell", "m1/c1", {"m1", "c1"}, false});
  extflat::FlattenOptions none;
  none.subcircuits = extflat::FlattenOptions::Subcircuits::none;
  const auto all = f.flat("top", ExtFixture::netlister_flags(), none, options);
  REQUIRE(all.ok());
  CHECK(all.value().devices.size() == 1);
  CHECK(all.value().subcircuits.empty());
}

TEST_CASE("the hierarchy lists definitions children first and uses in use-table order",
          "[cap:ext-flattening-hierarchy][req:One-level flattening for hierarchical output]") {
  ExtFixture f;
  f.file("leaf", "node p 0 0 0 0 m1\n");
  f.file("mid", "use leaf l1 1 0 0 0 1 0\n");
  f.file("top", "use mid u3 1 0 0 0 1 0\nuse leaf u1 1 0 0 0 1 0\nuse mid u2 1 0 5 0 1 0\n");
  const auto h = f.read_hier("top");
  REQUIRE(h.ok());
  Strings defs;
  h.value().visit_defs([&](const extflat::HierDef& d) { defs.push_back(d.name); });
  CHECK(defs == Strings{"leaf", "mid", "top"});
  Strings uses;
  h.value().visit_uses("top", [&](const extflat::HierUse& u) { uses.push_back(u.id); });
  CHECK(uses == Strings{"u1", "u2", "u3"});  // buckets 4, 5, 3+... as the use table orders them
}

TEST_CASE("the scale record sets the output scale and the scale-changed flag",
          "[cap:ext-flattening-read][req:Scale record]") {
  ExtFixture f;
  f.file("leaf", "scale 1000 1000 10\nnode p 0 0 0 0 m1\ndevice subckt x 0 0 1 1 p 0 0 p 0 0\n");
  f.file("top", "scale 1000 1000 5\nuse leaf u1 1 0 0 0 1 0\n");
  const auto n = f.flat("top");
  REQUIRE(n.ok());
  CHECK(n.value().scale == 1.0F);
  CHECK(n.value().scale_changed);
  REQUIRE(n.value().devices.size() == 1);
  CHECK(n.value().devices[0].scale == 10.0F);
}

TEST_CASE("read_flat is unchanged: EF_NOFLATSUBCKT, nodes only", "[cap:ext-flattening-hierarchy][req:Flat node construction]") {
  ExtFixture f;
  f.file("leaf", "port p 0 0 0 1 1 m1\nnode p 0 0 0 0 m1\nnode inner 0 0 1 1 m1\n");
  f.file("top", "use leaf u1 1 0 0 0 1 0\ndevice mosfet nfet 0 0 1 1 2 4 GND! u1/p 0 0 x 0 0 x 0 0\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(n.value().devices.empty());
  // efNodeList order: the device line made GND!, u1/p, x (each prepended).
  CHECK(names(n.value()) == Strings{"GND!", "x", "u1/p"});
}
