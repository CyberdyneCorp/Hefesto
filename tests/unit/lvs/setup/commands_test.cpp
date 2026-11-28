// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-setup-file: `equate classes` (Q-LVS-29), `ignore` (Q-LVS-22),
// `model`, `global`/`convert`, `flatten` and the query and output commands
// (Q-LVS-16). Netlists and setup texts are Hefesto-authored.
//
// Owner: LVSSETUP.

#include <catch2/catch_test_macros.hpp>

#include "setup_fixture.hpp"

namespace {

using setup_test::logged;
using setup_test::logged_error;
using setup_test::Pair;
namespace lvs = hefesto::lvs;

constexpr std::string_view kLayout =
    ".subckt XY_inv A Y VDD VSS\nM1 Y A VSS VSS nfet\nM2 Y A VDD VDD pfet\n.ends\n"
    ".subckt top a y vdd vss\nX1 a y vdd vss XY_inv\n.ends\n";
constexpr std::string_view kSchematic =
    ".subckt inv A Y VDD VSS\nM1 Y A VSS VSS nfet\nM2 Y A VDD VDD pfet\n.ends\n"
    ".subckt XY_inv A Y VDD VSS\nM1 Y A VSS VSS nfet\n.ends\n"
    ".subckt top a y vdd vss\nX1 a y vdd vss inv\n.ends\n";

}  // namespace

TEST_CASE("equate classes pairs a prefixed layout cell", "[cap:lvs-setup-file][req:equate classes]") {
  Pair pair(kLayout, kSchematic);
  const auto state = pair.ok("puts [equate classes \"-circuit1 XY_inv\" \"-circuit2 inv\"]\n");
  CHECK(logged(state, "Device classes XY_inv and inv are equivalent."));
  CHECK(logged(state, "1"));
  REQUIRE(state.equated_classes.size() == 1);
  CHECK(state.equated_classes.front() == std::pair<std::string, std::string>{"XY_inv", "inv"});
  CHECK(state.equated.front().identity.empty());
  CHECK_FALSE(state.equated.front().wildcard);
}

TEST_CASE("equate classes errors", "[cap:lvs-setup-file][req:equate classes]") {
  Pair pair(kLayout, kSchematic);
  const auto state = pair.ok(
      "puts [equate classes {nosuch 0} {inv 1}]\n"
      "equate classes {XY_inv 0} {nfet 0}\n"
      "equate classes {nfet 0}\n");
  // As netgen 1.5.327 does (capture of NG-SET-09, run 46050777576): the message goes to stdout and the
  // command fails with the result 0.
  CHECK(logged(state, "Cell to equate does not exist."));
  CHECK(logged_error(state, "Error setup.tcl:1 (ignoring), 0"));
  CHECK(logged(state, "Cells to equate are in the same netlist."));
  CHECK(logged_error(state, "Error setup.tcl:2 (ignoring), 0"));
  CHECK(state.errors == 3);
}

TEST_CASE("equate classes with a wildcard is recorded for the queue", "[cap:lvs-setup-file][req:equate classes]") {
  Pair pair(kLayout, kSchematic);
  const auto state = pair.ok("equate classes {* XY_inv} {inv 1}\n");
  REQUIRE(state.equated.size() == 1);
  CHECK(state.equated.front().wildcard);
}

namespace {

// Q-LVS-29 (fix in both modes): -unique draws the identity from the seeded
// generator, so the same seed gives the same identity on every run.
void unique_identity_is_deterministic(bool compat) {
  std::string first;
  for (int run = 0; run < 2; ++run) {
    Pair pair(kLayout, kSchematic);
    auto ctx = pair.context(compat);
    ctx.seed = 7;
    const auto state = lvs::evaluate_setup(
        lvs::Source{"u.tcl", std::string{"equate -unique classes {XY_inv 0} {inv 1}\n"}}, ctx);
    REQUIRE(state.ok());
    REQUIRE(state.value().equated.size() == 1);
    const std::string identity = state.value().equated.front().identity;
    REQUIRE(identity.size() == std::string{"XY_inv"}.size() + 1);
    CHECK(identity.starts_with("XY_inv"));
    CHECK(identity.back() >= 'A');
    CHECK(identity.back() <= 'Z');
    if (run == 0) {
      first = identity;
    } else {
      CHECK(identity == first);
    }
  }
  // No other name exists in the other netlist: no fresh identity is needed.
  Pair plain(kLayout, kSchematic);
  const auto state = plain.ok("equate -unique classes {top 0} {top 1}\n", compat);
  CHECK(state.equated.front().identity.empty());
}

}  // namespace

TEST_CASE("equate -unique uses the seeded generator (Q-LVS-29, netgen-compat ON)", "[cap:lvs-setup-file][req:equate classes]") {
  unique_identity_is_deterministic(true);
}

TEST_CASE("equate -unique uses the seeded generator (Q-LVS-29, netgen-compat OFF)", "[cap:lvs-setup-file][req:equate classes]") {
  unique_identity_is_deterministic(false);
}

TEST_CASE("equate classes with pin lists renames and renumbers pins", "[cap:lvs-setup-file][req:equate classes]") {
  constexpr std::string_view kA = ".subckt blk p q\n.ends\n.subckt top a b\nX1 a b blk\nR1 a b 1k\n.ends\n";
  constexpr std::string_view kB = ".subckt blk2 x y\n.ends\n.subckt top a b\nX1 a b blk2\nR1 a b 1k\n.ends\n";
  Pair pair(kA, kB);
  auto state = pair.ok("equate classes {blk 0} {p q} {blk2 1} {x y}\n");
  CHECK(logged(state, "Device classes blk and blk2 are equivalent."));
  CHECK(pair.cell(2, "blk2").ports.at(0).name == "p");
  CHECK(pair.cell(2, "blk2").ports.at(1).name == "q");
  Pair swapped(kA, kB);
  state = swapped.ok("equate classes {blk 0} {{p 1} {q 0}} {blk2 1} {{x 0} {y 1}}\n");
  CHECK(swapped.cell(2, "blk2").ports.at(0).name == "q");
  CHECK(swapped.cell(2, "blk2").ports.at(1).name == "p");
  const auto& instance = swapped.cell(2, "top").instances.front();
  CHECK(swapped.cell(2, "top").net(instance.connections.at(0)).name == "b");
  state = swapped.ok(
      "equate classes {blk 0} {p} {blk2 1} {x y}\n"
      "equate classes {blk 0} {p q r} {blk2 1} {x y r}\n"
      "equate classes {blk 0} {{p 1 2} q} {blk2 1} {x y}\n"
      "equate classes {blk 0} {{p 1} q} {blk2 1} {x y}\n"
      "equate classes {r 0} {a b} {r 1} {a b}\n");
  CHECK(logged_error(state, "Error setup.tcl:1 (ignoring), Pin lists are different length, cannot match pins!"));
  CHECK(logged_error(state, "Error setup.tcl:2 (ignoring), List length does not match  number of pins in cell."));
  CHECK(logged_error(state, "Error setup.tcl:3 (ignoring), All list items must have one or two entries."));
  CHECK(logged_error(state, "Error setup.tcl:4 (ignoring), All list items must have the same number of entries."));
  CHECK(logged_error(state, "Error setup.tcl:5 (ignoring), Device class is not black box or subcircuit!"));
}

TEST_CASE("ignore class removes fill instances in one circuit", "[cap:lvs-setup-file][req:ignore command]") {
  constexpr std::string_view kFill =
      ".subckt top a b\nX1 a b fill_2\nX2 a b fill_2\nR1 a b 1k\n.ends\n.subckt fill_2 p q\n.ends\n";
  Pair pair(kFill);
  const auto state = pair.ok("ignore class \"-circuit1 fill_2\"\n");
  CHECK(pair.cell(1, "top").instances.size() == 1);
  CHECK(pair.cell(2, "top").instances.size() == 3);
  REQUIRE(state.ignored.size() == 1);
  CHECK(state.ignored.front().first == 1);
  CHECK(state.ignored.front().second.name == "fill_2");
}

TEST_CASE("ignore shorted removes devices tied to one net", "[cap:lvs-setup-file][req:ignore command]") {
  Pair pair(".subckt top a b\nC1 a a 1p\nC2 a b 1p\n.ends\n");
  pair.ok("ignore shorted c\n");
  CHECK(pair.cell(1, "top").instances.size() == 1);
  CHECK(pair.cell(2, "top").instances.size() == 1);
}

namespace {

// Q-LVS-22 (fix in both modes): an unknown keyword is a bad option; the
// one-argument form keeps its wrong-argument error.
void ignore_bad_keyword(bool compat) {
  Pair pair;
  const auto state = pair.ok("ignore bogus nfet\nignore nfet\n", compat);
  CHECK(logged_error(state, "Error setup.tcl:1 (ignoring), bad option \"bogus\": must be class or shorted"));
  CHECK(logged_error(state,
                     "Error setup.tcl:2 (ignoring), wrong # args: should be \"ignore class|shorted <valid_cellname>\""));
  CHECK(pair.cell(1, "inv").instances.size() == 2);
}

}  // namespace

TEST_CASE("ignore with an unknown keyword is a bad option (Q-LVS-22, netgen-compat ON)", "[cap:lvs-setup-file][req:ignore command]") {
  ignore_bad_keyword(true);
}

TEST_CASE("ignore with an unknown keyword is a bad option (Q-LVS-22, netgen-compat OFF)", "[cap:lvs-setup-file][req:ignore command]") {
  ignore_bad_keyword(false);
}

TEST_CASE("model sets and queries classes with port checks", "[cap:lvs-setup-file][req:model command]") {
  Pair pair(".subckt top a b c\nX1 a b rpoly\nX2 a b c rpoly3\nX3 a b c q3\n.ends\n");
  const auto state = pair.ok(
      "model \"-circuit1 rpoly\" resistor\n"
      "puts [model \"-circuit1 rpoly\"]\n"
      "model {rpoly3 0} res\n"
      "model {q3 0} diode\n"
      "model {q3 0} npn\n"
      "model {q3 1} moscap\n"
      "puts [model blackbox]\n"
      "model blackbox on\n"
      "puts [model {rpoly 1}]\n"
      "model {rpoly 1} copy\n");
  CHECK(pair.cell(1, "rpoly").device_class == lvs::DeviceClass::resistor);
  CHECK(logged(state, "resistor"));
  CHECK(pair.cell(1, "rpoly3").device_class == lvs::DeviceClass::res3);
  CHECK(logged_error(state, "Error setup.tcl:4 (ignoring), Wrong number of ports for device"));
  CHECK(pair.cell(1, "q3").device_class == lvs::DeviceClass::npn);
  CHECK(pair.cell(2, "q3").device_class == lvs::DeviceClass::ecap);
  CHECK(logged(state, "0"));
  CHECK(state.auto_blackbox);
  CHECK(logged(state, "blackbox"));  // a placeholder module reported with automatic black-boxing
  CHECK(pair.cell(2, "rpoly").device_class == lvs::DeviceClass::resistor);
}

TEST_CASE("A resistor-modelled placeholder takes resistor permutation", "[cap:lvs-setup-file][req:model command]") {
  Pair pair(".subckt top a b\nX1 a b rpoly\n.ends\n");
  pair.ok("model rpoly resistor\npermute {rpoly 0} 1 2\n");
  CHECK(pair.cell(1, "rpoly").device_class == lvs::DeviceClass::resistor);
  CHECK(pair.cell(1, "rpoly").permutes.size() == 1);
}

TEST_CASE("global changes matching nets in the hierarchy", "[cap:lvs-setup-file][req:global and convert commands]") {
  Pair pair(".subckt inv A Y\nM1 Y A VDD1 VDD1 pfet\nM2 Y A VSS VSS nfet\n.ends\n"
            ".subckt top a y\nX1 a y inv\nR1 a VDD2 1k\n.ends\n");
  const auto state = pair.ok(
      "puts [global {top 0} VDD*]\n"
      "puts \"top1 [global {top 1} {V?D[2-9]} nomatch]\"\n"
      "puts \"inv1 [global {inv 1} {\\VS?}]\"\n");
  CHECK(logged(state, "2"));
  CHECK(logged(state, "top1 1"));  // VDD2 only
  CHECK(logged(state, "inv1 1"));  // VSS: `\\V` is a literal V
  CHECK(pair.cell(1, "inv").net(pair.cell(1, "inv").find_net("VDD1")).global);
  CHECK_FALSE(pair.cell(2, "inv").net(pair.cell(2, "inv").find_net("VDD1")).global);
}

TEST_CASE("convert turns a global supply into a port", "[cap:lvs-setup-file][req:global and convert commands]") {
  Pair pair(".global VDD\n.subckt inv A Y\nM1 Y A VDD VDD pfet\n.ends\n.subckt top a y\nX1 a y inv\n.ends\n");
  pair.ok("global {inv 0} VDD\nconvert {inv 0}\n");
  lvs::Cell& inv = pair.cell(1, "inv");
  REQUIRE(inv.ports.size() == 3);
  CHECK(inv.ports.at(2).name == "VDD");
  lvs::Cell& top = pair.cell(1, "top");
  REQUIRE(top.instances.front().connections.size() == 3);
  CHECK(top.net(top.instances.front().connections.at(2)).name == "VDD");
  CHECK(top.net(top.instances.front().connections.at(2)).global);
}

TEST_CASE("flatten forms", "[cap:lvs-setup-file][req:flatten command]") {
  constexpr std::string_view kHier =
      ".subckt inv A Y\nM1 Y A n1 n1 nfet\n.ends\n"
      ".subckt mid a y\nX1 a y inv\n.ends\n"
      ".subckt top a y\nX1 a y mid\nX2 a y inv\n.ends\n";
  SECTION("contents") {
    Pair pair(kHier);
    const auto state = pair.ok("flatten {top 0}\n");
    CHECK(logged(state, "Flattening contents of cell top"));
    lvs::Cell& top = pair.cell(1, "top");
    REQUIRE(top.instances.size() == 2);
    CHECK(top.instances.at(0).name == "inv:2/nfet:1");
    CHECK(top.instances.at(1).name == "mid:1/inv:1/nfet:1");
    CHECK(top.find_net("inv:2/n1") != lvs::kNoNet);
    CHECK(pair.cell(2, "top").instances.size() == 2);
  }
  SECTION("class, class in parent, prohibit") {
    Pair pair(kHier);
    const auto state = pair.ok(
        "flatten class {inv 0}\n"
        "flatten class mid {inv 1}\n"
        "flatten class mid {nfet 1}\n"
        "flatten prohibit {mid 0}\n"
        "flatten {top 0}\n");
    CHECK(logged(state, "Flattening instances of inv in file layout.spice"));
    CHECK(logged(state, "Flattening instances of inv in cell mid within file schematic.spice"));
    CHECK(logged_error(state, "Error setup.tcl:3 (ignoring), No instances found to flatten."));
    CHECK(logged(state, "Will not flatten instances of mid in file layout.spice"));
    CHECK(pair.cell(1, "mid").instances.front().name == "inv:1/nfet:1");
    CHECK(pair.cell(1, "top").instances.front().name == "mid:1");  // prohibited mid stays
    CHECK(pair.cell(2, "mid").instances.front().name == "inv:1/nfet:1");
    REQUIRE(state.no_flatten.size() == 1);
    CHECK(state.no_flatten.front().cell == "mid");
  }
}

TEST_CASE("format sets and returns the column widths", "[cap:lvs-setup-file][req:Query and output commands available to setup files]") {
  Pair pair;
  const auto state = pair.ok("puts [format]\nformat 30\nputs [format]\nformat 50 30\nputs [format]\n");
  CHECK(logged(state, "41 41"));
  CHECK(logged(state, "30 30"));
  CHECK(state.column_left == 50);
  CHECK(state.column_right == 30);
}

namespace {

// Q-LVS-16 (fix in both modes): a width of zero or less is rejected and
// the widths stay as they were.
void zero_width_rejected(bool compat) {
  Pair pair;
  const auto state = pair.ok("format 30\nputs [format 0]\nputs [format 20 -1]\n", compat);
  CHECK(logged(state, "Column width cannot be zero or less"));
  CHECK(state.column_left == 30);
  CHECK(state.column_right == 30);
}

}  // namespace

TEST_CASE("format rejects a width of zero or less (Q-LVS-16, netgen-compat ON)",
          "[cap:lvs-setup-file][req:Query and output commands available to setup files]") {
  zero_width_rejected(true);
}

TEST_CASE("format rejects a width of zero or less (Q-LVS-16, netgen-compat OFF)",
          "[cap:lvs-setup-file][req:Query and output commands available to setup files]") {
  zero_width_rejected(false);
}

TEST_CASE("cells list -all -circuit1 lists cells and primitives", "[cap:lvs-setup-file][req:Query and output commands available to setup files]") {
  Pair pair;
  const auto state = pair.ok(
      "puts [cells list -all -circuit1]\n"
      "puts [cells list -circuit1]\n"
      "puts [llength [cells list -top]]\n"
      "ports -circuit1\n"
      "puts [canonical {nfet 1}]\n"
      "describe {inv 0}\n"
      "readnet x.spice\n");
  INFO(setup_test::log_text(state));
  // The SPICE reader's file cell (layout.spice) is a top cell too.
  CHECK(logged(state, "inv nfet pfet layout.spice"));
  CHECK(logged(state, "inv layout.spice"));
  CHECK(logged(state, "4"));
  CHECK(logged(state, "VSS"));
  CHECK(logged(state, "Cell inv contains 4 ports."));
  CHECK(logged(state, "nfet 1"));
  CHECK(logged(state, "Cell inv (subcircuit): 4 ports, 4 nets (0 global), 2 instances"));
  CHECK(state.errors == 1);
}
