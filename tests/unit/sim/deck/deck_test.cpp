// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Deck preparation for the spice and raw sources (task 13.1; design D11,
// D18): testbench composition, stimuli, sanitisation (Q-SIM-9), the deck
// limits (Q-SIM-25) and delivery. Files come from a MemoryFileSystem.
//
// Owner: SIMDECK.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <string>
#include <vector>

#include "hefesto/platform/file_system.hpp"
#include "hefesto/sim/deck/deck.hpp"
#include "hefesto/sim/deck/spice_text.hpp"

namespace diag = hefesto::diag;
namespace platform = hefesto::platform;
namespace sim = hefesto::sim;

namespace {

constexpr const char* kInv =
    "* inverter\n"
    ".subckt inv A Y VPWR VGND\n"
    "X0 Y A VGND VGND nfet w=0.65u l=0.15u\n"
    "X1 Y A VPWR VPWR pfet w=1u l=0.15u\n"
    ".ends\n"
    ".subckt nfet d g s b\n.ends\n"
    ".subckt pfet d g s b\n.ends\n";

struct Fixture {
  platform::MemoryFileSystem files;
  sim::DeckContext context;

  Fixture() {
    REQUIRE(files.create_directories("/proj").ok());
    REQUIRE(files.write_atomic("/proj/inv.spice", kInv).ok());
    context.files = &files;
    context.base_dir = "/proj";
    context.pdk_store = "/pdk";
  }

  void write(const std::string& path, const std::string& text) { REQUIRE(files.write_atomic(path, text).ok()); }
};

sim::Testbench inverter_bench() {
  sim::Testbench tb;
  tb.name = "inv_tran";
  tb.corner = "tt_27_1v8";
  tb.dut = sim::CircuitSource{sim::CircuitSource::Kind::spice, "inv.spice", sim::ParasiticMode::none};
  tb.subckt = "inv";
  tb.bindings = {{"A", "in"}, {"Y", "out"}, {"VPWR", "vdd"}, {"VGND", "0"}};
  tb.temperature = 27;
  tb.stimuli = {sim::Stimulus{"Vdd", "vdd", "0", sim::Stimulus::Kind::dc, {1.8}, {}, {}}};
  tb.saves = {"v(in)", "v(out)"};
  tb.lines = {".tran 1e-11 2e-08"};
  return tb;
}

sim::Testbench raw_bench(std::vector<std::string> lines) {
  sim::Testbench tb;
  tb.name = "raw";
  tb.dut = sim::CircuitSource{sim::CircuitSource::Kind::raw, "", sim::ParasiticMode::none};
  tb.lines = std::move(lines);
  return tb;
}

bool contains(const std::vector<std::string>& lines, const std::string& line) {
  return std::find(lines.begin(), lines.end(), line) != lines.end();
}

}  // namespace

TEST_CASE("Pulse line", "[cap:simulation-netlist-preparation][req:Stimulus definitions]") {
  const sim::Stimulus vin{"Vin", "in", "0", sim::Stimulus::Kind::pulse, {0, 1.8, 1e-9, 100e-12, 100e-12, 5e-9, 10e-9},
                          {}, {}};
  const auto line = sim::stimulus_line(vin);
  REQUIRE(line.ok());
  CHECK(line.value() == "Vin in 0 PULSE(0 1.8 1e-09 1e-10 1e-10 5e-09 1e-08)");
  sim::Stimulus ac{"Vac", "in", "0", sim::Stimulus::Kind::dc, {0.9}, 1.0, 90.0};
  CHECK(sim::stimulus_line(ac).value() == "Vac in 0 0.9 AC 1 90");
  const sim::Stimulus sine{"Iin", "a", "b", sim::Stimulus::Kind::sin, {0, 1e-3, 1e6}, {}, {}};
  CHECK(sim::stimulus_line(sine).value() == "Iin a b SIN(0 0.001 1000000)");
}

TEST_CASE("Decreasing PWL rejected", "[cap:simulation-netlist-preparation][req:Stimulus definitions]") {
  const sim::Stimulus vx{"Vx", "x", "0", sim::Stimulus::Kind::pwl, {0, 0, 2e-9, 1, 1e-9, 0}, {}, {}};
  const auto line = sim::stimulus_line(vx);
  REQUIRE_FALSE(line.ok());
  CHECK(line.error().code == "SIM-DECK-010");
  CHECK(line.error().text == "Source Vx: PWL time values must be non-decreasing");
  // A negative pulse width, a missing node and a wrong prefix name the source too.
  const sim::Stimulus pulse{"Vp", "p", "0", sim::Stimulus::Kind::pulse, {0, 1, 0, 1e-9, 1e-9, -1, 1e-8}, {}, {}};
  CHECK(sim::stimulus_line(pulse).error().text == "Source Vp: PULSE pw must not be negative");
  const sim::Stimulus open{"Vq", "q", "", sim::Stimulus::Kind::dc, {1}, {}, {}};
  CHECK(sim::stimulus_line(open).error().text == "Source Vq: a node is missing");
  const sim::Stimulus named{"R1", "a", "0", sim::Stimulus::Kind::dc, {1}, {}, {}};
  CHECK(sim::stimulus_line(named).error().text == "Source R1: the name must start with V or I");
}

TEST_CASE("Port bound by name", "[cap:simulation-netlist-preparation][req:Testbench composition]") {
  Fixture f;
  const auto deck = sim::prepare(inverter_bench(), f.context);
  REQUIRE(deck.ok());
  const std::vector<std::string>& lines = deck.value().lines;
  REQUIRE(lines.size() > 5);
  CHECK(lines.front() == "* hefesto testbench inv_tran corner tt_27_1v8");
  CHECK(lines[1] == ".temp 27");
  CHECK(contains(lines, "Xdut in out vdd 0 inv"));
  CHECK(contains(lines, "Vdd vdd 0 1.8"));
  CHECK(lines[lines.size() - 3] == ".save v(in) v(out)");
  CHECK(lines[lines.size() - 2] == ".tran 1e-11 2e-08");
  CHECK(lines.back() == ".end");
  CHECK(deck.value().diagnostics.empty());
  CHECK(deck.value().names.empty());
}

TEST_CASE("Unbound port", "[cap:simulation-netlist-preparation][req:Testbench composition]") {
  Fixture f;
  sim::Testbench tb = inverter_bench();
  tb.bindings = {{"A", "in"}, {"VPWR", "vdd"}, {"VGND", "0"}};
  const auto deck = sim::prepare(tb, f.context);
  REQUIRE(deck.ok());
  CHECK(contains(deck.value().lines, "Xdut in nc_Y vdd 0 inv"));
  CHECK(contains(deck.value().lines, "Rnc_Y nc_Y 0 1e+12"));
  REQUIRE(deck.value().diagnostics.size() == 1);
  CHECK(deck.value().diagnostics[0].code == "SIM-DECK-012");
  CHECK(deck.value().diagnostics[0].text == "Port Y left unconnected");
  CHECK(deck.value().diagnostics[0].severity == diag::Severity::warning);
}

TEST_CASE("A binding to a name that is not a DUT port is an error",
          "[cap:simulation-netlist-preparation][req:Testbench composition]") {
  Fixture f;
  sim::Testbench tb = inverter_bench();
  tb.bindings.push_back({"Z", "z"});
  const auto deck = sim::prepare(tb, f.context);
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().code == "SIM-DECK-011");
  CHECK(deck.error().text == "Unknown DUT port Z");
}

TEST_CASE("Corner parameters are sorted and options keep their order",
          "[cap:simulation-netlist-preparation][req:Testbench composition]") {
  Fixture f;
  sim::Testbench tb = inverter_bench();
  tb.options = {"reltol=1e-4", "gmin=1e-15"};
  tb.params = {{"vdd", 1.8}, {"corner_k", 2}};
  const auto deck = sim::prepare(tb, f.context);
  REQUIRE(deck.ok());
  const std::vector<std::string>& l = deck.value().lines;
  CHECK(l[1] == ".option reltol=1e-4");
  CHECK(l[2] == ".option gmin=1e-15");
  CHECK(l[3] == ".temp 27");
  CHECK(l[4] == ".param corner_k = 2");
  CHECK(l[5] == ".param vdd = 1.8");
}

TEST_CASE("Shell in control section", "[cap:simulation-netlist-preparation][req:Deck validation and sanitisation]") {
  Fixture f;
  f.write("/proj/inv.spice", std::string{kInv} + ".control\nshell ls\n.endc\n");
  sim::Testbench tb = inverter_bench();
  tb.allow_control = true;
  const auto deck = sim::prepare(tb, f.context);
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().code == "SIM-DECK-004");
  CHECK(deck.error().text == "Control command 'shell' is not allowed in simulation decks");
  // Forbidden commands stay rejected when allowControl is off too.
  tb.allow_control = false;
  CHECK(sim::prepare(tb, f.context).error().code == "SIM-DECK-004");
}

TEST_CASE("Control section stripped", "[cap:simulation-netlist-preparation][req:Deck validation and sanitisation]") {
  Fixture f;
  std::vector<std::string> lines = {"* rc"};
  for (int i = 2; i < 40; ++i) {
    lines.push_back("* filler " + std::to_string(i));
  }
  lines.insert(lines.end(), {".control", "run", "plot v(out)", "print v(out)", ".endc", "R1 a 0 1k", "V1 a 0 1",
                             ".op", ".end"});
  const auto deck = sim::prepare(raw_bench(lines), f.context);
  REQUIRE(deck.ok());
  CHECK_FALSE(contains(deck.value().lines, "plot v(out)"));
  CHECK_FALSE(contains(deck.value().lines, ".control"));
  REQUIRE(deck.value().diagnostics.size() == 1);
  CHECK(deck.value().diagnostics[0].code == "SIM-DECK-005");
  CHECK(deck.value().diagnostics[0].text == "Removed .control section (lines 40-44)");
  // allowControl keeps it.
  sim::Testbench kept = raw_bench(lines);
  kept.allow_control = true;
  const auto with_control = sim::prepare(kept, f.context);
  REQUIRE(with_control.ok());
  CHECK(contains(with_control.value().lines, "plot v(out)"));
  CHECK(with_control.value().diagnostics.empty());
}

TEST_CASE("A forbidden command in an included file is rejected",
          "[cap:simulation-netlist-preparation][req:Deck validation and sanitisation]") {
  Fixture f;
  f.write("/proj/hook.inc", ".control\nsource /tmp/evil\n.endc\n");
  const auto deck = sim::prepare(raw_bench({"* t", ".include hook.inc", "R1 a 0 1", ".op", ".end"}), f.context);
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().text == "Control command 'source' is not allowed in simulation decks");
}

TEST_CASE("Subcircuit structure and calls are checked",
          "[cap:simulation-netlist-preparation][req:Deck validation and sanitisation]") {
  Fixture f;
  auto open = sim::prepare(raw_bench({"* t", ".subckt a x", "R1 x 0 1", ".op", ".end"}), f.context);
  CHECK(open.error().code == "SIM-DECK-006");
  auto stray = sim::prepare(raw_bench({"* t", ".ends", ".op", ".end"}), f.context);
  CHECK(stray.error().code == "SIM-DECK-006");
  auto undefined = sim::prepare(raw_bench({"* t", "X1 a b xinv", ".op", ".end"}), f.context);
  REQUIRE_FALSE(undefined.ok());
  CHECK(undefined.error().code == "SIM-DECK-007");
  CHECK(undefined.error().text == "undefined subcircuit 'xinv' called at line 2");
  // A PDK library may define it: exempt.
  auto pdk = sim::prepare(raw_bench({"* t", ".lib \"/pdk/models.lib\" tt", "X1 a b nfet w=1u", ".op", ".end"}),
                          f.context);
  CHECK(pdk.ok());
  // Continuation lines and parameters: the called subcircuit is `inv`.
  auto continued = sim::prepare(
      raw_bench({"* t", ".include inv.spice", "X1 a b vdd", "+ 0 inv", ".tran 1n 10n", ".end"}), f.context);
  CHECK(continued.ok());
  auto missing_file = sim::prepare(raw_bench({"* t", ".include nope.spice", ".op", ".end"}), f.context);
  REQUIRE_FALSE(missing_file.ok());
  CHECK(missing_file.error().code == "SIM-DECK-001");
  CHECK(missing_file.error().text == "File not found: /proj/nope.spice (raw deck:2)");
}

TEST_CASE("Raw deck without analysis", "[cap:simulation-netlist-preparation][req:Raw decks]") {
  Fixture f;
  const auto deck = sim::prepare(raw_bench({"* rc", "R1 a 0 1k", "C1 a 0 1p", ".end"}), f.context);
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().code == "SIM-DECK-008");
  CHECK(deck.error().text == "Deck has no analysis");
  const auto no_end = sim::prepare(raw_bench({"* rc", "R1 a 0 1k", ".op"}), f.context);
  CHECK(no_end.error().code == "SIM-DECK-009");
}

TEST_CASE("A raw deck is passed unchanged apart from its includes",
          "[cap:simulation-netlist-preparation][req:Raw decks]") {
  Fixture f;
  f.write("/proj/lib/r.inc", "R2 b 0 2k\n");
  const std::vector<std::string> lines = {"* rc", "R1 a b 1k", ".include lib/r.inc", "V1 a 0 1", ".op", ".end"};
  const auto deck = sim::prepare(raw_bench(lines), f.context);
  REQUIRE(deck.ok());
  CHECK(deck.value().lines == std::vector<std::string>{"* rc", "R1 a b 1k", "* hefesto include /proj/lib/r.inc",
                                                       "R2 b 0 2k", "* hefesto end include", "V1 a 0 1", ".op",
                                                       ".end"});
  CHECK(deck.value().names.empty());
}

TEST_CASE("Include loop in a user deck", "[cap:simulation-netlist-preparation][req:Deck limits]") {
  Fixture f;
  f.write("/proj/a.inc", ".include b.inc\n");
  f.write("/proj/b.inc", ".include a.inc\n");
  const auto deck = sim::prepare(raw_bench({"* loop", ".include a.inc", ".op", ".end"}), f.context);
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().code == "SIM-DECK-002");
  CHECK(deck.error().text.starts_with("include depth limit 32 exceeded by /proj/"));
}

TEST_CASE("Deck limits can be lowered, never raised", "[cap:simulation-netlist-preparation][req:Deck limits]") {
  Fixture f;
  f.write("/proj/big.inc", std::string(4096, '*') + "\n");
  f.context.limits.max_deck_bytes = 1000;
  const auto deck = sim::prepare(raw_bench({"* t", ".include big.inc", ".op", ".end"}), f.context);
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().code == "SIM-DECK-003");
  CHECK(deck.error().text == "prepared deck larger than the limit of 1000 bytes in /proj/big.inc");
  f.context.limits.max_include_depth = 1;
  f.context.limits.max_deck_bytes = 1U << 20;
  f.write("/proj/one.inc", ".include two.inc\n");
  f.write("/proj/two.inc", "R1 a 0 1\n");
  CHECK(sim::prepare(raw_bench({"* t", ".include one.inc", ".op", ".end"}), f.context).error().code ==
        "SIM-DECK-002");
}

// Regression (M9 R2 review): a raise was silently clamped to the default,
// while design D18 allows up to twice the default and LVS refuses more
// (LVS-READ-022).
TEST_CASE("A deck limit may be raised up to twice its default, not further",
          "[cap:simulation-netlist-preparation][req:Deck limits]") {
  Fixture f;
  const sim::Testbench tb = raw_bench({"* t", "R1 a 0 1", ".op", ".end"});
  f.context.limits = sim::DeckLimits{2 * sim::kMaxPreparedDeckBytes, 2 * sim::kMaxDeckIncludeDepth,
                                     2 * sim::kMaxDeckIncludes};
  CHECK(sim::prepare(tb, f.context).ok());
  f.context.limits.max_include_depth = 2 * sim::kMaxDeckIncludeDepth + 1;
  const auto deeper = sim::prepare(tb, f.context);
  REQUIRE_FALSE(deeper.ok());
  CHECK(deeper.error().code == "SIM-DECK-017");
  CHECK(deeper.error().text == "include depth limit 65 is zero or above twice its default 32");
  f.context.limits = sim::DeckLimits{};
  f.context.limits.max_includes = 0;
  CHECK(sim::prepare(tb, f.context).error().code == "SIM-DECK-017");
}

// Regression (M9 R2 review): a raw deck's top file was level 0 while a
// spice source's DUT file was level 1. Both are level 1 now, as in LVS.
TEST_CASE("The include depth counts the top file for raw and spice sources alike",
          "[cap:simulation-netlist-preparation][req:Deck limits]") {
  Fixture f;
  f.context.limits.max_include_depth = 2;
  f.write("/proj/one.inc", ".include two.inc\n");
  f.write("/proj/two.inc", "R1 a 0 1\n");
  const auto raw_two = sim::prepare(raw_bench({"* t", ".include two.inc", ".op", ".end"}), f.context);
  CHECK(raw_two.ok());
  const auto raw_three = sim::prepare(raw_bench({"* t", ".include one.inc", ".op", ".end"}), f.context);
  REQUIRE_FALSE(raw_three.ok());
  CHECK(raw_three.error().text == "include depth limit 2 exceeded by /proj/two.inc (/proj/one.inc:1)");
  f.write("/proj/inv.spice", std::string{kInv} + ".include two.inc\n");
  CHECK(sim::prepare(inverter_bench(), f.context).ok());
  f.write("/proj/inv.spice", std::string{kInv} + ".include one.inc\n");
  const auto spice_three = sim::prepare(inverter_bench(), f.context);
  REQUIRE_FALSE(spice_three.ok());
  CHECK(spice_three.error().text == "include depth limit 2 exceeded by /proj/two.inc (/proj/one.inc:1)");
}

// Regression (M9 R2 review, standing rule on crafted-input limits): files
// that each include the next twice grew the work exponentially below the
// depth limit; only the 256 MiB output cap stopped it, after millions of
// reads.
TEST_CASE("Deck includes are counted", "[cap:simulation-netlist-preparation][req:Deck limits]") {
  Fixture f;
  for (int level = 0; level < 20; ++level) {
    const std::string next = "f" + std::to_string(level + 1) + ".inc";
    f.write("/proj/f" + std::to_string(level) + ".inc", ".include " + next + "\n.include " + next + "\n");
  }
  f.write("/proj/f20.inc", "R1 a 0 1\n");
  const auto deck = sim::prepare(raw_bench({"* t", ".include f0.inc", ".op", ".end"}), f.context);
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().code == "SIM-DECK-014");
  CHECK(deck.error().text.starts_with("include count limit 65536 exceeded by /proj/f"));
}

// Regression (M9 R2 review): testbench strings were copied into deck lines
// unchecked, so one entry could carry a `.control` section past Q-SIM-9's
// line-by-line sanitisation, and the deck hash was that of the split text.
TEST_CASE("A deck line holding a line break is refused",
          "[cap:simulation-netlist-preparation][req:Deck validation and sanitisation]") {
  Fixture f;
  sim::Testbench tb = inverter_bench();
  tb.saves = {"v(out)\n.control\nshell rm -rf ~\n.endc"};
  const auto deck = sim::prepare(tb, f.context);
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().code == "SIM-DECK-015");
  tb = inverter_bench();
  tb.name = "inv\r.control";
  CHECK(sim::prepare(tb, f.context).error().code == "SIM-DECK-015");
  const auto raw = sim::prepare(raw_bench({"* t", std::string{"R1 a 0 1\0x", 10}, ".op", ".end"}), f.context);
  REQUIRE_FALSE(raw.ok());
  CHECK(raw.error().text == "line 2 of the deck holds a newline, carriage return or NUL character");
}

// Regression (M9 R2 review): `.lib` lines of an inlined file kept their
// relative path, which then resolved against the deck's folder; and a user
// `.lib` file was never sanitised.
TEST_CASE("Relative .lib paths become absolute, and .lib files must lie in the PDK store",
          "[cap:simulation-netlist-preparation][req:Model library resolution]") {
  Fixture f;
  f.write("/pdk/sky130A/models/corner.inc", ".lib ../libs/models.lib tt\n");
  const auto deck = sim::prepare(
      raw_bench({"* t", ".include /pdk/sky130A/models/corner.inc", "X1 a b nfet", ".op", ".end"}), f.context);
  REQUIRE(deck.ok());
  CHECK(contains(deck.value().lines, ".lib \"/pdk/sky130A/libs/models.lib\" tt"));
  const auto user = sim::prepare(raw_bench({"* t", ".lib mine.lib tt", ".op", ".end"}), f.context);
  REQUIRE_FALSE(user.ok());
  CHECK(user.error().code == "SIM-DECK-016");
  CHECK(user.error().text == ".lib file /proj/mine.lib is outside the PDK store (raw deck:2)");
  const auto escape = sim::prepare(raw_bench({"* t", ".lib /pdk/../etc/x.lib tt", ".op", ".end"}), f.context);
  CHECK(escape.error().code == "SIM-DECK-016");
  // `.lib <section>` opens a section of a library file: kept as it is.
  f.write("/proj/sections.inc", ".lib tt\n.endl\n");
  CHECK(sim::prepare(raw_bench({"* t", ".include sections.inc", "R1 a 0 1", ".op", ".end"}), f.context).ok());
}

// Regression (M9 R2 review): a comment longer than the delivery limit was
// split into `*...` and `+...`, and ngspice joins a `+` line to the
// previous card.
TEST_CASE("A comment line longer than 65535 bytes is cut, not continued",
          "[cap:simulation-netlist-preparation][req:Deck delivery]") {
  Fixture f;
  const std::string comment = "* " + std::string(70000, 'c') + " x";
  const auto deck = sim::prepare(raw_bench({"* t", "R1 a 0 1", comment, ".op", ".end"}), f.context);
  REQUIRE(deck.ok());
  const std::vector<std::string>& l = deck.value().lines;
  REQUIRE(l.size() == 5);
  CHECK(l[2] == comment.substr(0, 65535));
  CHECK(l[3] == ".op");
}

// Regression (M9 R2 review): ngspice never parses a deck's first line, but
// it was inlined and sanitised as a card.
TEST_CASE("A raw deck's title line is kept as it is",
          "[cap:simulation-netlist-preparation][req:Raw decks]") {
  Fixture f;
  const auto include = sim::prepare(raw_bench({".include nope.spice", "R1 a 0 1", ".op", ".end"}), f.context);
  REQUIRE(include.ok());
  CHECK(include.value().lines.front() == ".include nope.spice");
  const auto control = sim::prepare(raw_bench({".control", "R1 a 0 1", ".op", ".end"}), f.context);
  REQUIRE(control.ok());
  CHECK(control.value().lines.front() == ".control");
  CHECK(control.value().diagnostics.empty());
}

TEST_CASE("Re-run is identical", "[cap:simulation-netlist-preparation][req:Deck delivery]") {
  Fixture f;
  const auto a = sim::prepare(inverter_bench(), f.context);
  const auto b = sim::prepare(inverter_bench(), f.context);
  REQUIRE(a.ok());
  REQUIRE(b.ok());
  CHECK(a.value().lines == b.value().lines);
  CHECK(a.value().sha256.size() == 64);
  CHECK(a.value().sha256 == b.value().sha256);
  f.write("/proj/inv.spice", std::string{kInv} + "* changed\n");
  CHECK(sim::prepare(inverter_bench(), f.context).value().sha256 != a.value().sha256);
}

TEST_CASE("Lines longer than 65535 bytes are split with continuation lines",
          "[cap:simulation-netlist-preparation][req:Deck delivery]") {
  Fixture f;
  std::string longline = "R1 a 0";
  while (longline.size() < 70000) {
    longline += " m=1";
  }
  const auto deck = sim::prepare(raw_bench({"* t", longline, ".op", ".end"}), f.context);
  REQUIRE(deck.ok());
  const std::vector<std::string>& l = deck.value().lines;
  REQUIRE(l.size() == 5);
  CHECK(l[1].size() <= 65535);
  CHECK(l[2].starts_with("+ "));
  CHECK(l[2].size() <= 65535);
  CHECK(l[1] + l[2].substr(1) == longline);
}

TEST_CASE("Layout and schematic sources wait for task 13.4",
          "[cap:simulation-netlist-preparation][req:Circuit sources]") {
  Fixture f;
  sim::Testbench tb = inverter_bench();
  tb.dut.kind = sim::CircuitSource::Kind::layout;
  CHECK(sim::prepare(tb, f.context).error().code == "CORE-NOTIMPL-009");
  tb.dut.kind = sim::CircuitSource::Kind::schematic;
  CHECK(sim::prepare(tb, f.context).error().code == "CORE-NOTIMPL-009");
  tb.dut.kind = sim::CircuitSource::Kind::spice;
  CHECK(sim::prepare(tb, sim::DeckContext{}).error().code == "SIM-DECK-013");
  tb.subckt = "nope";
  CHECK(sim::prepare(tb, f.context).error().code == "SIM-DECK-013");
}

TEST_CASE("SPICE numbers take scale suffixes", "[cap:simulation-netlist-preparation][req:Stimulus definitions]") {
  using Catch::Matchers::WithinRel;
  CHECK_THAT(sim::parse_spice_number("10n").value(), WithinRel(10e-9));
  CHECK_THAT(sim::parse_spice_number("2.5Meg").value(), WithinRel(2.5e6));
  CHECK_THAT(sim::parse_spice_number("1.8V").value(), WithinRel(1.8));
  CHECK_THAT(sim::parse_spice_number("3mil").value(), WithinRel(76.2e-6));
  CHECK_THAT(sim::parse_spice_number("-1e-9s").value(), WithinRel(-1e-9));
  CHECK_FALSE(sim::parse_spice_number("abc").has_value());
  CHECK(sim::format_g12(1e-11) == "1e-11");
}
