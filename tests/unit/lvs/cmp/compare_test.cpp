// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs-comparison-algorithm: graph, partition, refinement, verification,
// symmetry resolution, verify, equate, stepping, determinism and
// cancellation. Netlists are Hefesto-authored.
//
// Owner: LVSALG.

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <string>

#include "cmp_fixture.hpp"
#include "hefesto/lvs/cmp/codes.hpp"

namespace {

using lvs_cmp_test::kInverter;
using lvs_cmp_test::logged;
using lvs_cmp_test::Pair;
namespace lvs = hefesto::lvs;

constexpr std::string_view kNor = R"(.subckt nor A B Y VDD VSS
M1 Y A VDD VDD pfet
M2 Y B VDD VDD pfet
M3 Y A VSS VSS nfet
M4 Y B VSS VSS nfet
.ends
)";

// Four identical inverters in parallel between one input and one output.
constexpr std::string_view kFourInverters1 = R"(.subckt quad IN OUT VDD VSS
X1 IN OUT VDD VSS inv
X2 IN OUT VDD VSS inv
X3 IN OUT VDD VSS inv
X4 IN OUT VDD VSS inv
.ends
)";

// Two inverters in parallel, each with its own internal output node that
// drives a separate load; the internal node names differ between circuits.
constexpr std::string_view kSymmetric1 = R"(.subckt top IN VDD VSS
M1 n1 IN VDD VDD pfet
M2 n1 IN VSS VSS nfet
M3 n2 IN VDD VDD pfet
M4 n2 IN VSS VSS nfet
C1 n1 VSS 1f
C2 n2 VSS 1f
.ends
)";
constexpr std::string_view kSymmetric2 = R"(.subckt top IN VDD VSS
M1 p IN VDD VDD pfet
M2 p IN VSS VSS nfet
M3 q IN VDD VDD pfet
M4 q IN VSS VSS nfet
C1 p VSS 1f
C2 q VSS 1f
.ends
)";

}  // namespace

// ------------------------------------------------------------ graph

TEST_CASE("Unconnected port is not a net", "[cap:lvs-comparison-algorithm][req:Comparison graph]") {
  const std::string inv = ".subckt inv A Y VDD VSS NC\nM1 Y A VDD VDD pfet\nM2 Y A VSS VSS nfet\n.ends\n";
  Pair pair{inv, inv};
  const auto comparison = pair.comparison();
  REQUIRE(comparison.initialized());
  CHECK(comparison.summary().net_vertices == 8);  // 4 per circuit
  CHECK(comparison.summary().device_vertices == 4);
}

TEST_CASE("A circuit with no devices stops the comparison", "[cap:lvs-comparison-algorithm][req:Comparison graph]") {
  Pair pair{".subckt e A\n.ends\n", kInverter};
  const auto result = pair.compare();
  CHECK(result.status == lvs::PairStatus::mismatch);
  REQUIRE_FALSE(result.diagnostics.empty());
  CHECK(result.diagnostics.front().code == lvs::cmp_codes::kEmptyCircuit);
  CHECK(logged(result, "Circuit e contains no devices."));
}

// ------------------------------------------------- identity and labels

TEST_CASE("Source and drain interchangeable", "[cap:lvs-comparison-algorithm][req:Class identity and pin labels]") {
  // A load on the drain makes the two terminals distinguishable.
  const std::string layout = ".subckt c out g gnd b\nM1 out g gnd b nfet\nR1 out g 1k\n.ends\n";
  const std::string schematic = ".subckt c out g gnd b\nM1 gnd g out b nfet\nR1 out g 1k\n.ends\n";
  Pair fixed{layout, schematic};
  CHECK(fixed.compare().status == lvs::PairStatus::mismatch);
  Pair permuted{layout, schematic};
  permuted.permute("nfet", "drain", "source");
  const auto result = permuted.compare();
  CHECK(result.status == lvs::PairStatus::match);
  CHECK(logged(result, "Netlists match uniquely."));
}

TEST_CASE("Equated classes share one identity", "[cap:lvs-comparison-algorithm][req:Class identity and pin labels]") {
  Pair pair{".subckt c a b\nM1 a b a a nfet\n.ends\n", ".subckt c a b\nM1 a b a a nmos_lvt\n.ends\n"};
  CHECK(pair.compare().status == lvs::PairStatus::mismatch);
  lvs::CompareOptions options;
  options.equated_classes.emplace_back("nfet", "nmos_lvt");
  CHECK(pair.compare(options).status == lvs::PairStatus::match);
}

// --------------------------------------------------- initial partition

TEST_CASE("Fanout split", "[cap:lvs-comparison-algorithm][req:Initial partition]") {
  Pair pair{kInverter, kInverter};
  const auto comparison = pair.comparison();
  const auto summary = comparison.summary();
  CHECK(summary.net_vertices == 8);
  CHECK(summary.net_classes == 1);     // every net has fanout 2
  CHECK(summary.device_classes == 2);  // pfet, nfet
  CHECK(summary.illegal_net_classes == 0);
}

// ---------------------------------------------------------- legality

TEST_CASE("Extra device", "[cap:lvs-comparison-algorithm][req:Partition legality]") {
  Pair pair{".subckt c a b g\nM1 a g b b nfet\nM2 a g b b nfet\nM3 a g b b nfet\n.ends\n",
            ".subckt c a b g\nM1 a g b b nfet\nM2 a g b b nfet\n.ends\n"};
  auto comparison = pair.comparison();
  CHECK(comparison.summary().illegal_device_classes == 1);
  const auto result = comparison.resolve();
  CHECK(result.verify == -1);
  CHECK(result.status == lvs::PairStatus::mismatch);
  CHECK(logged(result, "Netlists do not match."));
}

TEST_CASE("The bad-match condition stays until restart", "[cap:lvs-comparison-algorithm][req:Partition legality]") {
  Pair pair{".subckt c a b g\nM1 a g b b nfet\nM2 a g b b nfet\n.ends\n", ".subckt c a b g\nM1 a g b b nfet\n.ends\n"};
  auto comparison = pair.comparison();
  comparison.converge();
  CHECK(comparison.verify(lvs::VerifyOption::equivalent, false).value == 0);  // -1: equivalent returns 0
  CHECK_FALSE(comparison.restart().has_value());
  CHECK(comparison.summary().illegal_device_classes == 1);
}

// --------------------------------------------------------- refinement

namespace {

// Two inverters drive a NAND; circuit 2 swaps which driver feeds which
// NAND input and also feeds one inverter from the other's input.
constexpr std::string_view kDriven1 = R"(.subckt top a b y VDD VSS
X1 a na VDD VSS inv
X2 b nb VDD VSS inv
X3 na nb y VDD VSS nand
.ends
)";
constexpr std::string_view kDriven2 = R"(.subckt top a b y VDD VSS
X1 a na VDD VSS inv
X2 a nb VDD VSS inv
X3 na nb y VDD VSS nand
.ends
)";

std::string with_cells(std::string_view top) {
  return std::string{".subckt inv A Y VDD VSS\n.ends\n.subckt nand A B Y VDD VSS\n.ends\n"} + std::string{top};
}

}  // namespace

TEST_CASE("Swapped connection detected", "[cap:lvs-comparison-algorithm][req:Iterative refinement]") {
  Pair pair{with_cells(kDriven1), with_cells(kDriven2)};
  const auto result = pair.compare();
  CHECK(result.verify == -1);
  CHECK(result.status == lvs::PairStatus::mismatch);
  auto comparison = pair.comparison();
  comparison.converge();
  const auto verify = comparison.verify(lvs::VerifyOption::none, true);
  CHECK(verify.net_report);
  CHECK_FALSE(verify.bad_nets.empty());
}

TEST_CASE("Identical circuits converge to a unique match", "[cap:lvs-comparison-algorithm][req:Iterative refinement]") {
  Pair pair{with_cells(kDriven1), with_cells(kDriven1)};
  const auto result = pair.compare();
  CHECK(result.status == lvs::PairStatus::match);
  CHECK(result.matching.devices.size() == 3);
  CHECK(result.matching.nets.size() == 7);
}

TEST_CASE("Colliding keys are split by the structural signature",
          "[cap:lvs-comparison-algorithm][req:Iterative refinement]") {
  // Regression (Q-LVS-4 fix, both modes): with every refinement key forced
  // to collide, only the signature check separates the vertices; without
  // it a mismatch would be classed as a match.
  for (const bool compat : {true, false}) {
    lvs::CompareOptions options;
    options.netgen_compat = compat;
    options.key_mask = 0;
    Pair mismatch{with_cells(kDriven1), with_cells(kDriven2)};
    CHECK(mismatch.compare(options).status == lvs::PairStatus::mismatch);
    Pair same{with_cells(kDriven1), with_cells(kDriven1)};
    const auto result = same.compare(options);
    CHECK(result.status == lvs::PairStatus::match);
    CHECK(result.matching.nets.size() == 7);
  }
}

// -------------------------------------------------------- verification

TEST_CASE("Symmetric parallel devices", "[cap:lvs-comparison-algorithm][req:Match verification]") {
  const std::string cell = ".subckt c d g s\nM1 d g s s nfet\nM2 d g s s nfet\n.ends\n";
  Pair pair{cell, cell};
  auto comparison = pair.comparison();
  comparison.converge();
  const auto k = comparison.verify(lvs::VerifyOption::equivalent, false).value;
  REQUIRE(k.has_value());
  CHECK(*k > 0);
}

TEST_CASE("Extra disconnected pin", "[cap:lvs-comparison-algorithm][req:Match verification]") {
  Pair pair{".subckt inv A Y VDD VSS TEST\nM1 Y A VDD VDD pfet W=2 L=0.15\nM2 Y A VSS VSS nfet W=1 L=0.15\n.ends\n", kInverter};
  const auto result = pair.compare();
  CHECK(result.verify == -2);
  CHECK(result.status == lvs::PairStatus::match_port_errors);
  CHECK(logged(result, "Netlists match uniquely with port errors."));
}

TEST_CASE("A disconnected port needs a same-named partner", "[cap:lvs-comparison-algorithm][req:Match verification]") {
  Pair same{".subckt c A Y X\nM1 Y A Y Y nfet\n.ends\n", ".subckt c A Y X\nM1 Y A Y Y nfet\n.ends\n"};
  CHECK(same.compare().verify == 0);
  Pair renamed{".subckt c A Y X\nM1 Y A Y Y nfet\n.ends\n", ".subckt c A Y Z\nM1 Y A Y Y nfet\n.ends\n"};
  CHECK(renamed.compare().verify == -2);
}

// --------------------------------------------------------- resolution

TEST_CASE("Symmetry broken by pin name", "[cap:lvs-comparison-algorithm][req:Symmetry resolution]") {
  Pair pair{kNor, kNor};
  const auto result = pair.compare();
  CHECK(result.status == lvs::PairStatus::match);
  CHECK(logged(result, "Resolving symmetries by pin name."));
  CHECK(logged(result, "Netlists match uniquely."));
  const auto& one = pair.db.cells.at(pair.one);
  const auto& two = pair.db.cells.at(pair.two);
  for (const auto& [a, b] : result.matching.nets) {
    CHECK(one.net(a).name == two.net(b).name);
  }
  CHECK(result.matching.nets.size() == 5);
}

TEST_CASE("Symmetry broken by property value", "[cap:lvs-comparison-algorithm][req:Symmetry resolution]") {
  // The two branches differ only in the capacitor value.
  const std::string one = ".subckt top IN VSS\nR1 IN a 1k\nC1 a VSS 1p\nR2 IN b 1k\nC2 b VSS 2p\n.ends\n";
  const std::string two = ".subckt top IN VSS\nR1 IN x 1k\nC1 x VSS 2p\nR2 IN y 1k\nC2 y VSS 1p\n.ends\n";
  Pair pair{one, two};
  const auto result = pair.compare();
  CHECK(logged(result, "Resolving symmetries by property value."));
  CHECK(result.status == lvs::PairStatus::match);
  CHECK(result.property_state == lvs::PropertyState::none);
  // C1 (1p) pairs with C2 (1p).
  const auto& cell1 = pair.db.cells.at(pair.one);
  const auto& cell2 = pair.db.cells.at(pair.two);
  for (const auto& [a, b] : result.matching.devices) {
    if (cell1.instances[a].name == "c:1") {
      CHECK(cell2.instances[b].name == "c:2");
    }
  }
}

TEST_CASE("Arbitrary pairing resolves the rest", "[cap:lvs-comparison-algorithm][req:Symmetry resolution]") {
  Pair pair{kSymmetric1, kSymmetric2};
  const auto result = pair.compare();
  CHECK(result.status == lvs::PairStatus::match);
  CHECK(logged(result, "Netlists match uniquely."));
  CHECK(result.matching.devices.size() == 6);
}

// ----------------------------------------------------- deterministic

TEST_CASE("Arbitrary pairing takes the last vertices in vertex order",
          "[cap:lvs-comparison-algorithm][req:Deterministic arbitrary pairing]") {
  Pair pair{kSymmetric1, kSymmetric2};
  const auto result = pair.compare();
  const auto& cell1 = pair.db.cells.at(pair.one);
  const auto& cell2 = pair.db.cells.at(pair.two);
  // The first symmetric class is the pfet class: its last devices (M3 of
  // each circuit) are paired first, so n2 pairs with q.
  for (const auto& [a, b] : result.matching.nets) {
    if (cell1.net(a).name == "n2") {
      CHECK(cell2.net(b).name == "q");
    }
  }
}

TEST_CASE("Cross-platform pairing", "[cap:lvs-comparison-algorithm][req:Deterministic arbitrary pairing]") {
  const std::string inv = ".subckt inv A Y VDD VSS\nM1 Y A VDD VDD pfet\nM2 Y A VSS VSS nfet\n.ends\n";
  Pair pair{inv + std::string{kFourInverters1}, inv + std::string{kFourInverters1}};
  const auto first = pair.compare();
  CHECK(first.status == lvs::PairStatus::match);
  // The pairing is a pure function of the inputs and the seed: the
  // expected pairs are fixed here, so every platform must produce them.
  CHECK(first.matching.devices ==
        std::vector<std::pair<std::uint32_t, std::uint32_t>>{{0, 0}, {1, 1}, {2, 2}, {3, 3}});
}

TEST_CASE("Repeated runs", "[cap:lvs-comparison-algorithm][req:Single-threaded comparison per pair]") {
  Pair pair{kSymmetric1, kSymmetric2};
  const auto reference = pair.compare();
  for (int i = 0; i < 100; ++i) {
    const auto again = pair.compare();
    REQUIRE(again.matching.devices == reference.matching.devices);
    REQUIRE(again.matching.nets == reference.matching.nets);
    REQUIRE(again.log == reference.log);
    REQUIRE(again.status == reference.status);
  }
}

TEST_CASE("Repeatable result", "[cap:lvs-comparison-algorithm][req:Pseudo-random labels]") {
  Pair pair{kSymmetric1, kSymmetric2};
  auto comparison = pair.comparison();
  const auto first = comparison.resolve();
  REQUIRE_FALSE(comparison.restart().has_value());
  const auto second = comparison.resolve();
  CHECK(first.matching.nets == second.matching.nets);
  CHECK(first.status == second.status);
}

// ------------------------------------------------------------ verify

TEST_CASE("Final unique result text", "[cap:lvs-comparison-algorithm][req:verify command]") {
  // Q-LVS-21 (replicate): the "." line in both netgen-compat modes.
  for (const bool compat : {true, false}) {
    Pair pair{kInverter, kInverter};
    lvs::CompareOptions options;
    options.netgen_compat = compat;
    auto comparison = pair.comparison(options);
    comparison.converge();
    const auto out = comparison.verify(lvs::VerifyOption::only, false);
    REQUIRE(out.lines.size() == 2);
    CHECK(out.lines[0].text == "Circuits match uniquely.");
    CHECK(out.lines[1].text == ".");
    CHECK(comparison.verify(lvs::VerifyOption::unique, false).value == 1);
    CHECK(comparison.verify(lvs::VerifyOption::equivalent, false).value == 1);
  }
}

TEST_CASE("verify on a mismatch", "[cap:lvs-comparison-algorithm][req:verify command]") {
  Pair pair{with_cells(kDriven1), with_cells(kDriven2)};
  auto comparison = pair.comparison();
  comparison.converge();
  const auto none = comparison.verify(lvs::VerifyOption::none, false);
  CHECK(none.net_report);
  CHECK(none.device_report);
  const auto nodes = comparison.verify(lvs::VerifyOption::nodes, false);
  CHECK(nodes.net_report);
  CHECK_FALSE(nodes.device_report);
  const auto only = comparison.verify(lvs::VerifyOption::only, false);
  REQUIRE(only.lines.size() == 2);
  CHECK(only.lines[0].text == "Netlists do not match.");
  CHECK(only.lines[1].text == "Port matching may fail to disambiguate symmetries.");
  CHECK(comparison.verify(lvs::VerifyOption::unique, false).value == 0);
}

TEST_CASE("verify with port errors", "[cap:lvs-comparison-algorithm][req:verify command]") {
  Pair pair{".subckt inv A Y VDD VSS TEST\nM1 Y A VDD VDD pfet W=2 L=0.15\nM2 Y A VSS VSS nfet W=1 L=0.15\n.ends\n", kInverter};
  auto comparison = pair.comparison();
  comparison.converge();
  CHECK(comparison.verify(lvs::VerifyOption::equivalent, false).value == 1);
  CHECK(comparison.verify(lvs::VerifyOption::unique, false).value == -2);
  const auto all = comparison.verify(lvs::VerifyOption::all, false);
  REQUIRE(all.lines.size() == 1);
  CHECK(all.lines[0].text == "Circuits match uniquely with port errors.");
}

TEST_CASE("Symmetry verdict reaches the log only with netgen-compat OFF",
          "[cap:lvs-comparison-algorithm][req:verify command]") {
  // Q-LVS-20 (compat): one test per mode through the branch point.
  const std::string cell = ".subckt c d g s\nM1 d g s s nfet\nM2 d g s s nfet\n.ends\n";
  SECTION("netgen-compat ON: console only") {
    CHECK_FALSE(lvs::symmetry_verdict_in_log(true));
    Pair pair{cell, cell};
    auto comparison = pair.comparison();
    comparison.converge();
    const auto out = comparison.verify(lvs::VerifyOption::only, false);
    REQUIRE(out.lines.size() == 1);
    CHECK(out.lines[0].text.starts_with("Circuits match with "));
    CHECK(out.lines[0].console_only);
  }
  SECTION("netgen-compat OFF: also in the log") {
    CHECK(lvs::symmetry_verdict_in_log(false));
    Pair pair{cell, cell};
    lvs::CompareOptions options;
    options.netgen_compat = false;
    auto comparison = pair.comparison(options);
    comparison.converge();
    const auto out = comparison.verify(lvs::VerifyOption::only, false);
    REQUIRE(out.lines.size() == 1);
    CHECK_FALSE(out.lines[0].console_only);
  }
}

TEST_CASE("verify option prefixes", "[cap:lvs-comparison-algorithm][req:verify command]") {
  CHECK(lvs::parse_verify_option("prop") == lvs::VerifyOption::properties);
  CHECK(lvs::parse_verify_option("eq") == lvs::VerifyOption::equivalent);
  CHECK(lvs::parse_verify_option("u") == lvs::VerifyOption::unique);
  CHECK_FALSE(lvs::parse_verify_option("x").has_value());
  Pair empty{".subckt e A\n.ends\n", ".subckt e A\n.ends\n"};
  auto comparison = empty.comparison();
  CHECK(comparison.verify(lvs::VerifyOption::unique, false).value == -1);
  const auto out = comparison.verify(lvs::VerifyOption::only, false);
  REQUIRE(out.lines.size() == 1);
  CHECK(out.lines[0].text == "Verify:  cell e has no elements and/or nodes.  Not checked.");
}

// ---------------------------------------------------- equivalences

TEST_CASE("Equate across classes fails", "[cap:lvs-comparison-algorithm][req:Forced equivalences]") {
  Pair pair{kInverter, kInverter};
  auto comparison = pair.comparison();
  comparison.converge();
  const auto out = comparison.equate_nodes("A", "Y");
  CHECK(out.value == 0);
  CHECK(out.message == "Unable to equate nodes A and Y.");
}

TEST_CASE("Equate within a class splits the pair out", "[cap:lvs-comparison-algorithm][req:Forced equivalences]") {
  Pair pair{kSymmetric1, kSymmetric2};
  auto comparison = pair.comparison();
  comparison.converge();
  const auto nodes = comparison.equate_nodes("n1", "q");
  CHECK(nodes.value == 1);
  CHECK(nodes.message == "Nodes n1 and q are equivalent.");
  const auto result = comparison.resolve();
  const auto& cell1 = pair.db.cells.at(pair.one);
  const auto& cell2 = pair.db.cells.at(pair.two);
  for (const auto& [a, b] : result.matching.nets) {
    if (cell1.net(a).name == "n1") {
      CHECK(cell2.net(b).name == "q");
    }
  }
  Pair none{".subckt e A\n.ends\n", ".subckt e A\n.ends\n"};
  auto empty = none.comparison();
  CHECK(empty.equate_elements("x", "y").message == "Circuits not being compared, must specify netlists.");
}

// ------------------------------------------------------- stepping

TEST_CASE("Reset and stepping", "[cap:lvs-comparison-algorithm][req:Reset and stepping commands]") {
  Pair pair{kSymmetric1, kSymmetric2};
  auto comparison = pair.comparison();
  CHECK(comparison.exhaustive(std::nullopt) == "Exhaustive subdivision DISABLED.");
  CHECK(comparison.exhaustive(true) == "Exhaustive subdivision ENABLED.");
  CHECK(lvs::Comparison::symmetry_command() == "Symmetry breaking method has been deprecated.");
  const auto initial = comparison.summary();
  int rounds = 0;
  while (!comparison.iterate() && rounds < 100) {
    ++rounds;
  }
  CHECK(comparison.summary().net_classes > initial.net_classes);
  CHECK_FALSE(comparison.symmetries().empty());
  REQUIRE_FALSE(comparison.restart().has_value());
  CHECK(comparison.summary().net_classes == initial.net_classes);
  Pair none{".subckt e A\n.ends\n", ".subckt e A\n.ends\n"};
  auto empty = none.comparison();
  CHECK(empty.restart() == std::optional<std::string>{"Need to initialize data structures first!"});
}

// ----------------------------------------------------------- cancel

TEST_CASE("Cancel", "[cap:lvs-comparison-algorithm][req:Long-running comparisons]") {
  std::atomic<bool> flag{true};
  lvs::CompareOptions options;
  options.cancel.flag = &flag;
  Pair pair{kSymmetric1, kSymmetric2};
  const auto result = pair.compare(options);
  CHECK(result.cancelled);
  REQUIRE_FALSE(result.diagnostics.empty());
  CHECK(result.diagnostics.back().code == lvs::cmp_codes::kCancelled);
  CHECK_FALSE(logged(result, "Netlists match uniquely."));
}
