// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-hierarchy: the compare queue, cell-pair preparation, pre-match
// flattening, flattening semantics, duplicate and shorted pins, the per-pair
// flow, pin matching, black boxes, the final result and the depth limit.
// Every netlist is Hefesto-authored from the spec.
//
// Owner: LVSHIER.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>

#include "hefesto/lvs/hier/codes.hpp"
#include "hefesto/lvs/report/format.hpp"
#include "hier_fixture.hpp"

namespace {

using lvs_hier_test::cell;
using lvs_hier_test::cell_id;
using lvs_hier_test::circuit;
using lvs_hier_test::console_of;
using lvs_hier_test::contains;
using lvs_hier_test::kInv;
using lvs_hier_test::log_of;
using lvs_hier_test::run;
namespace lvs = hefesto::lvs;

using Queue = std::vector<std::pair<std::string, std::string>>;

std::string three_levels() {
  return std::string{kInv} + R"(.subckt mid A Y VDD VSS
Xinv1 A n VDD VSS inv
Xinv2 n Y VDD VSS inv
.ends
.subckt top A Y VDD VSS
Xmid1 A Y VDD VSS mid
.ends
)";
}

// A NAND gate; `bottom` is the class of the lower series device.
std::string nand2(std::string_view bottom) {
  return R"(.subckt nand2 A B Y VDD VSS
M1 Y A VDD VDD pfet
M2 Y B VDD VDD pfet
M3 Y A n VSS nfet
M4 n B VSS VSS )" +
         std::string{bottom} + R"(
.ends
.subckt top A B Y VDD VSS
Xnand2_1 A B Y VDD VSS nand2
.ends
)";
}

}  // namespace

// ------------------------------------------------------------ queue

TEST_CASE("Bottom-up order", "[cap:lvs-hierarchy][req:Compare queue construction]") {
  const auto result = run(three_levels(), three_levels());
  CHECK(result.queue == Queue{{"inv", "inv"}, {"mid", "mid"}, {"top", "top"}});
  CHECK(result.pairs.size() == 3);
  CHECK(result.clean);
}

TEST_CASE("Unpaired cell", "[cap:lvs-hierarchy][req:Compare queue construction]") {
  const std::string layout = std::string{kInv} + R"(.subckt filler_x VDD VSS
C1 VDD VSS 1f
.ends
.subckt top A Y VDD VSS
Xinv1 A Y VDD VSS inv
Xfiller_x1 VDD VSS filler_x
.ends
)";
  const std::string schematic = std::string{kInv} + R"(.subckt top A Y VDD VSS
Xinv1 A Y VDD VSS inv
.ends
)";
  const auto result = run(layout, schematic);
  CHECK(result.queue == Queue{{"inv", "inv"}, {"top", "top"}});
  CHECK(contains(log_of(result), "Flattening unmatched subcell filler_x in circuit top (0)(1 instance)\n"));
}

TEST_CASE("Equated classes pair differently named cells", "[cap:lvs-hierarchy][req:Compare queue construction]") {
  const std::string layout = std::string{kInv} + ".subckt top A Y VDD VSS\nXinv1 A Y VDD VSS inv\n.ends\n";
  std::string schematic = layout;
  schematic.replace(schematic.find(".subckt inv"), 11, ".subckt not");
  schematic.replace(schematic.find("VSS inv\n.ends"), 7, "VSS not");
  lvs::RunOptions options;
  options.equated_classes = {{"inv", "not"}};
  const auto result = run(layout, schematic, options);
  CHECK(result.queue == Queue{{"inv", "not"}, {"top", "top"}});
  CHECK(result.clean);
}

// ------------------------------------------------------------ preparation

TEST_CASE("Flatten unmatched child", "[cap:lvs-hierarchy][req:Preparing a cell pair]") {
  const std::string layout = std::string{kInv} + R"(.subckt filler_x VDD VSS
C1 VDD VSS 1f
.ends
.subckt top A Y VDD VSS
Xinv1 A Y VDD VSS inv
Xfiller_x1 VDD VSS filler_x
Xfiller_x2 VDD VSS filler_x
Xfiller_x3 VDD VSS filler_x
.ends
)";
  const std::string schematic = std::string{kInv} + R"(.subckt top A Y VDD VSS
Xinv1 A Y VDD VSS inv
C1 VDD VSS 3f
.ends
)";
  const auto result = run(layout, schematic);
  CHECK(contains(log_of(result), "Flattening unmatched subcell filler_x in circuit top (0)(3 instances)\n"));
}

TEST_CASE("Black box against contents is not flattened", "[cap:lvs-hierarchy][req:Preparing a cell pair]") {
  const std::string empty = ".subckt adc IN OUT VDD VSS\n.ends\n";
  const std::string full = ".subckt adc IN OUT VDD VSS\nM1 OUT IN VSS VSS nfet\n.ends\n";
  const std::string top = ".subckt top IN OUT VDD VSS\nXadc1 IN OUT VDD VSS adc\nM9 OUT IN VDD VDD pfet\n.ends\n";
  const auto result = run(circuit(full + top, "one.spice", 0), circuit(empty + top, "two.spice", 1, true));
  CHECK(contains(log_of(result), "Circuit 2 cell adc is a black box; will not flatten Circuit 1\n"));
}

// ------------------------------------------------------------ pre-match

TEST_CASE("Extra hierarchy level in layout", "[cap:lvs-hierarchy][req:Pre-match flattening]") {
  const std::string pair = R"(.subckt pair A Y VDD VSS
Xinv1 A n VDD VSS inv
Xinv2 n Y VDD VSS inv
.ends
)";
  const std::string layout = std::string{kInv} + pair + R"(.subckt top A Y VDD VSS
Xpair1 A m VDD VSS pair
Xpair2 m Y VDD VSS pair
.ends
)";
  const std::string schematic = std::string{kInv} + pair + R"(.subckt top A Y VDD VSS
Xinv1 A n1 VDD VSS inv
Xinv2 n1 n2 VDD VSS inv
Xinv3 n2 n3 VDD VSS inv
Xinv4 n3 Y VDD VSS inv
.ends
)";
  const auto result = run(layout, schematic);
  const auto log = log_of(result);
  CHECK(contains(log, "Flattening instances of pair in cell top (0) makes a better match\n"));
  CHECK(contains(log, "Making another compare attempt.\n"));
  CHECK(contains(console_of(result), "Flattened mismatched instances and attempting compare again.\n"));
  REQUIRE(result.reports.size() == 3);
  const auto& top = result.reports.back();
  CHECK(top.devices1.front().name == "inv");
  CHECK(top.devices1.front().count == 4);
  CHECK(top.devices2.front().count == 4);
  CHECK(result.clean);
}

// ------------------------------------------------------------ flattening

TEST_CASE("Flattened net names", "[cap:lvs-hierarchy][req:Flattening semantics]") {
  auto c = circuit(std::string{kInv} + R"(.subckt buf A Y VDD VSS
Xinv_a A n1 VDD VSS inv
Xinv_b n1 Y VDD VSS inv
.ends
.subckt top IN OUT VDD VSS
Xbuf_1 IN OUT VDD VSS buf
.ends
)",
                   "one.spice");
  const auto top = cell_id(c, "top");
  CHECK(lvs::flatten_instances(c.db, top, cell_id(c, "buf")) == 1);
  const auto& flat = c.db.cells[top];
  REQUIRE(flat.instances.size() == 2);
  CHECK(flat.instances[0].name == "/buf_1//inv_a");
  CHECK(flat.instances[0].cell == cell_id(c, "inv"));
  const lvs::NetId n1 = flat.find_net("/buf_1/n1");
  REQUIRE(n1 != lvs::kNoNet);
  // Port nets connect to the parent's nets on the instance's pins.
  CHECK(flat.net(flat.instances[0].connections[0]).name == "IN");
  CHECK(flat.instances[0].connections[1] == n1);
  CHECK(flat.instances[1].connections[0] == n1);
  CHECK(flat.net(flat.instances[1].connections[1]).name == "OUT");
}

TEST_CASE("Flattening never applies to the cell itself or a placeholder",
          "[cap:lvs-hierarchy][req:Flattening semantics]") {
  auto c = circuit(".subckt top A B\nXu1 A B undefined_cell\n.ends\n", "one.spice");
  const auto top = cell_id(c, "top");
  CHECK(lvs::flatten_instances(c.db, top, top) == 0);
  CHECK(lvs::flatten_instances(c.db, top, cell_id(c, "undefined_cell")) == 0);
  CHECK(c.db.cells[top].instances.size() == 1);
}

TEST_CASE("Flattening copies global nets into the parent", "[cap:lvs-hierarchy][req:Flattening semantics]") {
  auto c = circuit(R"(.global VPWR
.subckt leaf A
M1 A A VPWR VPWR pfet
.ends
.subckt top A
Xleaf1 A leaf
.ends
)",
                   "one.spice");
  const auto top = cell_id(c, "top");
  lvs::flatten_class(c.db, cell_id(c, "leaf"));
  const lvs::Cell& flat = c.db.cells[top];
  const lvs::NetId vpwr = flat.find_net("VPWR");
  REQUIRE(vpwr != lvs::kNoNet);
  CHECK(flat.net(vpwr).global);
}

TEST_CASE("Flattening evaluates expressions with the instance's parameters",
          "[cap:lvs-hierarchy][req:Flattening semantics]") {
  auto c = circuit(R"(.subckt res A B rval=1
R1 A B 'rval*2'
.ends
.subckt top x y
Xres1 x y res rval=5
.ends
)",
                   "one.spice");
  const auto top = cell_id(c, "top");
  lvs::flatten_class(c.db, cell_id(c, "res"));
  const auto& flat = c.db.cells[top];
  REQUIRE(flat.instances.size() == 1);
  REQUIRE_FALSE(flat.instances[0].properties.empty());
  bool found = false;
  for (const auto& p : flat.instances[0].properties.front()) {
    found = found || p.value == "10";
  }
  CHECK(found);
}

// ------------------------------------------------------------ pins

TEST_CASE("Duplicate pin is removed and its parent nets merged", "[cap:lvs-hierarchy][req:Duplicate and shorted pins]") {
  auto c = circuit(R"(.subckt d A A B
R1 A B 1k
.ends
.subckt top x y z
Xd1 x y z d
.ends
)",
                   "one.spice");
  const auto d = cell_id(c, "d");
  REQUIRE(c.db.cells[d].ports.size() == 3);
  const auto messages = lvs::remove_duplicate_ports(c.db, d);
  REQUIRE(messages.size() == 1);
  CHECK(messages[0] == "Duplicate pin A in cell d (0)");
  CHECK(c.db.cells[d].ports.size() == 2);
  const auto& top = c.db.cells[cell_id(c, "top")];
  REQUIRE(top.instances[0].connections.size() == 2);
  CHECK(top.net(top.instances[0].connections[0]).name == "x");
  CHECK(top.net(top.instances[0].connections[1]).name == "z");
  CHECK(top.find_net("y") == lvs::kNoNet);
}

TEST_CASE("Shorted outputs", "[cap:lvs-hierarchy][req:Duplicate and shorted pins]") {
  const std::string text = R"(.subckt buf2 A Y1 Y2 VDD VSS
M1 Y1 A VDD VDD pfet
M2 Y1 A VSS VSS nfet
.ends
.subckt top A Y VDD VSS
Xbuf2_1 A Y Y VDD VSS buf2
.ends
)";
  auto one = circuit(text, "one.spice", 0);
  auto two = circuit(text, "two.spice", 1);
  for (auto* c : {&one, &two}) {
    auto& buf2 = cell(*c, "buf2", c == &one ? 0 : 1);
    buf2.ports[2].net = buf2.ports[1].net;  // Y2 on Y1's net
  }
  const auto result = run(std::move(one), std::move(two));
  const auto console = console_of(result);
  CHECK(contains(console, "Pins Y1 and Y2 are shorted in cell buf2 (0)\n"));
  CHECK(contains(console, "Pins Y1 and Y2 are shorted in cell buf2 (1)\n"));
  CHECK_FALSE(contains(log_of(result), "are shorted"));  // console only
  REQUIRE_FALSE(result.reports.empty());
  REQUIRE(result.reports.front().pins);
  CHECK(result.reports.front().pins->first.size() == 5);  // neither removed
}

// ------------------------------------------------------------ per-pair flow

TEST_CASE("Mismatched subcell flattened", "[cap:lvs-hierarchy][req:lvs per-pair flow]") {
  const auto result = run(nand2("nfet"), nand2("pfet"));
  const auto log = log_of(result);
  CHECK(contains(log, "  Flattening non-matched subcircuits nand2 nand2\n"));
  REQUIRE(result.pairs.size() == 2);
  CHECK(result.pairs[0].status == lvs::PairStatus::mismatch);
  CHECK(result.reports[0].flattened);
  CHECK(result.failed_subcells.empty());
  // top is compared with nand2 flattened in both: its devices are the
  // four transistors.
  CHECK(result.reports[1].devices1.size() == 2);
  CHECK_FALSE(result.clean);
}

TEST_CASE("No-flatten list", "[cap:lvs-hierarchy][req:lvs per-pair flow]") {
  lvs::RunOptions options;
  options.noflatten = {"nand2"};
  const auto result = run(nand2("nfet"), nand2("pfet"), options);
  const auto log = log_of(result);
  CHECK(contains(log, "  Continuing with black-boxed subcircuits nand2 nand2\n"));
  CHECK(contains(log, "\nFinal result: Subcell(s) failed matching.\n"));
  CHECK(contains(log, "\nThe following subcells failed to match:\n nand2(0)\n nand2(1)\n"));
  CHECK(result.failed_subcells == std::vector<std::string>{"nand2(0)", "nand2(1)"});
  CHECK(result.reports[0].black_boxed);
  CHECK_FALSE(result.reports[0].flattened);
}

TEST_CASE("Symmetric pair logs the swapped-connection hint", "[cap:lvs-hierarchy][req:lvs per-pair flow]") {
  // Two parallel inverter chains whose internal nodes cannot be told apart.
  const std::string text = R"(.subckt top IN VDD VSS
M1 n1 IN VDD VDD pfet
M2 n1 IN VSS VSS nfet
M3 n2 IN VDD VDD pfet
M4 n2 IN VSS VSS nfet
C1 n1 VSS 1f
C2 n2 VSS 1f
.ends
)";
  const auto result = run(text, text);
  // Symmetries are resolved by net name here, so the pair matches.
  CHECK(result.pairs.back().status == lvs::PairStatus::match);
}

// ------------------------------------------------------------ pin matching

TEST_CASE("Reordered ports", "[cap:lvs-hierarchy][req:Pin matching]") {
  const std::string top = ".subckt top a y vdd vss\nXinv1 a y vss vdd inv\n.ends\n";
  auto one = circuit(R"(.subckt inv Y A VDD VSS
M1 Y A VDD VDD pfet
M2 Y A VSS VSS nfet
.ends
.subckt top a y vdd vss
Xinv1 y a vdd vss inv
.ends
)",
                     "one.spice", 0);
  auto two = circuit(std::string{".subckt inv A Y VSS VDD\nM1 Y A VDD VDD pfet\nM2 Y A VSS VSS nfet\n.ends\n"} + top,
                     "two.spice", 1);
  const auto inv1 = cell_id(one, "inv", 0);
  const auto inv2 = cell_id(two, "inv", 1);
  lvs::Comparison comparison{one.db, inv1, two.db, inv2, {}};
  (void)comparison.resolve();
  const auto matching = comparison.matching();
  lvs::PinMatchInput input;
  input.matching = &matching;
  const auto result = lvs::match_pins(one.db, inv1, two.db, inv2, input, {});
  CHECK(result.result == 1);
  // Regression: removing no port used to empty circuit 1's port names.
  CHECK(result.pins1 == std::vector<std::string>{"Y", "A", "VDD", "VSS"});
  CHECK(result.pins2 == std::vector<std::string>{"Y", "A", "VDD", "VSS"});
  // The schematic's instance of inv now lists its pins in the new order.
  const auto& top2 = two.db.cells[cell_id(two, "top", 1)];
  std::vector<std::string> nets;
  for (const auto net : top2.instances[0].connections) {
    nets.push_back(top2.net(net).name);
  }
  CHECK(nets == std::vector<std::string>{"y", "a", "vdd", "vss"});
  CHECK(result.output.back().text == "Cell pin lists are equivalent.\n");
}

TEST_CASE("Missing pin at top", "[cap:lvs-hierarchy][req:Pin matching]") {
  const std::string layout = ".subckt top A Y VDD VSS\nM1 Y en_int VDD VDD pfet\nM2 Y A VSS VSS nfet\n"
                             "M3 en_int A VSS VSS nfet\n.ends\n";
  const std::string schematic = ".subckt top A Y VDD VSS EN\nM1 Y EN VDD VDD pfet\nM2 Y A VSS VSS nfet\n"
                                "M3 EN A VSS VSS nfet\n.ends\n";
  const auto result = run(layout, schematic);
  const auto log = log_of(result);
  CHECK(contains(log, lvs::column_row("(no matching pin)", "EN", {})));
  REQUIRE(result.top_pin_result);
  CHECK(*result.top_pin_result == 0);
  CHECK(contains(log, "\nFinal result: Top level cell failed pin matching.\n"));
  CHECK(contains(log, "Cell pin lists for top and top altered to match.\n"));
}

TEST_CASE("Cells without pins need no pin matching", "[cap:lvs-hierarchy][req:Pin matching]") {
  const std::string text = "M1 a b c c nfet\nM2 c b a a pfet\n";
  const auto result = run(text, text);
  CHECK(contains(log_of(result), "Cells have no pins;  pin matching not needed.\n"));
  REQUIRE(result.top_pin_result);
  CHECK(*result.top_pin_result == 2);
}

TEST_CASE("Pin-match results are never -1 or -2 (Q-LVS-17)", "[cap:lvs-hierarchy][req:Pin matching]") {
  for (const auto& [a, b] : std::vector<std::pair<std::string, std::string>>{
           {".subckt top A B\nR1 A B 1k\n.ends\n", ".subckt top A B\nR1 A B 1k\n.ends\n"},
           {".subckt top A B\nR1 A B 1k\n.ends\n", ".subckt top A C\nR1 A C 1k\n.ends\n"},
           {".subckt top A B\nR1 A B 1k\n.ends\n", ".subckt top A\nR1 A x 1k\n.ends\n"},
           {".subckt top\nR1 A B 1k\n.ends\n", ".subckt top A B C\nR1 A B 1k\n.ends\n"}}) {
    const auto result = run(a, b);
    REQUIRE(result.top_pin_result);
    CHECK(*result.top_pin_result >= 0);
  }
}

TEST_CASE("Size-1 array pin names pair in a matched black box with netgen-compat ON (Q-LVS-24)",
          "[cap:lvs-hierarchy][req:Pin matching]") {
  auto one = circuit(".subckt bb a[0] y\n.ends\n", "one.spice", 0, true);
  auto two = circuit(".subckt bb a y\n.ends\n", "two.spice", 1, true);
  const auto c1 = cell_id(one, "bb", 0);
  const auto c2 = cell_id(two, "bb", 1);
  one.db.cells[c1].flags.matched = true;  // as the compare queue marks it
  two.db.cells[c2].flags.matched = true;
  lvs::PinMatchInput input;
  input.forced = true;
  input.netgen_compat = true;
  const auto result = lvs::match_pins(one.db, c1, two.db, c2, input, {});
  REQUIRE_FALSE(result.rows.empty());
  CHECK(result.rows.front().left == "a[0]");
  CHECK(result.rows.front().right == "a");
}

TEST_CASE("Size-1 array pin names need the placeholder flag with netgen-compat OFF (Q-LVS-24)",
          "[cap:lvs-hierarchy][req:Pin matching]") {
  auto one = circuit(".subckt bb a[0] y\n.ends\n", "one.spice", 0, true);
  auto two = circuit(".subckt bb a y\n.ends\n", "two.spice", 1, true);
  const auto c1 = cell_id(one, "bb", 0);
  const auto c2 = cell_id(two, "bb", 1);
  one.db.cells[c1].flags.matched = true;
  two.db.cells[c2].flags.matched = true;
  lvs::PinMatchInput input;
  input.forced = true;
  input.netgen_compat = false;
  const auto result = lvs::match_pins(one.db, c1, two.db, c2, input, {});
  REQUIRE_FALSE(result.rows.empty());
  CHECK(result.rows.front().left == "a[0]");
  CHECK(result.rows.front().right == "(no matching pin)");
  // With the placeholder flag set, OFF pairs them too.
  auto three = circuit(".subckt bb a[0] y\n.ends\n", "three.spice", 0, true);
  auto four = circuit(".subckt bb a y\n.ends\n", "four.spice", 1, true);
  three.db.cells[cell_id(three, "bb", 0)].flags.placeholder = true;
  const auto paired =
      lvs::match_pins(three.db, cell_id(three, "bb", 0), four.db, cell_id(four, "bb", 1), input, {});
  CHECK(paired.rows.front().right == "a");
}

TEST_CASE("Pin names ignore a trailing ! and a leading backslash", "[cap:lvs-hierarchy][req:Pin matching]") {
  CHECK(lvs::pin_names_equal("vdd!", "VDD", true, false));
  CHECK(lvs::pin_names_equal("\\a", "a", false, false));
  CHECK_FALSE(lvs::pin_names_equal("a[0]", "a", true, false));
  CHECK(lvs::pin_names_equal("a[0]", "a", true, true));
}

// ------------------------------------------------------------ black boxes

TEST_CASE("Analog macro black box", "[cap:lvs-hierarchy][req:Black boxes and flattening prohibitions]") {
  const std::string text = R"(.subckt adc IN OUT VDD VSS
.ends
.subckt top IN OUT VDD VSS
Xadc1 IN OUT VDD VSS adc
M1 OUT IN VDD VDD pfet
.ends
)";
  const auto result = run(circuit(text, "one.spice", 0, true), circuit(text, "two.spice", 1, true));
  const auto log = log_of(result);
  CHECK(contains(log, "Circuit 1 cell adc and Circuit 2 cell adc are black boxes.\n"));
  REQUIRE(result.pairs.size() == 2);
  CHECK(result.pairs[0].status == lvs::PairStatus::black_box);
  CHECK(result.reports[0].pins);
  CHECK(result.clean);
}

TEST_CASE("No-flatten patterns are anchored regular expressions",
          "[cap:lvs-hierarchy][req:Black boxes and flattening prohibitions]") {
  CHECK(lvs::pattern_matches("nand2", "nand2"));
  CHECK_FALSE(lvs::pattern_matches("nand", "nand2"));
  CHECK(lvs::pattern_matches("nand.*", "nand3"));
  CHECK(lvs::pattern_matches("^sky130_fd_sc_hd__[a-z]+_[0-9]$", "sky130_fd_sc_hd__inv_1"));
  CHECK_FALSE(lvs::pattern_matches("[^x]+", "abxc"));
  CHECK(lvs::pattern_matches("a\\.b?", "a."));
  // A pathological pattern stays linear (no backtracking).
  const std::string name(4096, 'a');
  CHECK_FALSE(lvs::pattern_matches("a*a*a*a*a*a*a*a*a*a*b", name));
}

TEST_CASE("flatten prohibit keeps a mismatched pair as failed subcells",
          "[cap:lvs-hierarchy][req:Black boxes and flattening prohibitions]") {
  lvs::RunOptions options;
  options.prohibited1 = {"nand2"};
  const auto result = run(nand2("nfet"), nand2("pfet"), options);
  CHECK(result.failed_subcells == std::vector<std::string>{"nand2(0)", "nand2(1)"});
}

// ------------------------------------------------------------ final result

TEST_CASE("Clean result", "[cap:lvs-hierarchy][req:Final result]") {
  const auto result = run(three_levels(), three_levels());
  const auto log = log_of(result);
  const std::string tail = "\nFinal result: Circuits match uniquely.\n.\n";
  REQUIRE(log.size() >= tail.size());
  CHECK(log.substr(log.size() - tail.size()) == tail);
  CHECK(contains(console_of(result), "LVS Done.\n"));
  CHECK(result.clean);
}

TEST_CASE("Property errors are listed after the final result", "[cap:lvs-hierarchy][req:Final result]") {
  auto one = circuit(kInv, "one.spice", 0);
  auto two = circuit(std::string{kInv}.replace(std::string{kInv}.find("W=2"), 3, "W=4"), "two.spice", 1);
  for (auto* c : {&one, &two}) {
    const lvs::FileNumber file = c == &one ? 0 : 1;
    cell(*c, "pfet", file).properties.push_back(lvs::PropertyDef{"W", lvs::PropertyType::value, 0.01, "1"});
  }
  const auto result = run(std::move(one), std::move(two));
  const auto log = log_of(result);
  CHECK(contains(log, "\nFinal result: Circuits match uniquely.\nProperty errors were found.\n"));
  CHECK(contains(log, "\nThe following cells had property errors:\n inv\n"));
  CHECK(result.property_error_cells == std::vector<std::string>{"inv"});
  CHECK(result.pairs.back().status == lvs::PairStatus::match_property_errors);
  CHECK_FALSE(result.clean);
}

TEST_CASE("Cancelled run stops before the next pair", "[cap:lvs-hierarchy][req:Final result]") {
  std::atomic<bool> flag{true};
  lvs::RunOptions options;
  options.cancel.flag = &flag;
  const auto result = run(three_levels(), three_levels(), options);
  CHECK(result.cancelled);
  CHECK_FALSE(result.clean);
  CHECK(result.pairs.empty());
}

// ------------------------------------------------------------ depth limit

namespace {

/// A chain of `depth` subcircuit levels below `top`.
std::string chain(std::size_t depth) {
  std::string text = ".subckt c0 a b\nR1 a b 1k\n.ends\n";
  for (std::size_t k = 1; k <= depth; ++k) {
    text += ".subckt c" + std::to_string(k) + " a b\nX1 a b c" + std::to_string(k - 1) + "\n.ends\n";
  }
  return text;
}

}  // namespace

TEST_CASE("Recursive subcircuit", "[cap:lvs-hierarchy][req:Hierarchy depth limit]") {
  const std::string text = R"(.subckt a x y
Xb1 x y b
.ends
.subckt b x y
Xa1 x y a
.ends
)";
  for (const bool compat : {true, false}) {
    lvs::RunOptions options;
    options.netgen_compat = compat;
    const auto result = run(text, text, options);
    CHECK(result.input_error);
    REQUIRE_FALSE(result.diagnostics.empty());
    CHECK(result.diagnostics.front().code == lvs::hier_codes::kDepthLimit);
    CHECK(result.pairs.empty());
  }
}

TEST_CASE("Depth 256 is allowed and 257 is not", "[cap:lvs-hierarchy][req:Hierarchy depth limit]") {
  auto ok = circuit(chain(256), "one.spice");
  CHECK(lvs::hierarchy_depth(ok.db, *ok.top, 256).ok());
  CHECK(lvs::hierarchy_depth(ok.db, *ok.top, 256).value() == 256);
  auto deep = circuit(chain(257), "two.spice");
  const auto depth = lvs::hierarchy_depth(deep.db, *deep.top, 256);
  REQUIRE_FALSE(depth.ok());
  CHECK(depth.error().code == lvs::hier_codes::kDepthLimit);
  CHECK(contains(depth.error().text, "c0"));  // the deepest cell
}

TEST_CASE("The depth limit can be lowered but not raised past 512", "[cap:lvs-hierarchy][req:Hierarchy depth limit]") {
  lvs::RunOptions options;
  options.hierarchy_depth = 3;
  auto result = run(chain(4), chain(4), options);
  CHECK(result.input_error);
  options.hierarchy_depth = 513;
  result = run(chain(1), chain(1), options);
  CHECK(result.input_error);
  REQUIRE_FALSE(result.diagnostics.empty());
  CHECK(result.diagnostics.front().code == lvs::hier_codes::kBadDepthSetting);
  options.hierarchy_depth = 512;
  CHECK_FALSE(run(chain(4), chain(4), options).input_error);
}
