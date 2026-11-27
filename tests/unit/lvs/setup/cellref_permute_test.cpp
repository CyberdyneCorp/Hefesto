// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-setup-file: the cell reference syntax and `permute` (Q-LVS-5).
// Netlists and setup texts are Hefesto-authored.
//
// Owner: LVSSETUP.

#include <catch2/catch_test_macros.hpp>

#include "setup_fixture.hpp"

namespace {

using setup_test::logged;
using setup_test::logged_error;
using setup_test::Pair;
using setup_test::property;
namespace lvs = hefesto::lvs;

bool has_l(lvs::Cell& cell) {
  return property(cell, "L") != nullptr;
}

}  // namespace

TEST_CASE("-circuit1 names circuit 1's cell only", "[cap:lvs-setup-file][req:Cell reference syntax]") {
  Pair pair;
  pair.ok("property \"-circuit1 nfet\" tolerance {W 0.02}\n");
  CHECK(property(pair.cell(1, "nfet"), "W")->tolerance == 0.02);
  CHECK(property(pair.cell(2, "nfet"), "W")->tolerance == 0.01);
}

TEST_CASE("Every cell reference form resolves", "[cap:lvs-setup-file][req:Cell reference syntax]") {
  SECTION("{cell fnum} and {fnum cell}") {
    Pair pair;
    pair.ok("property {nfet 1} delete L\nproperty {0 pfet} delete W\n");
    CHECK(has_l(pair.cell(1, "nfet")));
    CHECK_FALSE(has_l(pair.cell(2, "nfet")));
    CHECK(property(pair.cell(1, "pfet"), "W") == nullptr);
    CHECK(property(pair.cell(2, "pfet"), "W") != nullptr);
  }
  SECTION("{cell file} and {file cell}") {
    Pair pair;
    pair.ok("property {nfet schematic.spice} delete L\nproperty {layout.spice pfet} delete W\n");
    CHECK(has_l(pair.cell(1, "nfet")));
    CHECK_FALSE(has_l(pair.cell(2, "nfet")));
    CHECK(property(pair.cell(1, "pfet"), "W") == nullptr);
  }
  SECTION("{* cell} and a plain non-top name apply to both circuits") {
    Pair pair;
    pair.ok("property {* nfet} delete L\nproperty pfet delete W\n");
    CHECK_FALSE(has_l(pair.cell(1, "nfet")));
    CHECK_FALSE(has_l(pair.cell(2, "nfet")));
    CHECK(property(pair.cell(1, "pfet"), "W") == nullptr);
    CHECK(property(pair.cell(2, "pfet"), "W") == nullptr);
  }
  SECTION("keywords in second place, -current and file numbers") {
    Pair pair;
    const auto state = pair.ok(
        "property {nfet -circuit2} delete L\n"
        "puts [ports -current]\n"
        "ports 0\n");
    CHECK(has_l(pair.cell(1, "nfet")));
    CHECK_FALSE(has_l(pair.cell(2, "nfet")));
    CHECK(state.errors == 0);
    CHECK(logged(state, "VDD"));
  }
}

TEST_CASE("Bad cell references raise the netgen messages", "[cap:lvs-setup-file][req:Cell reference syntax]") {
  Pair pair;
  const auto state = pair.ok(
      "property nosuch\n"
      "property {nosuch 0}\n"
      "property {inv_missing layout.spice}\n"
      "property {7 nfet}\n"
      "property *\n"
      "property 9\n");
  CHECK(logged_error(state, "Error setup.tcl:1 (ignoring), No such cell!"));
  CHECK(logged_error(state, "Error setup.tcl:2 (ignoring), No such cellname!"));
  CHECK(logged_error(state, "Error setup.tcl:3 (ignoring), No such cellname!"));
  CHECK(logged_error(state, "Error setup.tcl:4 (ignoring), No such file number!"));
  CHECK(logged_error(state, "Error setup.tcl:5 (ignoring), Wildcards must be used with a valid cellname"));
  CHECK(logged_error(state, "Error setup.tcl:6 (ignoring), No such file number!"));
}

TEST_CASE("A cell present only in the other netlist is not in the file", "[cap:lvs-setup-file][req:Cell reference syntax]") {
  Pair pair(setup_test::kInverter,
            ".subckt inv A Y VDD VSS\nM1 Y A VSS VSS nfet W=1 L=0.15\nX1 A Y extra\n.ends\n.subckt extra a b\n.ends\n");
  const auto state = pair.ok("property {extra 0}\n");
  CHECK(logged_error(state, "Error setup.tcl:1 (ignoring), Cell is not in file!"));
}

TEST_CASE("Circuit keywords need assigned circuits", "[cap:lvs-setup-file][req:Cell reference syntax]") {
  Pair pair;
  pair.two.top.reset();
  const auto state = pair.ok("property -circuit2\n");
  CHECK(logged_error(state, "Error setup.tcl:1 (ignoring), No circuit has been declared for comparison"));
}

TEST_CASE("A circuit whose top is outside its loaded files is not declared", "[cap:lvs-setup-file][req:Cell reference syntax]") {
  Pair pair;
  pair.two.db.files.clear();  // a top cell set without a loaded file
  const auto state = pair.ok("property {-circuit2 nfet}\nproperty -circuit2\n");
  CHECK(logged_error(state, "Error setup.tcl:1 (ignoring), No circuit has been declared for comparison"));
  CHECK(state.errors == 1);
}

TEST_CASE("permute default declares source/drain and resistor ends", "[cap:lvs-setup-file][req:permute command]") {
  Pair pair(".subckt top a b g\nM1 a g b b nfet\nR1 a b 1k\nC1 a b 1p\nL1 a b 1n\n.ends\n");
  pair.ok("permute\n");
  REQUIRE(pair.cell(1, "nfet").permutes.size() == 1);
  CHECK(pair.cell(1, "nfet").permutes.front().first == "drain");
  CHECK(pair.cell(1, "r").permutes.size() == 1);
  CHECK(pair.cell(1, "l").permutes.size() == 1);
  CHECK(pair.cell(1, "c").permutes.empty());  // capacitors are not in the default
  pair.ok("permute capacitors\npermute default\n");
  CHECK(pair.cell(2, "c").permutes.size() == 1);
  CHECK(pair.cell(2, "nfet").permutes.size() == 1);  // no duplicate pair
}

TEST_CASE("permute pins on a placeholder", "[cap:lvs-setup-file][req:permute command]") {
  Pair pair(".subckt top a b c\nX1 a b c sky130_fd_pr__res_generic_po\n.ends\n");
  const auto state = pair.ok("permute \"-circuit1 sky130_fd_pr__res_generic_po\" 1 2\n");
  CHECK(logged(state, "Model sky130_fd_pr__res_generic_po pin 1 == 2"));
  CHECK(pair.cell(1, "sky130_fd_pr__res_generic_po").permutes.size() == 1);
  CHECK(pair.cell(2, "sky130_fd_pr__res_generic_po").permutes.empty());
}

TEST_CASE("permute reports a missing pin and a missing model", "[cap:lvs-setup-file][req:permute command]") {
  Pair pair;
  const auto state = pair.ok("permute pins {nfet 0} drain nosuch\npermute nosuch 1 2\n");
  CHECK(logged(state, "No such pin nosuch in model nfet"));
  CHECK(logged(state, "Unable to permute model nfet pins drain, nosuch."));
  CHECK(logged(state, "No such model nosuch"));
  CHECK(logged(state, "Unable to permute model nosuch pins 1, 2."));
  CHECK(pair.cell(1, "nfet").permutes.empty());
}

TEST_CASE("permute forget removes a pair in either order", "[cap:lvs-setup-file][req:permute command]") {
  Pair pair;
  const auto state = pair.ok("permute {nfet 0} drain source\npermute forget {nfet 0} source drain\n");
  CHECK(logged(state, "Model nfet pin source != drain"));
  CHECK(pair.cell(1, "nfet").permutes.empty());
}

namespace {

// Q-LVS-5 (fix in both modes): a cell-wide forget leaves an empty list, so
// a later declaration starts from nothing and nothing stale remains.
void forget_cell_empties_list(bool compat) {
  Pair pair;
  const auto state = pair.ok(
      "permute transistors\n"
      "permute forget {nfet 0}\n"
      "permute {nfet 0} gate bulk\n",
      compat);
  CHECK(logged(state, "No permutations on circuit nfet"));
  REQUIRE(pair.cell(1, "nfet").permutes.size() == 1);
  CHECK(pair.cell(1, "nfet").permutes.front().first == "gate");
  CHECK(pair.cell(2, "nfet").permutes.size() == 1);
  pair.ok("permute forget\n", compat);
  CHECK(pair.cell(1, "nfet").permutes.empty());
  CHECK(pair.cell(2, "pfet").permutes.empty());
}

}  // namespace

TEST_CASE("permute forget of a cell empties its pair list (Q-LVS-5, netgen-compat ON)",
          "[cap:lvs-setup-file][req:permute command]") {
  forget_cell_empties_list(true);
}

TEST_CASE("permute forget of a cell empties its pair list (Q-LVS-5, netgen-compat OFF)",
          "[cap:lvs-setup-file][req:permute command]") {
  forget_cell_empties_list(false);
}

TEST_CASE("permute keywords match unique prefixes", "[cap:lvs-setup-file][req:permute command]") {
  Pair pair(".subckt top a b\nR1 a b 1k\nM1 a b a a nfet\n.ends\n");
  pair.ok("permute res\n");
  CHECK(pair.cell(1, "r").permutes.size() == 1);
  CHECK(pair.cell(1, "nfet").permutes.empty());
}
