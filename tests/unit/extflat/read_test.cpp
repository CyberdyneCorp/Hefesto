// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// ext-flattening-read: the records of the `.ext` reader subset (EFread.c,
// EFbuild.c) seen through read_flat on hand-written files. The subset
// reads as `def write` does: dosubckt on, no resistance overlay, shorts
// `none`. Owner: EXTSUB.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "ext_fixture.hpp"

namespace extflat = hefesto::extflat;
using extflat::test::all_names;
using extflat::test::ExtFixture;
using extflat::test::names;
using extflat::test::node_named;
using Strings = std::vector<std::string>;

TEST_CASE("a quoted node name keeps its white space", "[cap:ext-flattening-read][req:Line tokenization]") {
  ExtFixture f;
  f.file("top", "node \"a b\" 0 0 1 2 m1\nequiv \"a b\" c\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  REQUIRE(n.value().nodes().size() == 1);
  CHECK(all_names(n.value().nodes()[0]) == Strings{"a b", "c"});
}

TEST_CASE("a backslash continues a line, comments and blank lines are skipped", "[cap:ext-flattening-read][req:Line tokenization]") {
  ExtFixture f;
  f.file("top", "# comment\n\n   \nnode n1 0 10 \\\n3 4 metal1\nbogus\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  REQUIRE(n.value().nodes().size() == 1);
  CHECK(n.value().nodes()[0].location == hefesto::db::Point{3, 4});
  CHECK(n.value().nodes()[0].layer == "metal1");
  // The diagnostic names the last physical line of the logical line.
  CHECK(f.texts("EXTFLAT-READ-002") == Strings{"/w/top.ext, line 6: Unrecognized token \"bogus\" (ignored)"});
}

TEST_CASE("a NUL byte ends a line and a continuation cut by the end of file is dropped",
          "[cap:ext-flattening-read][req:Line tokenization]") {
  ExtFixture f;
  f.file("top", std::string{"node a 0 0 1 1 m1"} + '\0' + " garbage\nnode b 0 0 2 2 m1 \\");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(names(n.value()) == Strings{"a"});
  CHECK(f.sink.snapshot().empty());
}

// Regression: efReadLine stores tokens in argv[128] without a bound.
TEST_CASE("a line of more than 128 tokens is ignored with EXTFLAT-READ-011", "[cap:ext-flattening-read][req:Line tokenization]") {
  ExtFixture f;
  std::string line = "node big 0 0 1 1 m1";
  for (int i = 0; i < 130; ++i) {
    line += " 0";
  }
  f.file("top", line + "\nnode small 0 0 0 0 m1\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(names(n.value()) == Strings{"small"});
  REQUIRE(f.texts("EXTFLAT-READ-011").size() == 1);
  CHECK(f.sink.snapshot()[0].native_only);
}

TEST_CASE("keywords match by unique prefix, case-insensitively", "[cap:ext-flattening-read][req:Keyword recognition]") {
  ExtFixture f;
  f.file("top", "NODE a 0 0 0 0 m1\nkill a\ns x y z\nnode b 0 0\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(names(n.value()).empty());  // `kill` is killnode; `node b` is too short
  CHECK(f.texts("EXTFLAT-READ-002") == Strings{"/w/top.ext, line 3: Unrecognized token \"s\" (ignored)",
                                               "/w/top.ext, line 4: Not enough tokens for node line"});
}

TEST_CASE("zero scale factors are reported and lscale rescales coordinates", "[cap:ext-flattening-read][req:Scale record]") {
  ExtFixture f;
  f.file("top", "scale 0 0 10\nnode a 0 0 3 -3 m1\nuse sub s0 1 0 0 0 1 0\n");
  f.file("sub", "node b 0 0 5 5 m1\n");
  extflat::ReadOptions options;
  options.units_per_lambda = 5.0F;
  const auto n = f.read("top", options);
  REQUIRE(n.ok());
  CHECK(f.texts("EXTFLAT-READ-002") == Strings{"/w/top.ext, line 1: Bad resistance scaling = 0; reset to 1.",
                                               "/w/top.ext, line 1: Bad capacitance scaling = 0; reset to 1."});
  // locScale = 10 / 5 = 2; (int)(0.5 + -6) = -5.
  CHECK(node_named(n.value(), "a")->location == hefesto::db::Point{6, -5});
  // locScale is a global of the read: the child, with no scale record, keeps it.
  CHECK(node_named(n.value(), "s0/b")->location == hefesto::db::Point{10, 10});
}

TEST_CASE("a repeated node record adds to the node instead of creating one", "[cap:ext-flattening-read][req:Node and substrate records]") {
  ExtFixture f;
  f.file("top", "resistclasses 10\nnode a 0 7 0 0 m1 4 8\nnode a 0 3 10 0 m1 1 2\nnode b 0 0 1 1 m2\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(names(n.value()) == Strings{"a", "b"});  // the cell's list (b, a), prepended again when flattened
  CHECK(node_named(n.value(), "a")->location == hefesto::db::Point{0, 0});
  CHECK(f.texts("EXTFLAT-READ-002") == Strings{"/w/top.ext, line 4: Too few area/perim values: assuming remainder are zero"});
}

TEST_CASE("a substrate record marks the global substrate", "[cap:ext-flattening-read][req:Node and substrate records]") {
  ExtFixture f;
  f.file("top", "substrate VSUB 0 0 -10 -10 space\nnode a 0 0 0 0 m1\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  const auto* sub = node_named(n.value(), "VSUB");
  REQUIRE(sub != nullptr);
  CHECK(sub->substrate);
  CHECK(sub->layer == "space");
  CHECK_FALSE(node_named(n.value(), "a")->substrate);
}

TEST_CASE("a name defined again after a kill of its node becomes a node of its own",
          "[cap:ext-flattening-read][req:Node and substrate records]") {
  ExtFixture f;
  f.file("top", "node a 0 0 0 0 m1\nequiv a b\nkillnode a\nnode b 0 0 5 5 m1\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  // `a` is killed; `b` left it and lives on.
  CHECK(names(n.value()) == Strings{"b"});
  CHECK(all_names(n.value().nodes()[0]) == Strings{"b"});
}

TEST_CASE("equiv adds aliases by preference and merges two nodes", "[cap:ext-flattening-read][req:Equivalence, merge and kill records]") {
  ExtFixture f;
  f.file("top",
         "node a# 0 0 0 0 m1\n"
         "equiv a# lbl\n"   // a# exists: lbl is added behind the head (never preferred)
         "equiv zz a#\n"    // zz is new as name1: added by preference (zz beats a#)
         "node q 0 0 1 1 m1\n"
         "equiv q a#\n"     // two nodes: merged, the better canonical name wins
         "equiv x y\n");    // neither exists: nothing happens
  const auto n = f.read("top");
  REQUIRE(n.ok());
  REQUIRE(n.value().nodes().size() == 1);
  // In the cell: q, zz, a#, lbl (q kept its name in the merge). Flattening
  // keeps the first name and inserts the others behind it, reversing them.
  CHECK(all_names(n.value().nodes()[0]) == Strings{"q", "lbl", "a#", "zz"});
}

TEST_CASE("equiv of two ports warns, unless one name is the other made unique",
          "[cap:ext-flattening-read][req:Equivalence, merge and kill records]") {
  ExtFixture f;
  f.file("top",
         "port A 0 0 0 1 1 m1\nport B 1 5 5 6 6 m1\nequiv A B\n"
         "port net 2 9 9 10 10 m1\nport net_uq1 3 12 12 13 13 m1\nequiv net net_uq1\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(f.texts("EXTFLAT-READ-006") == Strings{"Warning:  Ports \"A\" and \"B\" are electrically shorted."});
  CHECK(n.value().nodes().size() == 2);
}

TEST_CASE("merge records check their subscript ranges", "[cap:ext-flattening-read][req:Equivalence, merge and kill records]") {
  ExtFixture f;
  f.file("top",
         "merge a[1:0] b[0:1]\n"
         "merge a[0:1] b[0:2]\n"
         "merge a[0:1] b\n"
         "merge a[0:1][0:1][0:1] b[0:1][0:1][0:1]\n"
         "merge a[x:1] b[0:1]\n"
         "merge %s[0:1] %d[0:1]\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(f.texts("EXTFLAT-READ-002") ==
        Strings{"/w/top.ext, line 1: Backwards subscript range [1:0]",
                "/w/top.ext, line 2: Subscript 0 range mismatch",
                "/w/top.ext, line 3: Number of subscripts doesn't match",
                "/w/top.ext, line 4: Too many array subscripts (maximum=2)",
                "/w/top.ext, line 5: Subscript syntax error"});
  // Magic turns the name into a printf format; a '%' in it is plain text here.
  CHECK(f.texts("EXTFLAT-READ-007") == Strings{"connect(1): no such node %s[0]", "connect(1): no such node %s[1]"});
}

TEST_CASE("an array use and a repeated use id", "[cap:ext-flattening-read][req:Use, port, cap, resist, attr and connect records]") {
  ExtFixture f;
  f.file("top", "use inv u1[0:1:10][0:0:0] 1 0 0 0 1 0\nuse inv v 1 0 0 0 1 0\nuse inv v 1 0 7 0 1 0\n");
  f.file("inv", "node a 0 0 1 1 m1\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(f.texts("EXTFLAT-READ-005") == Strings{"Warning: use v appears more than once in def!"});
  const auto* u0 = node_named(n.value(), "u1[0]/a");
  const auto* u1 = node_named(n.value(), "u1[1]/a");
  REQUIRE(u0 != nullptr);
  REQUIRE(u1 != nullptr);
  CHECK(u1->location == hefesto::db::Point{11, 1});
  CHECK(node_named(n.value(), "v/a")->location == hefesto::db::Point{8, 1});  // the later use replaced the first
}

TEST_CASE("cap and resist names are checked, attr needs its node",
          "[cap:ext-flattening-read][req:Use, port, cap, resist, attr and connect records]") {
  ExtFixture f;
  f.file("top", "node a 0 0 0 0 m1\ncap a[0:1] b 5\nresist a b[2:1] 5\nattr zz 0 0 1 1 m1 \"t\"\nattr a 0 0 1 1 m1 t\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(f.texts("EXTFLAT-READ-002") == Strings{"/w/top.ext, line 2: Number of subscripts doesn't match",
                                               "/w/top.ext, line 3: Backwards subscript range [2:1]",
                                               "/w/top.ext, line 4: Attribute for nonexistent node zz ignored"});
}

TEST_CASE("a tech record naming another technology fails the read",
          "[cap:ext-flattening-read][req:Technology, style, version and resistance classes]") {
  ExtFixture f;
  f.file("top", "tech other\nnode a 0 0 0 0 m1\n");
  extflat::ReadOptions options;
  options.tech_name = "mine";
  const auto n = f.read("top", options);
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().code == "EXTFLAT-READ-003");
  CHECK(f.texts("EXTFLAT-READ-003") == Strings{"Error: .ext file has different technology other",
                                               "Load this technology and repeat."});
  options.tech_name = "other";
  CHECK(f.read("top", options).ok());
}

TEST_CASE("style, version and resistance class records", "[cap:ext-flattening-read][req:Technology, style, version and resistance classes]") {
  ExtFixture f;
  f.file("top", "style nosuch\nversion 7.5\nresistclasses 100 200\nuse c c0 1 0 0 0 1 0\nuse d d0 1 0 0 0 1 0\n");
  f.file("c", "style known\nresistclasses 100 300\n");
  f.file("d", "resistclasses 100\n");
  extflat::ReadOptions options;
  options.style_units = [](std::string_view s) -> std::optional<float> {
    return s == "known" ? std::optional<float>{1.0F} : std::nullopt;
  };
  const auto n = f.read("top", options);
  REQUIRE(n.ok());
  CHECK(f.texts("EXTFLAT-READ-004") == Strings{"Warning:  .ext file style nosuch is not known in this technology!"});
  CHECK(n.value().selected_style == std::optional<std::string>{"known"});
  const Strings expected{
      "/w/top.ext, line 2: Cell was extracted using version 7.5 of the extractor.",
      "/w/top.ext, line 2:    It should be re-extracted.",
      "/w/c.ext, line 2: Resistance class values don't match:",
      "/w/c.ext, line 2: Re-extract the entire tree with the same technology file",
      "/w/d.ext, line 1: Number of resistance classes doesn't match:",
      "/w/d.ext, line 1: Re-extract the entire tree with the same technology file",
  };
  // c and d are read in use-table order; compare as a set of lines per file.
  auto got = f.texts("EXTFLAT-READ-002");
  std::sort(got.begin(), got.end());
  auto want = expected;
  std::sort(want.begin(), want.end());
  CHECK(got == want);
}

TEST_CASE("a missing root fails with EXTFLAT-READ-001", "[cap:ext-flattening-read][req:Locating and recursively reading .ext files]") {
  ExtFixture f;
  const auto n = f.read("nothere");
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().code == "EXTFLAT-READ-001");
  CHECK(n.error().text == "Cannot read extract file nothere.ext");
}

TEST_CASE("a missing subcell fails the read after reading the rest",
          "[cap:ext-flattening-read][req:Locating and recursively reading .ext files]") {
  ExtFixture f;
  f.file("top", "use gone g0 1 0 0 0 1 0\nuse here h0 1 0 0 0 1 0\n");
  f.file("here", "version 1\n");
  const auto n = f.read("top");
  REQUIRE_FALSE(n.ok());
  CHECK(n.error().text == "Cannot read extract file gone.ext");
  CHECK(f.texts("EXTFLAT-READ-002").size() == 2);  // here.ext was still read
}

TEST_CASE("a shared subcell is read once; -p and the loaded cell's directory are searched",
          "[cap:ext-flattening-read][req:Locating and recursively reading .ext files]") {
  ExtFixture f;
  (void)f.fs.write_atomic("/p/top.ext", "timestamp 1\nuse inv u1 1 0 0 0 1 0\nuse inv u2 1 0 0 0 1 0\n");
  (void)f.fs.write_atomic("/cells/inv.ext", "timestamp 2\nnode a 0 0 0 0 m1\n");
  (void)f.fs.write_atomic("/w/inv.ext", "timestamp 3\n");
  extflat::ReadOptions options;
  options.timestamps = true;
  options.search_path = {"/p"};
  options.cell_directory = [](std::string_view cell) -> std::optional<std::string> {
    return cell == "inv" ? std::optional<std::string>{"/cells"} : std::nullopt;
  };
  const auto n = f.read("top", options);
  REQUIRE(n.ok());
  REQUIRE(n.value().stamps().size() == 2);
  CHECK(n.value().stamps()[1].timestamp == 2);  // /cells, not /w: -p was given
  CHECK(n.value().nodes().size() == 2);
}

TEST_CASE("device records create their terminal and substrate nodes", "[cap:ext-flattening-read][req:Device records]") {
  ExtFixture f;
  f.file("top",
         "node a 0 0 0 0 m1\n"
         "device mosfet nfet 10 20 11 21 2 8 GND! a 4 0 b 8 0 c 8 0\n"
         "device bogus nfet 0 0 1 1 a 0 0 b 0 0\n"
         "device mosfet nfet 0 0 1 1 2 8 GND! a 4\n"
         "device mosfet nfet 10 20 11 21 2 8 GND! a 4 0 b 8 0\n"
         "node x 0 0 0 0 m1\n"
         "node y 0 0 0 0 m1\n"
         "device subckt Short 0 0 1 1 None x 0 0 y 0 0\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  CHECK(f.texts("EXTFLAT-READ-002") == Strings{"/w/top.ext, line 3: Unknown device class",
                                               "/w/top.ext, line 4: Incomplete terminal description for device"});
  CHECK(f.texts("EXTFLAT-READ-012") ==
        Strings{"Device mosfet nfet at (10, 20) overlaps device with incompatible number of terminals (2 vs. 3)!"});
  // Creation order: a, GND! (substrate), b, c, x, y; the Short joined x
  // and y into x's node, under the preferred name y.
  CHECK(names(n.value()) == Strings{"a", "GND!", "b", "c", "y"});
}

TEST_CASE("a Short device takes the second and fifth tokens after its parameters", "[cap:ext-flattening-read][req:Device records]") {
  ExtFixture f;
  f.file("top", "node p 0 0 0 0 m1\nnode q 0 0 1 1 m1\ndevice subckt Short 0 0 1 1 k=v None p 0 0 q 0 0\n"
                "device subckt Short 0 0 1 1 a=1 b=2 c=3 p\n");
  const auto n = f.read("top");
  REQUIRE(n.ok());
  REQUIRE(n.value().nodes().size() == 1);
  CHECK(f.texts("EXTFLAT-READ-002") == Strings{"/w/top.ext, line 4: Bad terminal description for Short device"});
}

TEST_CASE("parameters records report tokens without '='", "[cap:ext-flattening-read][req:Parameters records]") {
  ExtFixture f;
  f.file("top", "parameters nfet l=l w w=w\nparameters nfet bad\n");
  REQUIRE(f.read("top").ok());
  CHECK(f.texts("EXTFLAT-READ-002") ==
        Strings{"/w/top.ext, line 1: Bad parameter assignment \"w\" for device \"nfet\""});
}
