// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The schematic circuit source, M8 part (openspec/changes/m8-schematic task
// 11.1, design D13; capability schematic-simulation-integration "Schematic
// circuit source", "Testbench code blocks", "Control block translation"):
// the deck text, the hand-off to simulation-netlist-preparation, the
// `.include`/`.lib` resolution and the `.control` translation, and the deck
// of golden case SCHEM-NL-07's IHP `dc_lv_nmos.sch` (the deck SCHEM-SIM-01
// captured; skipped when the IHP xschem data set is not fetched).
//
// Owner: SIMINT.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "../cmd/cmd_fixture.hpp"
#include "../devlib/netlist_support.hpp"
#include "hefesto/schem/sim/deck.hpp"
#include "reference_data.hpp"
#include "sim_support.hpp"

namespace schem = hefesto::schem;
namespace platform = hefesto::platform;
namespace dl = hefesto::test::devlib;

#define SOURCE "[cap:schematic-simulation-integration][req:Schematic circuit source]"
#define CODE_BLOCKS "[cap:schematic-simulation-integration][req:Testbench code blocks]"
#define CONTROL "[cap:schematic-simulation-integration][req:Control block translation]"

namespace {

constexpr const char* kHeader = "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n";

// A testbench: a resistor between `in` and GND and a code block whose value
// is `code`.
std::string testbench(const std::string& code) {
  return std::string{kHeader} +
         "C {res.sym} 0 0 0 0 {name=R1 value=1k}\n"
         "C {lab_pin.sym} 0 -30 0 0 {name=l1 lab=in}\n"
         "C {gnd.sym} 0 30 0 0 {name=l2 lab=GND}\n"
         "C {code.sym} 100 0 0 0 {name=s1 only_toplevel=false value=\"" +
         code + "\"}\n";
}

const char* const kControl =
    ".control\nsave all\nop\nwrite dc.raw\nset appendwrite\ndc V1 0 1 0.1\nwrite dc.raw v(in)\nquit\n.endc";

std::vector<std::string> lines_with(const std::string& text, std::string_view needle) {
  std::vector<std::string> out;
  for (const std::string& line : dl::lines_of(text)) {
    if (line.find(needle) != std::string::npos) out.push_back(line);
  }
  return out;
}

}  // namespace

TEST_CASE("the deck is the testbench's hier netlist with its control block translated", SOURCE) {
  hefesto::test::SchemSession s;
  hefesto::test::sim::copy_devices(s.folder().path() / "w", {"res.sym", "lab_pin.sym", "gnd.sym", "code.sym"});
  s.folder().write("w/tb.sch", testbench(kControl));
  REQUIRE(s.schematic({"open", s.folder().str("w/tb.sch")}).ok());
  const auto deck = schem::build_deck(s.schem(), s.schem().documents.back().document);
  REQUIRE(deck.ok());
  const std::string& text = deck.value().text;
  CHECK(text.find("\nR1 in GND 1k\n") != std::string::npos);
  CHECK(text.find("\n.GLOBAL GND\n") != std::string::npos);
  CHECK(text.ends_with(".end\n"));
  CHECK(deck.value().control);
  CHECK(text.find("\necho hefesto-capture 0\n") != std::string::npos);
  CHECK(text.find("\necho hefesto-capture 1\n") != std::string::npos);
  CHECK(text.find("quit") == std::string::npos);
  CHECK(text.find("\nwrite ") == std::string::npos);
}

// The hand-off to simulation-netlist-preparation (M9): a validator sees the
// built deck, and its refusal fails the build.
TEST_CASE("the deck goes to the validator of simulation-netlist-preparation", SOURCE) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    hefesto::test::TempFolder folder;
    hefesto::test::sim::copy_devices(folder.path(), {"res.sym", "lab_pin.sym", "gnd.sym", "code.sym"});
    folder.write("tb.sch", testbench(".param x=1"));
    dl::NetlistBench bench(dl::folder_input(folder.path()), {folder.path()});
    auto document = schem::load_document(bench.files, folder.path() / "tb.sch", schem::ReadOptions{});
    REQUIRE(document.ok());
    schem::SymbolCache symbols;
    const schem::TclBridge bridge = bench.tcl.bridge();
    const schem::NetlistEnvironment environment{symbols, bench.rc.search_path, bench.files, &bridge, {}, 4};
    hefesto::compat::CompatSettings settings;
    settings.xschem_compat = compat;
    schem::DeckOptions options;
    std::string seen;
    options.validate = [&seen](const schem::Deck& deck) -> hefesto::diag::Status {
      seen = deck.text;
      return hefesto::diag::Done{};
    };
    const auto deck = schem::build_deck(environment, document.value(), options, settings);
    REQUIRE(deck.ok());
    CHECK(seen == deck.value().text);
    CHECK_FALSE(deck.value().control);
    options.validate = [](const schem::Deck&) -> hefesto::diag::Status {
      return hefesto::diag::make("SIM-DECK-001", "refused by the validator");
    };
    const auto refused = schem::build_deck(environment, document.value(), options, settings);
    REQUIRE_FALSE(refused.ok());
    CHECK(refused.error().code == "SIM-DECK-001");
  }
}

TEST_CASE("a testbench that does not netlist builds no deck", SOURCE) {
  hefesto::test::SchemSession s;
  hefesto::test::sim::copy_devices(s.folder().path() / "w", {"lab_pin.sym"});
  // Two labels on one wire: a short (SCHEM-NET-001), an error of the run.
  s.folder().write("w/short.sch", std::string{kHeader} + "N 0 0 100 0 {}\n"
                                                         "C {lab_pin.sym} 0 0 0 0 {name=l1 lab=A}\n"
                                                         "C {lab_pin.sym} 100 0 0 0 {name=l2 lab=B}\n");
  REQUIRE(s.schematic({"open", s.folder().str("w/short.sch")}).ok());
  const auto deck = schem::build_deck(s.schem(), s.schem().documents.back().document);
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().code == "SCHEM-SIM-004");
}

// Q-SCHEM-53 (approved 2026-11-27, both modes): a testbench whose netlist
// run fails only by the net driver check (an undriven gate, driven in
// SPICE by a source of its code block) still gives its deck, with the
// findings as warnings.
TEST_CASE("Undriven node does not block the deck", SOURCE) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    hefesto::test::TempFolder folder;
    hefesto::test::sim::copy_devices(folder.path(), {"lab_pin.sym", "gnd.sym", "code.sym"});
    folder.write("nfet.sym", std::string{kHeader} +
                                 "K {type=nmos format=\"@name @pinlist nfet\" template=\"name=M1\"}\n"
                                 "B 5 17.5 -32.5 22.5 -27.5 {name=D dir=inout}\n"
                                 "B 5 -22.5 -2.5 -17.5 2.5 {name=G dir=in}\n"
                                 "B 5 17.5 27.5 22.5 32.5 {name=S dir=inout}\n");
    folder.write("tb.sch", std::string{kHeader} +
                               "C {nfet.sym} 100 100 0 0 {name=M1}\n"
                               "C {lab_pin.sym} 80 100 0 0 {name=l1 lab=G}\n"
                               "C {lab_pin.sym} 120 70 0 0 {name=l2 lab=D}\n"
                               "C {gnd.sym} 120 130 0 0 {name=l3 lab=0}\n"
                               "C {code.sym} 300 0 0 0 {name=s1 only_toplevel=false value=\"vg G 0 1\"}\n");
    dl::NetlistBench bench(dl::folder_input(folder.path()), {folder.path()});
    auto document = schem::load_document(bench.files, folder.path() / "tb.sch", schem::ReadOptions{});
    REQUIRE(document.ok());
    schem::SymbolCache symbols;
    const schem::TclBridge bridge = bench.tcl.bridge();
    const schem::NetlistEnvironment environment{symbols, bench.rc.search_path, bench.files, &bridge, {}, 4};
    hefesto::compat::CompatSettings settings;
    settings.xschem_compat = compat;
    const auto netlisted = schem::netlist(environment, document.value(), schem::NetlistOptions{}, settings);
    REQUIRE(netlisted.failed);  // `schematic netlist` still fails (exit status 10)
    CHECK(netlisted.driver_check_only);
    const auto deck = schem::build_deck(environment, document.value(), schem::DeckOptions{}, settings);
    REQUIRE(deck.ok());
    CHECK(deck.value().text.find("\nM1 D G 0 nfet\n") != std::string::npos);
    CHECK(deck.value().text.find("\nvg G 0 1\n") != std::string::npos);
    const auto& d = deck.value().diagnostics;
    const auto undriven = std::find_if(d.begin(), d.end(), [](const auto& w) { return w.code == "SCHEM-NET-019"; });
    REQUIRE(undriven != d.end());
    CHECK(undriven->text == "undriven node: G");
    CHECK(std::none_of(d.begin(), d.end(), [](const auto& w) { return w.severity == hefesto::diag::Severity::error; }));
  }
}

TEST_CASE("Raw file write captured", CONTROL) {
  const auto deck = schem::translate_control(
      "* deck\n.control\nsave all\nop\nwrite dc_lv_nmos.raw\nset appendwrite\n"
      "dc Vds 0 1.2 0.01 Vgs 0.3 1.0 0.1\nwrite dc_lv_nmos.raw\n.endc\n.end\n");
  REQUIRE(deck.ok());
  CHECK(deck.value().text ==
        "* deck\n.control\nsave all\nop\necho hefesto-capture 0\nset appendwrite\n"
        "dc Vds 0 1.2 0.01 Vgs 0.3 1.0 0.1\necho hefesto-capture 1\n.endc\n.end\n");
  // Two plots under one name: the op plot, then the dc plot appended.
  const std::vector<schem::DeckCapture> expected = {{"dc_lv_nmos.raw", {}, false, false},
                                                    {"dc_lv_nmos.raw", {}, false, true}};
  CHECK(deck.value().captures == expected);
}

TEST_CASE("wrdata, named vectors, quit and passed-through lines", CONTROL) {
  const auto deck = schem::translate_control(
      ".CONTROL\n  tran 1n 1u\n  wrdata out.txt v(a) v(b)\n  meas tran t1 when v(a)=0.5\n  plot v(a)\n"
      "  let x = v(a) * 2\n  foreach v 1 2\n  alter R1 $v\n  end\n  quit\n  exit\n.ENDC\nwrite outside.raw\n");
  REQUIRE(deck.ok());
  CHECK(deck.value().text ==
        ".CONTROL\n  tran 1n 1u\n  echo hefesto-capture 0\n  meas tran t1 when v(a)=0.5\n  plot v(a)\n"
        "  let x = v(a) * 2\n  foreach v 1 2\n  alter R1 $v\n  end\n.ENDC\nwrite outside.raw\n");
  REQUIRE(deck.value().captures.size() == 1);
  CHECK(deck.value().captures[0] == schem::DeckCapture{"out.txt", {"v(a)", "v(b)"}, true, false});
}

TEST_CASE("Shell refused", CONTROL) {
  const auto deck = schem::translate_control(".control\nshell rm -f x.raw\n.endc\n");
  REQUIRE_FALSE(deck.ok());
  CHECK(deck.error().code == "SCHEM-SIM-001");
  CHECK(deck.error().text == "Control command 'shell' is not allowed in simulation decks");
}

TEST_CASE("every command that reaches the host is refused", CONTROL) {
  for (const std::string command : {"shell", "cd", "load", "source", "codemodel", "osdi", "pre_osdi"}) {
    INFO(command);
    const auto deck = schem::translate_control(".control\nop\n  " + command + " x\n.endc\n");
    REQUIRE_FALSE(deck.ok());
    CHECK(deck.error().text == "Control command '" + command + "' is not allowed in simulation decks");
  }
}

// Hardening (not named by the spec; both modes): a command word known only at
// run time, or an alias or backquote that runs a refused command, would reach
// it anyway.
TEST_CASE("indirect commands are refused too", CONTROL) {
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"op; shell id", "shell"},     {"alias sh shell", "shell"},       {"$cmd rm x", "$cmd"},
      {"!sh", "!sh"},                {"echo `source x.cir`", "source"}, {"SHELL id", "shell"},
  };
  for (const auto& [line, refused] : cases) {
    INFO(line);
    const auto deck = schem::translate_control(".control\n" + line + "\n.endc\n");
    REQUIRE_FALSE(deck.ok());
    CHECK(deck.error().text == "Control command '" + refused + "' is not allowed in simulation decks");
  }
  // Outside a control block nothing is translated or refused.
  CHECK(schem::translate_control("* shell is a word here\n").ok());
}

// Regression (review of round 5, finding 9): a quoted command word and a `+`
// continuation line passed the check, but ngspice removes the quotes and
// joins the continuation to the line before it.
TEST_CASE("quoted command words and continuation lines are checked as ngspice reads them", CONTROL) {
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"\"shell\" ls", "shell"},
      {"'shell' ls", "shell"},
      {"sh\"ell\" ls", "shell"},
      {"alias s \"shell\"", "shell"},
      {"echo x ;\n+ shell rm -f y", "shell"},
      {"alias s\n+ shell", "shell"},
      {"+ shell rm -f y", "+"},  // right after .control: no control line to join
  };
  for (const auto& [line, refused] : cases) {
    INFO(line);
    const auto deck = schem::translate_control(".control\n" + line + "\n.endc\n");
    REQUIRE_FALSE(deck.ok());
    CHECK(deck.error().text == "Control command '" + refused + "' is not allowed in simulation decks");
  }
  // A harmless continuation is joined; outside .control `+` lines stay.
  const auto deck = schem::translate_control("R1 a b\n+ 1k\n.control\nop\nprint v(a)\n+ v(b)\n.endc\n");
  REQUIRE(deck.ok());
  CHECK(deck.value().text == "R1 a b\n+ 1k\n.control\nop\nprint v(a) v(b)\n.endc\n");
}

// The commands that start other programs reach the host like `shell`
// (review of round 5, finding 10).
TEST_CASE("commands that start programs are refused", CONTROL) {
  for (const std::string command : {"edit", "gnuplot", "hardcopy", "aspice", "rspice"}) {
    INFO(command);
    const auto deck = schem::translate_control(".control\nop\n" + command + " x v(a)\n.endc\n");
    REQUIRE_FALSE(deck.ok());
    CHECK(deck.error().text == "Control command '" + command + "' is not allowed in simulation decks");
  }
}

TEST_CASE("include and lib paths resolve against the netlist folder, the schematic folder and the PDK store",
          CODE_BLOCKS) {
  hefesto::test::TempFolder folder;
  folder.write("net/models.inc", "* models\n");
  folder.write("sch/local.inc", "* local\n");
  folder.write("store/sky130A/libs.tech/combined/sky130.lib.spice", "* lib\n");
  platform::LocalFileSystem fs;
  schem::Deck deck;
  deck.text = ".include models.inc\n.include \"local.inc\"\n.lib /usr/share/pdk/sky130A/libs.tech/combined/"
              "sky130.lib.spice ss\n.lib tt\n.inc missing.inc\n.control\n.include models.inc\n.endc\n";
  schem::DeckOptions options;
  options.netlist_folder = folder.path() / "net";
  options.pdk_paths.push_back({"/usr/share/pdk", folder.path() / "store"});
  schem::resolve_deck_paths(deck, fs, folder.path() / "sch", options);
  CHECK(deck.text == ".include " + folder.str("net/models.inc") + "\n.include " + folder.str("sch/local.inc") +
                         "\n.lib " + folder.str("store/sky130A/libs.tech/combined/sky130.lib.spice") +
                         " ss\n.lib tt\n.inc missing.inc\n.control\n.include models.inc\n.endc\n");
  REQUIRE(deck.diagnostics.size() == 1);
  CHECK(deck.diagnostics[0].code == "SCHEM-SIM-002");
  CHECK(deck.diagnostics[0].text.find("\"missing.inc\"") != std::string::npos);
}

// The deck through SCHEM-NL-07's dc_lv_nmos.spice (the deck SCHEM-SIM-01
// captured): the netlist xschem wrote, with its two `write` lines turned
// into capture points; the relative `.lib` and `.include` (found by ngspice's
// sourcepath at run time, M9) are kept with SCHEM-SIM-002.
TEST_CASE("SCHEM-NL-07: the deck of IHP dc_lv_nmos.sch", SOURCE CONTROL) {
  using hefesto::test::kIhpSg13g2;
  const auto tree = hefesto::test::pinned_xschem(kIhpSg13g2);
  if (!tree) SKIP("ihp-sg13g2 xschem data set not fetched (scripts/fetch_pdk.py)");
  const platform::Path root = tree->parent_path().parent_path().parent_path();
  const platform::Path top = *tree / "sg13g2_tests" / "dc_lv_nmos.sch";
  const auto input =
      schem::pdk_xschemrc_input(root, "ihp-sg13g2", dl::kSharedir, top.parent_path() / ".xschem", top.parent_path());
  dl::NetlistBench bench(input, {root});
  auto document = schem::load_document(bench.files, top, schem::ReadOptions{});
  REQUIRE(document.ok());
  schem::SymbolCache symbols;
  const schem::TclBridge bridge = bench.tcl.bridge();
  const schem::NetlistEnvironment environment{symbols, bench.rc.search_path, bench.files, &bridge, root, 4};
  const auto deck = schem::build_deck(environment, document.value(), schem::DeckOptions{}, {});
  REQUIRE(deck.ok());
  const std::string expected = dl::read_text(dl::kSource / "tests/golden/SCHEM/cases/SCHEM-NL-07/expected/"
                                                           "dc_lv_nmos.spice");
  const auto translated = schem::translate_control(expected);
  REQUIRE(translated.ok());
  const std::string actual = dl::replace_all(deck.value().text, root.generic_string(), "/pdk");
  INFO("--- expected\n" << translated.value().text << "--- deck\n" << actual);
  const auto elements = [](const std::string& text) {
    std::vector<std::string> out;
    for (const std::string& line : dl::lines_of(text)) {
      const std::size_t first = line.find_first_not_of(" \t");
      if (first != std::string::npos && line.compare(first, 2, "**") != 0) out.push_back(line);
    }
    return out;
  };
  CHECK(elements(actual) == elements(translated.value().text));
  CHECK(deck.value().captures == translated.value().captures);
  REQUIRE(deck.value().captures.size() == 2);
  CHECK(deck.value().captures[1].append);
  CHECK(lines_with(actual, "echo hefesto-capture").size() == 2);
  std::size_t unresolved = 0;
  for (const auto& d : deck.value().diagnostics) {
    if (d.code == "SCHEM-SIM-002") ++unresolved;
  }
  CHECK(unresolved == 2);
}
