// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The netlist writer end to end (capability schematic-netlisting "Netlist
// options", "Netlist file structure", "Subcircuit body", "Top-level port
// order", "Subcircuit definitions", "Schematic resolution", "Device models",
// "LVS netlist mode", "Flat netlist", "Errors and warnings", "In-process
// netlisting"; tasks.md 8.3-8.5). The SCHEM-NL golden inputs are netlisted
// with the test symbols of tests/data/schem/net/devices (written from the
// system-library interface table by gen_test_devices.py) and compared with
// the outputs xschem gave in CI, the `schem-paths` mask applied by writing
// the input folder as /work/input.
//
// Owner: NETLIST.

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <algorithm>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/net/netlist.hpp"

namespace schem = hefesto::schem;
namespace platform = hefesto::platform;
using platform::Path;

namespace {

const Path kSource{HEFESTO_SOURCE_DIR};
const Path kDevices = Path{HEFESTO_TEST_DATA_DIR} / "devices";
const Path kCases = kSource / "tests/golden/SCHEM/cases";

hefesto::compat::CompatSettings compat(bool on) {
  hefesto::compat::CompatSettings settings;
  settings.xschem_compat = on;
  return settings;
}

std::string replace_all(std::string text, const std::string& from, const std::string& to) {
  for (std::size_t at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
    text.replace(at, from.size(), to);
  }
  return text;
}

bool has_code(const schem::NetlistResult& r, std::string_view code) {
  return std::any_of(r.diagnostics.begin(), r.diagnostics.end(), [&](const auto& d) { return d.code == code; });
}

std::size_t count_code(const schem::NetlistResult& r, std::string_view code) {
  return static_cast<std::size_t>(
      std::count_if(r.diagnostics.begin(), r.diagnostics.end(), [&](const auto& d) { return d.code == code; }));
}

/// Netlists `top` with the search path `directories` (the test devices
/// first) on `fs`.
schem::NetlistResult run(platform::FileSystem& fs, const Path& top, std::vector<Path> directories,
                         const schem::NetlistOptions& options = {}, bool xschem_compat = true) {
  directories.insert(directories.begin(), kDevices);
  const schem::SearchPath search{directories};
  schem::SymbolCache symbols;
  schem::ReadOptions read;
  read.xschem_compat = xschem_compat;
  auto document = schem::load_document(fs, top, read);
  REQUIRE(document);
  const schem::NetlistEnvironment environment{symbols, search, fs, nullptr, {}, 4};
  return schem::netlist(environment, document.value(), options, compat(xschem_compat));
}

std::string read_file(const Path& path) {
  platform::LocalFileSystem fs;
  auto text = fs.read(path);
  REQUIRE(text);
  return text.value();
}

/// A golden case's input netlisted, with its input folder written as
/// /work/input like the capture.
struct GoldenRun {
  schem::NetlistResult result;
  std::string text;
};

GoldenRun golden(const std::string& id, const std::string& schematic, const schem::NetlistOptions& options = {},
                 bool xschem_compat = true) {
  platform::LocalFileSystem fs;
  const Path input = kCases / id / "input";
  GoldenRun out{run(fs, input / schematic, {input}, options, xschem_compat), {}};
  out.text = replace_all(out.result.text, input.generic_string(), "/work/input");
  return out;
}

std::string expected(const std::string& id, const std::string& file) {
  return read_file(kCases / id / "expected" / file);
}

/// A memory file system holding the test devices and `files`.
struct MemoryProject {
  platform::MemoryFileSystem fs;
  Path folder{"/p"};

  MemoryProject() {
    platform::LocalFileSystem local;
    auto names = local.list(kDevices);
    REQUIRE(names);
    for (const Path& file : names.value()) {
      REQUIRE(fs.write_atomic(kDevices / file.filename(), read_file(kDevices / file.filename())));
    }
  }

  void put(const std::string& name, const std::string& text) { REQUIRE(fs.write_atomic(folder / name, text)); }

  schem::NetlistResult netlist(const std::string& top, const schem::NetlistOptions& options = {},
                               bool xschem_compat = true) {
    return run(fs, folder / top, {folder}, options, xschem_compat);
  }
};

constexpr const char* kHeader = "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n";

std::string symbol(const std::string& properties, const std::vector<std::string>& pins) {
  std::string text = "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {" + properties + "}\nV {}\nS {}\nE {}\n";
  double y = 0;
  for (const std::string& pin : pins) {
    text += "B 5 -42.5 " + std::to_string(y - 2.5) + " -37.5 " + std::to_string(y + 2.5) + " {name=" + pin +
            " dir=inout}\n";
    y += 20;
  }
  return text;
}

/// A two-pin subcircuit symbol `<cell>.sym` and its schematic with iopin
/// ports A and Y joined by a resistor `R1` of value `value`.
void two_pin_cell(MemoryProject& p, const std::string& cell, const std::string& properties, const std::string& value) {
  p.put(cell + ".sym", symbol(properties, {"A", "Y"}));
  p.put(cell + ".sch", std::string{kHeader} +
                           "C {iopin.sym} 0 0 0 0 {name=p1 lab=A}\nN 0 0 0 20 {}\n"
                           "C {res.sym} 0 50 0 0 {name=R1 value=" + value + "}\n"
                           "N 0 80 0 100 {}\nC {iopin.sym} 0 100 0 0 {name=p2 lab=Y}\n");
}

}  // namespace

// --- Golden cases ----------------------------------------------------------

TEST_CASE("SCHEM-NL-01 netlist", "[cap:schematic-netlisting][req:Netlist file structure]") {
  const auto on = golden("SCHEM-NL-01", "SC_Test.sch");
  CHECK(on.text == expected("SCHEM-NL-01", "SC_Test.spice"));
  CHECK_FALSE(on.result.failed);
}

TEST_CASE("Flat testbench", "[cap:schematic-netlisting][req:Netlist file structure]") {
  const auto run = golden("SCHEM-NL-01", "SC_Test.sch");
  CHECK(run.text.starts_with("** sch_path: /work/input/SC_Test.sch\n**.subckt SC_Test\nV1 VIN 0 5\n"));
  CHECK(run.text.ends_with("**** end user architecture code\n**.ends\n.end\n"));
}

TEST_CASE("netlist file structure: the xschem-compat=off line", "[cap:schematic-netlisting][req:Netlist file structure]") {
  const auto off = golden("SCHEM-NL-01", "SC_Test.sch", {}, false);
  std::string want = expected("SCHEM-NL-01", "SC_Test.spice");
  want = replace_all(want, "SC_Test.sch\n", "SC_Test.sch\n** xschem-compat=off\n");
  want = replace_all(want, "\n.param\n", "\n.param m=1\n");  // Q-SCHEM-22 OFF keeps user code
  CHECK(off.text == want);
}

TEST_CASE("SCHEM-NL-11 netlist", "[cap:schematic-netlisting][req:Net naming order]") {
  const auto r = golden("SCHEM-NL-11", "nl11.sch");
  CHECK(r.text == expected("SCHEM-NL-11", "nl11.spice"));
  CHECK(r.result.failed);  // xschem exits with 10: the shorts
  CHECK(has_code(r.result, hefesto::schem::net_codes::kShort));
}

TEST_CASE("SCHEM-NL-12 netlist", "[cap:schematic-netlisting][req:Line breaking]") {
  const auto r = golden("SCHEM-NL-12", "nl12.sch");
  CHECK(r.text == expected("SCHEM-NL-12", "nl12.spice"));
  CHECK_FALSE(r.result.failed);
  // Line breaking does not depend on xschem-compat (Q-SCHEM-20).
  CHECK(golden("SCHEM-NL-12", "nl12.sch", {}, false).text ==
        replace_all(r.text, "nl12.sch\n", "nl12.sch\n** xschem-compat=off\n"));
}

TEST_CASE("SCHEM-NL-13 netlist", "[cap:schematic-netlisting][req:expr evaluation]") {
  const auto r = golden("SCHEM-NL-13", "nl13.sch");
  CHECK(r.text == expected("SCHEM-NL-13", "nl13.spice"));
}

TEST_CASE("SCHEM-NL-14 netlist", "[cap:schematic-netlisting][req:Schematic resolution]") {
  for (const bool mode : {true, false}) {
    INFO("xschem-compat " << mode);
    const auto r = golden("SCHEM-NL-14", "nl14.sch", {}, mode);
    std::string want = expected("SCHEM-NL-14", "nl14.spice");
    if (!mode) {
      want = replace_all(want, "nl14.sch\n", "nl14.sch\n** xschem-compat=off\n");
    }
    CHECK(r.text == want);
    CHECK_FALSE(r.result.failed);
    CHECK(has_code(r.result, hefesto::schem::net_codes::kMissingSchematic));
  }
}

TEST_CASE("Missing subcircuit schematic", "[cap:schematic-netlisting][req:Schematic resolution]") {
  const auto r = golden("SCHEM-NL-14", "nl14.sch");
  CHECK(r.text.find("x1 N1 N2 missing\n") != std::string::npos);
  CHECK(r.text.find("** sch_path: /work/input/missing.sch\n.subckt missing A B\n.ends\n") != std::string::npos);
  const auto& d = r.result.diagnostics;
  CHECK(std::any_of(d.begin(), d.end(), [](const auto& w) { return w.text.find("missing.sch") != std::string::npos; }));
}

TEST_CASE("spice_sym_def, spice_stop and default_schematic=ignore",
          "[cap:schematic-netlisting][req:Schematic resolution]") {
  const auto r = golden("SCHEM-NL-14", "nl14.sch");
  CHECK(r.text.find("** sym_path: /work/input/withdef.sym\n.include x.spice\n\n") != std::string::npos);
  CHECK(r.text.find(".subckt stopped A B\n.ends\n") != std::string::npos);
  CHECK(r.text.find("ignored.sym") == std::string::npos);
  CHECK(r.text.find("x4 N1 N2 ignored\n") != std::string::npos);
}

TEST_CASE("Instance-based implementation", "[cap:schematic-netlisting][req:Schematic resolution]") {
  const auto r = golden("SCHEM-NL-15", "nl15.sch");
  CHECK(r.text == expected("SCHEM-NL-15", "nl15.spice"));
  const auto off = golden("SCHEM-NL-15", "nl15.sch", {}, false);
  CHECK(off.text == replace_all(r.text, "nl15.sch\n", "nl15.sch\n** xschem-compat=off\n"));
}

TEST_CASE("Same cell name from two libraries", "[cap:schematic-netlisting][req:Subcircuit definitions]") {
  const auto r = golden("SCHEM-NL-16", "nl16.sch");
  CHECK(r.text == expected("SCHEM-NL-16", "nl16.spice"));
  REQUIRE(count_code(r.result, hefesto::schem::net_codes::kSameCellName) == 1);
  CHECK_FALSE(r.result.failed);
}

TEST_CASE("Same cell name from two libraries, xschem-compat OFF",
          "[cap:schematic-netlisting][req:Subcircuit definitions]") {
  const auto r = golden("SCHEM-NL-16", "nl16.sch", {}, false);
  CHECK(r.text.find("x1 A B inv\nx2 A B inv_1\n") != std::string::npos);
  CHECK(r.text.find(".subckt inv A Y\n*.iopin A\n*.iopin Y\nR1 A Y 1k\n.ends\n") != std::string::npos);
  CHECK(r.text.find("** sch_path: /work/input/libB/inv.sch\n.subckt inv_1 A Y\n*.iopin A\n*.iopin Y\nR1 A Y 2k\n") !=
        std::string::npos);
  REQUIRE(count_code(r.result, hefesto::schem::net_codes::kSameCellName) == 1);
  const auto& d = r.result.diagnostics;
  CHECK(std::any_of(d.begin(), d.end(), [](const auto& w) {
    return w.text.find("libA/inv.sym") != std::string::npos && w.text.find("libB/inv.sym") != std::string::npos;
  }));
}

TEST_CASE("Options instance with top_is_subckt", "[cap:schematic-netlisting][req:Netlist options]") {
  CHECK(golden("SCHEM-NL-17", "nl17.sch").text == expected("SCHEM-NL-17", "nl17-options.spice"));
  schem::NetlistOptions upper;
  upper.uppercase_subckt = true;
  CHECK(golden("SCHEM-NL-17", "nl17.sch", upper).text == expected("SCHEM-NL-17", "nl17-uppercase.spice"));
}

TEST_CASE("SCHEM-NL-18 netlist", "[cap:schematic-netlisting][req:Device models]") {
  const auto r = golden("SCHEM-NL-18", "nl18.sch");
  CHECK(r.text == expected("SCHEM-NL-18", "nl18.spice"));
}

TEST_CASE("SCHEM-NL-19 netlist", "[cap:schematic-netlisting][req:Bus taps]") {
  const auto r = golden("SCHEM-NL-19", "nl19.sch");
  CHECK(r.text == expected("SCHEM-NL-19", "nl19.spice"));
}

// Regression (SCHEM-NL-02, NL-04 and NL-06 replay, M8 round 5): `@#n:net_name`
// gives the net as named: a bus net is not expanded (`Z[1:0]`, not
// `Z[1],Z[0]`) and an automatic net keeps its `#`, in both modes.
TEST_CASE("net_name of a bus net and of an automatic net", "[cap:schematic-netlisting][req:Format string substitution]") {
  MemoryProject p;
  p.put("t.sch", std::string{kHeader} +
                     "N 0 0 100 0 {}\nC {lab_pin.sym} 0 0 0 0 {name=l1 lab=Z[1:0]}\n"
                     "C {bus_tap.sym} 100 0 0 0 {name=l2 lab=[0]}\nN 110 -10 200 -10 {}\n"
                     "C {res.sym} 200 20 0 0 {name=R1 value=1}\n"
                     "N 300 0 500 0 {}\nC {noconn.sym} 300 0 0 0 {name=l3}\n"
                     "C {res.sym} 500 30 0 0 {name=R2 value=1}\n");
  for (const bool on : {true, false}) {
    CAPTURE(on);
    const auto r = p.netlist("t.sch", {}, on);
    CHECK(r.text.find("* tap: Z[1:0] --> Z[0]\n") != std::string::npos);
    CHECK(r.text.find("* noconn #net") != std::string::npos);
  }
}

// Q-SCHEM-35 (approved 2026-11-25): a zero count is a syntax error and the
// name is used unchanged, the same in both modes (here a top-level port of
// a subcircuit netlist, whose names are bus-expanded with a warning).
TEST_CASE("Zero and negative counts are syntax errors: the netlist run in both modes",
          "[cap:schematic-netlisting][req:Bus and vector notation]") {
  MemoryProject p;
  p.put("t.sch", std::string{kHeader} +
                     "C {iopin.sym} 0 20 0 0 {name=p1 lab=a*0}\nC {res.sym} 0 50 0 0 {name=R1 value=1}\n"
                     "C {lab_pin.sym} 0 80 0 0 {name=l2 lab=0}\n");
  schem::NetlistOptions options;
  options.top_is_subckt = true;
  for (const bool on : {true, false}) {
    CAPTURE(on);
    const auto r = p.netlist("t.sch", options, on);
    INFO(r.text);
    CHECK(r.text.find(".subckt t a*0\n") != std::string::npos);
    CHECK(r.text.find("\nR1 a*0 0 1\n") != std::string::npos);
    REQUIRE(has_code(r, hefesto::schem::net_codes::kBusSyntax));
    for (const auto& d : r.diagnostics) {
      if (d.code == hefesto::schem::net_codes::kBusSyntax) CHECK(d.text == "syntax error in a*0");
    }
  }
}

// Q-SCHEM-37 (approved 2026-11-25), the same in both modes: a tap whose
// pin 0 label matches the computed tap name is not a short; one whose label
// differs is reported once, computed name first.
TEST_CASE("Tap side already named: one short in both modes", "[cap:schematic-netlisting][req:Bus taps]") {
  MemoryProject p;
  p.put("t.sch", std::string{kHeader} +
                     "N 110 -10 150 -10 {}\nC {lab_pin.sym} 150 -10 0 0 {name=l1 lab=DATA[3]}\n"
                     "N 210 -10 250 -10 {}\nC {lab_pin.sym} 250 -10 0 0 {name=l2 lab=X}\n"
                     "N 0 0 300 0 {}\nC {lab_pin.sym} 0 0 0 0 {name=l3 lab=DATA[3:0]}\n"
                     "C {bus_tap.sym} 100 0 0 0 {name=l4 lab=[3]}\nC {bus_tap.sym} 200 0 0 0 {name=l5 lab=[2]}\n");
  for (const bool on : {true, false}) {
    CAPTURE(on);
    const auto r = p.netlist("t.sch", {}, on);
    CHECK(r.text.find("* tap: DATA[3:0] --> DATA[3]\n") != std::string::npos);
    CHECK(r.text.find("* tap: DATA[3:0] --> X\n") != std::string::npos);
    REQUIRE(count_code(r, hefesto::schem::net_codes::kShort) == 1);
    for (const auto& d : r.diagnostics) {
      if (d.code == hefesto::schem::net_codes::kShort) CHECK(d.text == "Bus tap shorted: DATA[2] - X");
    }
    CHECK(r.failed);
  }
}

TEST_CASE("SCHEM-NL-20 netlists", "[cap:schematic-netlisting][req:Top-level port order]") {
  schem::NetlistOptions options;
  options.top_is_subckt = true;
  const auto with = golden("SCHEM-NL-20", "with/inv.sch", options);
  CHECK(with.text == expected("SCHEM-NL-20", "inv-with-symbol.spice"));
  CHECK(with.result.failed);  // the symbol's pin directions differ from the ports (xschem exit 10)
  CHECK(has_code(with.result, hefesto::schem::net_codes::kPinMismatch));
  const auto without = golden("SCHEM-NL-20", "without/inv.sch", options);
  CHECK(without.text == expected("SCHEM-NL-20", "inv-without-symbol.spice"));
  CHECK_FALSE(without.result.failed);
}

// --- Spec scenarios on small projects ---------------------------------------

TEST_CASE("Port comments first", "[cap:schematic-netlisting][req:Subcircuit body]") {
  MemoryProject p;
  p.put("t.sch", std::string{kHeader} +
                     "C {res.sym} 0 30 0 0 {name=R1 value=1k}\nC {ipin.sym} 0 0 0 0 {name=p1 lab=A}\n"
                     "C {opin.sym} 0 60 0 0 {name=p2 lab=Y}\n");
  const auto r = p.netlist("t.sch");
  CHECK(r.text == "** sch_path: /p/t.sch\n**.subckt t A Y\n*.ipin A\n*.opin Y\nR1 A Y 1k\n**.ends\n.end\n");
}

TEST_CASE("Options instance enables LVS mode", "[cap:schematic-netlisting][req:Netlist options]") {
  MemoryProject p;
  p.put("t.sch", std::string{kHeader} +
                     "C {netlist_options.sym} 300 0 0 0 {name=o1 lvs_netlist=true}\n"
                     "C {ipin.sym} 0 0 0 0 {name=p1 lab=A}\nC {res.sym} 0 30 0 0 {name=R1 value=1k}\n"
                     "C {opin.sym} 0 60 0 0 {name=p2 lab=Y}\n");
  const auto r = p.netlist("t.sch");
  CHECK(r.text == "** sch_path: /p/t.sch\n.subckt t A Y\n*.PININFO A:I Y:O\nR1 A Y 1k\n.ends\n");
}

TEST_CASE("Port order from symbol", "[cap:schematic-netlisting][req:Top-level port order]") {
  MemoryProject p;
  p.put("inv.sym", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\n"
                   "K {type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1\"}\nV {}\nS {}\nE {}\n"
                   "B 5 -42.5 -2.5 -37.5 2.5 {name=A dir=in}\nB 5 -42.5 17.5 -37.5 22.5 {name=Y dir=out}\n");
  p.put("inv.sch", std::string{kHeader} +
                       "C {opin.sym} 0 0 0 0 {name=p1 lab=Y}\nC {res.sym} 0 30 0 0 {name=R1 value=1k}\n"
                       "C {ipin.sym} 0 60 0 0 {name=p2 lab=A}\n");
  schem::NetlistOptions options;
  options.top_is_subckt = true;
  const auto r = p.netlist("inv.sch", options);
  CHECK(r.text.find(".subckt inv A Y\n") != std::string::npos);
  CHECK_FALSE(r.failed);  // names and directions match
}

TEST_CASE("top-level port order: sim_pinnumber sorts stably and warns on duplicates (Q-SCHEM-5)",
          "[cap:schematic-netlisting][req:Top-level port order]") {
  MemoryProject p;
  p.put("t.sch", std::string{kHeader} +
                     "C {ipin.sym} 0 0 0 0 {name=p1 lab=C sim_pinnumber=2}\n"
                     "C {ipin.sym} 100 0 0 0 {name=p2 lab=B sim_pinnumber=1}\n"
                     "C {ipin.sym} 200 0 0 0 {name=p3 lab=A sim_pinnumber=2}\n");
  for (const bool mode : {true, false}) {
    const auto r = p.netlist("t.sch", {}, mode);
    CHECK(r.text.find("subckt t B C A\n") != std::string::npos);
    CHECK(count_code(r, hefesto::schem::net_codes::kDuplicatePortNumber) == 1);
  }
}

TEST_CASE("Parametric subcircuit header", "[cap:schematic-netlisting][req:Subcircuit definitions]") {
  MemoryProject p;
  p.put("comp_ngspice.sym",
        "v {xschem version=3.4.8RC file_version=1.3}\nG {}\n"
        "K {type=subcircuit\nformat=\"@name @pinlist @symname OFFSET=@OFFSET\"\n"
        "template=\"name=x1 OFFSET=0 AMPLITUDE=5\"}\nV {}\nS {}\nE {}\n"
        "B 5 -42.5 -2.5 -37.5 2.5 {name=PLUS dir=in}\nB 5 -42.5 17.5 -37.5 22.5 {name=OUT dir=out}\n"
        "B 5 -42.5 37.5 -37.5 42.5 {name=MINUS dir=in}\n");
  p.put("comp_ngspice.sch", std::string{kHeader} +
                                "C {ipin.sym} 0 0 0 0 {name=p1 lab=PLUS}\nC {opin.sym} 0 100 0 0 {name=p2 lab=OUT}\n"
                                "C {ipin.sym} 0 200 0 0 {name=p3 lab=MINUS}\n");
  p.put("t.sch", std::string{kHeader} + "C {comp_ngspice.sym} 0 0 0 0 {name=x1}\n");
  const auto r = p.netlist("t.sch");
  CHECK(r.text.find("\n.subckt comp_ngspice PLUS OUT MINUS  OFFSET=0 AMPLITUDE=5\n") != std::string::npos);
}

TEST_CASE("extra key matched as a substring", "[cap:schematic-netlisting][req:Subcircuit definitions]") {
  MemoryProject p;
  two_pin_cell(p, "cell",
               "type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1 VPW=1 L=1\"\n"
               "extra=\"VPWR VGND\"",
               "1k");
  p.put("t.sch", std::string{kHeader} + "C {cell.sym} 0 0 0 0 {name=x1}\n");
  const auto on = p.netlist("t.sch", {}, true);
  // The separators around removed keys are kept.
  CHECK(on.text.find("\n.subckt cell A Y   L=1\n") != std::string::npos);
  const auto off = p.netlist("t.sch", {}, false);
  CHECK(off.text.find("\n.subckt cell A Y  VPW=1 L=1\n") != std::string::npos);
  for (const auto* r : {&on, &off}) {
    REQUIRE(count_code(*r, hefesto::schem::net_codes::kExtraSubstring) == 1);
    CHECK(std::any_of(r->diagnostics.begin(), r->diagnostics.end(),
                      [](const auto& d) { return d.text.find("VPW") != std::string::npos; }));
  }
}

TEST_CASE("Multi-line template, xschem-compat OFF", "[cap:schematic-netlisting][req:Subcircuit definitions]") {
  MemoryProject p;
  two_pin_cell(p, "cell", "type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1\nW=1\"", "1k");
  p.put("t.sch", std::string{kHeader} + "C {cell.sym} 0 0 0 0 {name=x1}\n");
  const auto off = p.netlist("t.sch", {}, false);
  CHECK(off.text.find("\n.subckt cell A Y  W=1\n") != std::string::npos);
  CHECK(count_code(off, hefesto::schem::net_codes::kTemplateLf) == 1);
  const auto on = p.netlist("t.sch", {}, true);
  CHECK(on.text.find("\n.subckt cell A Y \nW=1\n") == std::string::npos);  // trailing blank removed
  CHECK(on.text.find("\n.subckt cell A Y\nW=1\n") != std::string::npos);
  CHECK(count_code(on, hefesto::schem::net_codes::kTemplateLf) == 1);
}

TEST_CASE("Template continuation lines, xschem-compat OFF", "[cap:schematic-netlisting][req:Subcircuit definitions]") {
  for (const std::string last : {"+ M=4", "  + M=4"}) {
    INFO(last);
    MemoryProject p;
    p.put("amp.sym", symbol("type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1 W=1\nL=2\n" +
                                last + "\"",
                            {"IN", "OUT"}));
    p.put("amp.sch", std::string{kHeader} +
                         "C {iopin.sym} 0 0 0 0 {name=p1 lab=IN}\nC {iopin.sym} 0 100 0 0 {name=p2 lab=OUT}\n");
    p.put("t.sch", std::string{kHeader} + "C {amp.sym} 0 0 0 0 {name=x1}\n");
    const bool plus_first = last.front() == '+';
    const auto off = p.netlist("t.sch", {}, false);
    // `+ M=4` is a continuation line, joined by post-processing (one space,
    // then the text after the `+`); `  + M=4` is a line of its own.
    CHECK(off.text.find(plus_first ? "\n.subckt amp IN OUT  W=1 L=2  M=4\n" : "\n.subckt amp IN OUT  W=1 L=2\n+ M=4\n") !=
          std::string::npos);
    CHECK(count_code(off, hefesto::schem::net_codes::kTemplateLf) == 1);
    const auto on = p.netlist("t.sch", {}, true);
    // ON: `L=2` starts an element line of its own, written field by field.
    CHECK(on.text.find(plus_first ? "\n.subckt amp IN OUT  W=1\nL=2 M=4\n" : "\n.subckt amp IN OUT  W=1\nL=2\n+ M=4\n") !=
          std::string::npos);
  }
}

TEST_CASE("Duplicate model printed once", "[cap:schematic-netlisting][req:Device models]") {
  MemoryProject p;
  p.put("diode.sym", symbol("type=diode\nformat=\"@name @pinlist D1N4148\"\ntemplate=\"name=D1\"\n"
                            "device_model=\".model D1N4148 D(Is=2.52n)\"",
                            {"p", "m"}));
  p.put("t.sch", std::string{kHeader} + "C {diode.sym} 0 0 0 0 {name=D1}\nC {diode.sym} 200 0 0 0 {name=D2}\n");
  const auto r = p.netlist("t.sch");
  const std::string block = "**** begin user architecture code\n.model D1N4148 D(Is=2.52n)\n"
                            "**** end user architecture code\n.end\n";
  CHECK(r.text.ends_with(block));
  CHECK(r.text.find(".model D1N4148") == r.text.rfind(".model D1N4148"));
}

TEST_CASE("device models: keys and buckets", "[cap:schematic-netlisting][req:Device models]") {
  CHECK(schem::device_model_key(".MODEL D1N4148 D(Is=2.52n)") == "d1n4148d(is=2.52n)");
  CHECK(schem::device_model_key("* x\n.subckt zz a b\n") == "zza");
  CHECK(schem::device_model_key("one") == "one");
  CHECK(schem::device_model_key("Two Words more") == "twowords");
  CHECK(schem::device_model_bucket("") == 5381U % 367U);
  CHECK(schem::device_model_bucket("a") == (5381U * 33U + 'a') % 367U);
}

TEST_CASE("LVS inverter", "[cap:schematic-netlisting][req:LVS netlist mode]") {
  MemoryProject p;
  p.put("nmos.sym", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\n"
                    "K {type=nmos\nformat=\"@spiceprefix@name @pinlist nfet_01v8 L=@L W=@W ad=1 nrd=1\"\n"
                    "lvs_format=\"@spiceprefix@name @pinlist nfet_01v8 L=@L W=@W\"\n"
                    "template=\"name=M1 L=0.15 W=1 spiceprefix=X\"}\nV {}\nS {}\nE {}\n"
                    "B 5 17.5 -32.5 22.5 -27.5 {name=D dir=inout}\nB 5 -22.5 -2.5 -17.5 2.5 {name=G dir=in}\n"
                    "B 5 17.5 27.5 22.5 32.5 {name=S dir=inout}\nB 5 17.5 -2.5 22.5 2.5 {name=B dir=in}\n");
  p.put("inv.sch", std::string{kHeader} +
                       "C {ipin.sym} -20 0 0 0 {name=p1 lab=A}\nC {opin.sym} 20 -30 0 0 {name=p2 lab=Y}\n"
                       "C {lab_pin.sym} 20 30 0 0 {name=l1 lab=VSS}\nC {lab_pin.sym} 20 0 0 0 {name=l2 lab=VSS}\n"
                       "C {nmos.sym} 0 0 0 0 {name=M1}\n");
  schem::NetlistOptions options;
  options.lvs_netlist = true;
  const auto r = p.netlist("inv.sch", options);
  CHECK(r.text == "** sch_path: /p/inv.sch\n.subckt inv A Y\n*.PININFO A:I Y:O\nXM1 Y A VSS VSS nfet_01v8 L=0.15 W=1\n"
                  ".ends\n");
  CHECK(r.text.find(".end\n") == std::string::npos);
}

// Regression (SCHEM-NL-10 replay, M8 round 5): in LVS mode a level with no
// port writes its `*.PININFO` line as an empty line, as xschem does; it
// wrote nothing. Both modes.
TEST_CASE("LVS netlist of a schematic with no ports", "[cap:schematic-netlisting][req:Subcircuit body]") {
  MemoryProject p;
  p.put("tb.sch", std::string{kHeader} + "N 0 -30 0 -60 {lab=A}\nC {res.sym} 0 0 0 0 {name=R1 value=1}\n"
                                         "N 0 30 0 60 {lab=B}\n");
  schem::NetlistOptions options;
  options.lvs_netlist = true;
  for (const bool on : {true, false}) {
    CAPTURE(on);
    const auto r = p.netlist("tb.sch", options, on);
    CHECK(r.text.find(".subckt tb\n\nR1 ") != std::string::npos);
  }
}

TEST_CASE("Flattened inverter chain", "[cap:schematic-netlisting][req:Flat netlist]") {
  MemoryProject p;
  p.put("dev.sym", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\n"
                   "K {type=resistor\nformat=\"@name @pinlist rmodel w=@W\"\ntemplate=\"name=R1 W=1\"}\nV {}\nS {}\nE {}\n"
                   "B 5 -2.5 -32.5 2.5 -27.5 {name=p dir=inout}\nB 5 -2.5 27.5 2.5 32.5 {name=m dir=inout}\n");
  p.put("inv.sym", symbol("type=subcircuit\nformat=\"@name @pinlist @symname WP=@WP\"\ntemplate=\"name=x1 WP=2\"",
                          {"A", "Y"}));
  p.put("inv.sch", std::string{kHeader} +
                       "C {iopin.sym} 0 -30 0 0 {name=p1 lab=A}\nC {dev.sym} 0 0 0 0 {name=R1 W=WP}\n"
                       "C {lab_pin.sym} 0 30 0 0 {name=l1 lab=mid}\nC {dev.sym} 0 60 0 0 {name=R2 W=3}\n"
                       "C {iopin.sym} 0 90 0 0 {name=p2 lab=Y}\n");
  p.put("t.sch", std::string{kHeader} +
                     "C {inv.sym} 0 0 0 0 {name=x1}\nC {lab_pin.sym} -40 0 0 0 {name=l1 lab=IN}\n"
                     "C {lab_pin.sym} -40 20 0 0 {name=l2 lab=M}\nC {inv.sym} 0 100 0 0 {name=x2 WP=5}\n"
                     "C {lab_pin.sym} -40 100 0 0 {name=l3 lab=M}\nC {lab_pin.sym} -40 120 0 0 {name=l4 lab=OUT}\n");
  schem::NetlistOptions options;
  options.hierarchy = schem::Hierarchy::flat;
  const auto r = p.netlist("t.sch", options);
  CHECK(r.text.find(".subckt") == r.text.find(".subckt t"));  // only the commented top line
  CHECK(r.text.find("R1_x1 IN x1_mid rmodel w=2\n") != std::string::npos);
  CHECK(r.text.find("R2_x1 x1_mid M rmodel w=3\n") != std::string::npos);
  CHECK(r.text.find("R1_x2 M x2_mid rmodel w=5\n") != std::string::npos);
  CHECK(r.text.find("R2_x2 x2_mid OUT rmodel w=3\n") != std::string::npos);
  CHECK(r.text.find("x1 IN M inv") == std::string::npos);
}

TEST_CASE("flat netlist: globals, 0, undefined subcircuits, nesting and quoted values are kept",
          "[cap:schematic-netlisting][req:Flat netlist]") {
  const std::vector<schem::ProcessedLine> lines{
      {"**.subckt top", false},
      {"x1 a b outer", false},
      {"XP a b pdk_model W=1", false},
      {"**.ends", false},
      {"", false},
      {"* expanding   symbol:  outer.sym # of pins=2", false},
      {"** sym_path: /p/outer.sym", false},
      {"** sch_path: /p/outer.sch", false},
      {".subckt outer A B  W_N=0.42", false},
      {"x7 A n inner W_N=W_N", false},
      {"C1 n 0 1f", false},
      {".ends", false},
      {".subckt inner P Q  W_N=1", false},
      {"XM1 P Q VDD 0 nfet W=W_N ad='W_N*0.29'", false},
      {".ends", false},
      {".GLOBAL VDD", false},
      {".end", false}};
  const auto flat = schem::flatten_netlist(lines).lines;
  std::vector<std::string> text;
  for (const auto& l : flat) text.push_back(l.text);
  CHECK(text == std::vector<std::string>{"**.subckt top", "XM1_x1_x7 a x1_n VDD 0 nfet W=0.42 ad='W_N*0.29'",
                                         "C1_x1 x1_n 0 1f", "XP a b pdk_model W=1", "**.ends", "",
                                         ".GLOBAL VDD", ".end"});
}

// SCHEM-NL-21 (M8 round 7): a quoted expression with blanks is one field, so
// neither its words nor the model name after it are taken for nodes and
// prefixed with the instance path (`ad='int((1 x6_+ x6_1)/2) ...'`,
// `x6_sky130_fd_pr__nfet_01v8`).
TEST_CASE("flat netlist: a quoted expression with blanks is one field", "[cap:schematic-netlisting][req:Flat netlist]") {
  const std::vector<schem::ProcessedLine> lines{
      {"**.subckt top", false},
      {"x6 A Z VCC VSS passgate_1 W_N=0.42 L_N=0.35", false},
      {"**.ends", false},
      {".subckt passgate_1 A Z VCCBPIN VSSBPIN  W_N=1 L_N=0.2", false},
      {"XM1 Z GN A VSSBPIN sky130_fd_pr__nfet_01v8 L=L_N W=W_N nf=1 ad='int((1 + 1)/2) * W_N / 1 * 0.29' "
       "pd={2 * W_N} m=1",
       false},
      {".ends", false},
      {".end", false}};
  const auto flat = schem::flatten_netlist(lines).lines;
  REQUIRE(flat.size() == 4);
  CHECK(flat[1].text ==
        "XM1_x6 Z x6_GN A VSS sky130_fd_pr__nfet_01v8 L=0.35 W=0.42 nf=1 ad='int((1 + 1)/2) * W_N / 1 * 0.29' "
        "pd={2 * W_N} m=1");
}

TEST_CASE("flat netlist: a subcircuit that places itself stops at kMaxFlatDepth",
          "[cap:schematic-netlisting][req:Flat netlist]") {
  const std::vector<schem::ProcessedLine> lines{{"**.subckt top", false}, {"x1 a loop", false}, {"**.ends", false},
                                                {".subckt loop P", false}, {"x2 P loop", false}, {"R1 P 0 1", false},
                                                {".ends", false}};
  const auto flat = schem::flatten_netlist(lines).lines;
  const auto resistors = std::count_if(flat.begin(), flat.end(), [](const auto& l) { return l.text.starts_with("R1"); });
  CHECK(resistors == schem::kMaxFlatDepth);
  CHECK(std::any_of(flat.begin(), flat.end(), [](const auto& l) { return l.text.starts_with("x2_"); }));
}

TEST_CASE("Netlist with a short", "[cap:schematic-netlisting][req:Errors and warnings]") {
  MemoryProject p;
  p.put("t.sch", std::string{kHeader} +
                     "N 0 0 100 0 {}\nC {lab_pin.sym} 0 0 0 0 {name=l1 lab=A}\nC {lab_pin.sym} 100 0 0 0 {name=l2 lab=B}\n"
                     "C {res.sym} 50 30 0 0 {name=R1 value=1}\n");
  const auto r = p.netlist("t.sch");
  CHECK(r.failed);
  CHECK(r.text.find("R1 A net1 1\n") != std::string::npos);
  CHECK(has_code(r, hefesto::schem::net_codes::kShort));
}

TEST_CASE("errors and warnings: no name, no type, overlapping instances",
          "[cap:schematic-netlisting][req:Errors and warnings]") {
  MemoryProject p;
  p.put("untyped.sym", symbol("format=\"@name @pinlist\"", {"a"}));
  p.put("t.sch", std::string{kHeader} +
                     "C {untyped.sym} 0 0 0 0 {}\nC {res.sym} 300 0 0 0 {name=R1 value=1}\n"
                     "C {res.sym} 300 0 0 0 {name=R2 value=1}\n");
  const auto r = p.netlist("t.sch");
  CHECK(has_code(r, hefesto::schem::net_codes::kNoType));
  CHECK(has_code(r, hefesto::schem::net_codes::kNoName));
  CHECK(has_code(r, hefesto::schem::net_codes::kOverlapping));
  CHECK_FALSE(r.failed);  // warnings alone do not fail the run
}

TEST_CASE("Top schematic name", "[cap:schematic-netlisting][req:Format string substitution]") {
  MemoryProject p;
  p.put("note.sym", symbol("type=annotation\nformat=\"* top=@topschname\"\ntemplate=\"name=n1\"", {}));
  p.put("inv.sym", symbol("type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1\"", {"A", "Y"}));
  p.put("inv.sch", std::string{kHeader} +
                       "C {iopin.sym} 0 0 0 0 {name=p1 lab=A}\nC {iopin.sym} 0 100 0 0 {name=p2 lab=Y}\n"
                       "C {note.sym} 0 200 0 0 {name=n1}\n");
  p.put("inv_tb.sch", std::string{kHeader} + "C {inv.sym} 0 0 0 0 {name=x1}\n");
  const auto r = p.netlist("inv_tb.sch");
  CHECK(r.text.find(".subckt inv A Y\n*.iopin A\n*.iopin Y\n* top=inv_tb\n.ends\n") != std::string::npos);
}

TEST_CASE("Portable netlist paths", "[cap:schematic-netlisting][req:Netlist file structure]") {
  CHECK(schem::netlist_path("/home/u/proj/tb.sch", true, "/pdks", "/home/u/proj") == "tb.sch");
  CHECK(schem::netlist_path("/pdks/sky130A/libs.tech/xschem/sky130_tests/not.sym", true, "/pdks", "/home/u/proj") ==
        "$PDK_ROOT/sky130A/libs.tech/xschem/sky130_tests/not.sym");
  CHECK(schem::netlist_path("/home/u/lib/a.sym", true, "/pdks", "/home/u/proj") == "../lib/a.sym");
  CHECK(schem::netlist_path("/home/u/proj/tb.sch", false, "/pdks", "/home/u/proj") == "/home/u/proj/tb.sch");
  MemoryProject p;
  p.put("t.sch", std::string{kHeader} + "C {res.sym} 0 0 0 0 {name=R1 value=1}\n");
  schem::NetlistOptions options;
  options.portable_paths = true;
  const auto portable = p.netlist("t.sch", options);
  const auto absolute = p.netlist("t.sch");
  CHECK(portable.text.starts_with("** sch_path: t.sch\n"));
  CHECK(replace_all(absolute.text, "/p/t.sch", "t.sch") == portable.text);
}

TEST_CASE("iPad netlist in memory", "[cap:schematic-netlisting][req:In-process netlisting]") {
  MemoryProject p;
  p.put("t.sch", std::string{kHeader} + "C {res.sym} 0 0 0 0 {name=R1 value=1}\n");
  auto before = p.fs.list("/p");
  REQUIRE(before);
  const std::string saved = p.fs.read("/p/t.sch").value();
  const auto r = p.netlist("t.sch");
  CHECK_FALSE(r.text.empty());
  auto after = p.fs.list("/p");
  REQUIRE(after);
  CHECK(after.value() == before.value());  // no .spice file, nothing written
  CHECK(p.fs.read("/p/t.sch").value() == saved);
}

TEST_CASE("netlisting: computed lab values are returned, not stored (Q-SCHEM-29)",
          "[cap:schematic-netlisting][req:Net naming order]") {
  MemoryProject p;
  p.put("t.sch", std::string{kHeader} +
                     "N 0 0 100 0 {}\nC {lab_pin.sym} 0 0 0 0 {name=l1}\nC {res.sym} 100 30 0 0 {name=R1 value=1}\n");
  const auto r = p.netlist("t.sch");
  CHECK(std::any_of(r.labels.begin(), r.labels.end(), [](const schem::ComputedLabel& l) {
    return l.target == schem::LabelTarget::wire && l.instance == 0 && l.lab == "#net1";
  }));
  CHECK(std::any_of(r.labels.begin(), r.labels.end(), [](const schem::ComputedLabel& l) {
    return l.target == schem::LabelTarget::instance && l.instance == 0 && l.lab == "net1";
  }));
}

// Regression (found while applying the review of PR #80): each `@symname`
// of a `schematic` attribute was replaced in place, shifting the rest of the
// value, so a value of 400,000 of them took about 10^12 byte moves. They
// are now replaced in one pass.
TEST_CASE("schematic resolution: @symname is replaced in one pass",
          "[cap:schematic-netlisting][req:Schematic resolution]") {
  MemoryProject p;
  std::string value;
  for (int i = 0; i < 400000; ++i) value += "@symname";
  p.put("cell.sym", symbol("type=subcircuit format=\"@name @pinlist @symname\" template=\"name=x1\" schematic=" + value,
                           {"A", "Y"}));
  p.put("top.sch", std::string{kHeader} + "C {cell.sym} 0 0 0 0 {name=x1}\n");
  const auto start = std::chrono::steady_clock::now();
  const auto r = p.netlist("top.sch");
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(r.text.find(".subckt cell A Y\n") != std::string::npos);
  CHECK(r.text.find("cellcellcellcell") != std::string::npos);
  CHECK(elapsed < std::chrono::seconds{10});
}

// --- Round 3: output limits (Q-SCHEM-42, Q-SCHEM-41) and linear headers ------

namespace {

/// Netlists `top` of `p` with the bus budget and flat limits given
/// (NetlistEnvironment fields that tests lower).
schem::NetlistResult netlist_with(MemoryProject& p, const std::string& top, const schem::NetlistOptions& options,
                                  bool xschem_compat, std::uint64_t bus_budget, schem::FlatLimits flat = {}) {
  const schem::SearchPath search{std::vector<Path>{kDevices, p.folder}};
  schem::SymbolCache symbols;
  schem::ReadOptions read;
  read.xschem_compat = xschem_compat;
  auto document = schem::load_document(p.fs, p.folder / top, read);
  REQUIRE(document);
  schem::NetlistEnvironment environment{symbols, search, p.fs, nullptr, {}, 4};
  environment.bus_budget = bus_budget;
  environment.flat_limits = flat;
  return schem::netlist(environment, document.value(), options, compat(xschem_compat));
}

std::vector<const hefesto::diag::Diagnostic*> with_code(const schem::NetlistResult& r, std::string_view code) {
  std::vector<const hefesto::diag::Diagnostic*> out;
  for (const auto& d : r.diagnostics) {
    if (d.code == code) out.push_back(&d);
  }
  return out;
}

schem::NetlistOptions flat_options() {
  schem::NetlistOptions options;
  options.hierarchy = schem::Hierarchy::flat;
  return options;
}

std::size_t occurrences(const std::string& text, const std::string& what) {
  std::size_t n = 0;
  for (std::size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) ++n;
  return n;
}

}  // namespace

// Q-SCHEM-42 (user decision M8-D1, 2026-11-22), both modes: a subcircuit
// whose schematic places itself expands 64 levels deep, then the instance is
// kept as an X line with one SCHEM-NET-017 warning; the run does not fail.
TEST_CASE("Flat netlist deeper than 64 levels", "[cap:schematic-netlisting][req:Flat netlist]") {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    MemoryProject p;
    p.put("loop.sym", symbol("type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1\"", {"A"}));
    p.put("loop.sch", std::string{kHeader} +
                          "C {iopin.sym} 0 0 0 0 {name=p1 lab=A}\nC {loop.sym} 100 0 0 0 {name=x1}\n"
                          "C {lab_pin.sym} 60 0 0 0 {name=l1 lab=A}\nC {res.sym} 0 100 0 0 {name=R1 value=1}\n"
                          "C {lab_pin.sym} 0 70 0 0 {name=l2 lab=A}\nC {lab_pin.sym} 0 130 0 0 {name=l3 lab=0}\n");
    p.put("t.sch", std::string{kHeader} + "C {loop.sym} 0 0 0 0 {name=x1}\nC {lab_pin.sym} -40 0 0 0 {name=l1 lab=IN}\n");
    const auto r = netlist_with(p, "t.sch", flat_options(), on, schem::kMaxBusTotalBytes);
    CHECK_FALSE(r.failed);
    const auto stopped = with_code(r, schem::net_codes::kFlatStopped);
    REQUIRE(stopped.size() == 1);
    CHECK(stopped.front()->text == "flat netlist stopped: depth limit of 64 levels");
    CHECK(occurrences(r.text, "\nR1_") == static_cast<std::size_t>(schem::kMaxFlatDepth));
    CHECK(r.text.ends_with(".end\n"));  // what follows the kept instance is written
  }
}

// Q-SCHEM-42, both modes: ten instances per level, nine levels (10^9
// resistors unflattened). Substitution stops before the line that would pass
// the byte limit, with one SCHEM-NET-017 warning; the instances not yet
// substituted are written as their X lines and the rest of the netlist down
// to `.end` is kept (regression: everything after the stop was dropped, so
// the netlist had no `.end`). It finishes quickly without allocating far
// past the limit. The limit is lowered through
// NetlistEnvironment::flat_limits; the default is checked against the
// decided 268435456 bytes.
TEST_CASE("Flat netlist larger than 256 MiB", "[cap:schematic-netlisting][req:Flat netlist]") {
  CHECK(schem::kMaxFlatBytes == 268435456U);
  CHECK(schem::FlatLimits{}.bytes == schem::kMaxFlatBytes);
  CHECK(schem::FlatLimits{}.depth == schem::kMaxFlatDepth);
  constexpr std::size_t kLimit = std::size_t{1} << 20;
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    MemoryProject p;
    for (int level = 0; level < 9; ++level) {
      const std::string cell = "c" + std::to_string(level);
      p.put(cell + ".sym", symbol("type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1\"", {}));
      std::string body = kHeader;
      for (int k = 0; k < 10; ++k) {
        const std::string at = std::to_string(200 * k) + " 0 0 0 {name=";
        body += level < 8 ? "C {c" + std::to_string(level + 1) + ".sym} " + at + "x" + std::to_string(k) + "}\n"
                          : "C {res.sym} " + at + "R" + std::to_string(k) + " value=1}\n";
      }
      p.put(cell + ".sch", body);
    }
    p.put("t.sch", std::string{kHeader} + "C {c0.sym} 0 0 0 0 {name=x1}\n");
    const auto start = std::chrono::steady_clock::now();
    const auto r =
        netlist_with(p, "t.sch", flat_options(), on, schem::kMaxBusTotalBytes, {schem::kMaxFlatDepth, kLimit});
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK_FALSE(r.failed);
    const auto stopped = with_code(r, schem::net_codes::kFlatStopped);
    REQUIRE(stopped.size() == 1);
    CHECK(stopped.front()->text == "flat netlist stopped: output passed 1048576 bytes");
    CHECK(r.text.size() <= kLimit + 4096);  // the kept X lines and line breaking add a little
    CHECK(r.text.size() > kLimit / 4);
    CHECK(r.text.find("\nR0_x1_x0_x0_x0_x0_x0_x0_x0_x0 ") != std::string::npos);
    CHECK(r.text.find("\nx9_x1 c1\n") != std::string::npos);  // a level-1 instance not substituted
    CHECK(r.text.find("\n**.ends\n") != std::string::npos);
    CHECK(r.text.ends_with("\n.end\n"));
    CHECK(elapsed < std::chrono::seconds{20});
  }
}

TEST_CASE("flat netlist: the byte limit counts expanded instances, so empty subcircuits stop too",
          "[cap:schematic-netlisting][req:Flat netlist]") {
  std::vector<schem::ProcessedLine> lines{{"**.subckt top", false}, {"x1 c0", false}, {"**.ends", false}};
  for (int level = 0; level < 12; ++level) {
    lines.push_back({".subckt c" + std::to_string(level), false});
    for (int k = 0; k < 10 && level < 11; ++k) {
      lines.push_back({"x" + std::to_string(k) + " c" + std::to_string(level + 1), false});
    }
    lines.push_back({".ends", false});
  }
  const auto start = std::chrono::steady_clock::now();
  const auto flat = schem::flatten_netlist(lines, {schem::kMaxFlatDepth, 4096});
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{10});
  REQUIRE(flat.warnings.size() == 1);
  CHECK(flat.warnings.front().code == schem::net_codes::kFlatStopped);
  CHECK(flat.lines.front().text == "**.subckt top");
}

// Q-SCHEM-42 regression (review of PR #88): the byte limit counted only the
// text and LF of each kept line, so short lines (a 1-byte comment counted 2)
// kept about 16 times the limit in line records. Each kept line now also
// counts kFlatLineOverhead.
TEST_CASE("flat netlist: the byte limit bounds the kept line records", "[cap:schematic-netlisting][req:Flat netlist]") {
  constexpr std::size_t kLimit = 4096;
  std::vector<schem::ProcessedLine> lines{{"**.subckt top", false}};
  for (int k = 0; k < 1000; ++k) lines.push_back({"x" + std::to_string(k) + " c", false});
  lines.push_back({"**.ends", false});
  lines.push_back({".subckt c", false});
  for (int k = 0; k < 100; ++k) lines.push_back({"*", false});
  lines.push_back({".ends", false});
  lines.push_back({".end", false});
  const auto flat = schem::flatten_netlist(lines, {schem::kMaxFlatDepth, kLimit});
  REQUIRE(flat.warnings.size() == 1);
  const auto comments = static_cast<std::size_t>(
      std::count_if(flat.lines.begin(), flat.lines.end(), [](const auto& l) { return l.text == "*"; }));
  // The limit's worth, plus the rest of the one body open when it stopped.
  CHECK(comments <= kLimit / (schem::kFlatLineOverhead + 2) + 100);
  CHECK(comments > 0);
  CHECK(flat.lines.back().text == ".end");
  CHECK(flat.lines[flat.lines.size() - 3].text == "x999 c");  // later instances are kept as X lines
}

// Q-SCHEM-41 (user decision M8-D3, 2026-11-22), both modes: the expanded
// bus text of one netlist run is capped in total; past the cap a name is
// used unchanged with SCHEM-NET-006, like one name over kMaxBusBytes. The
// budget is lowered through NetlistEnvironment::bus_budget (default
// 64 x kMaxBusBytes).
TEST_CASE("Total bus expansion limit in a netlist run", "[cap:schematic-netlisting][req:Bus and vector notation]") {
  const std::string total = "exceeds the total of 4294967296 bytes of expanded bus text per netlist run or session";
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    MemoryProject p;
    p.put("t.sch", std::string{kHeader} + "C {ipin.sym} 0 0 0 0 {name=p1 lab=A[3:0]}\n"
                                          "C {ipin.sym} 0 100 0 0 {name=p2 lab=B[3:0]}\n");
    const auto full = netlist_with(p, "t.sch", {}, on, schem::kMaxBusTotalBytes);
    CHECK(full.text.find("B[3],B[2],B[1],B[0]") != std::string::npos);
    CHECK(full.text.find("B[3:0]") == std::string::npos);
    CHECK(with_code(full, schem::net_codes::kSizeLimit).empty());

    const auto capped = netlist_with(p, "t.sch", {}, on, 30);
    CHECK_FALSE(capped.failed);
    CHECK(capped.text.find("A[3],A[2],A[1],A[0]") != std::string::npos);  // 19 bytes fit
    CHECK(capped.text.find("B[3:0]") != std::string::npos);               // the next 19 do not
    const auto limited = with_code(capped, schem::net_codes::kSizeLimit);
    REQUIRE_FALSE(limited.empty());
    CHECK(limited.front()->text == "bus expansion of B[3:0] " + total + "; used unchanged");
  }
  schem::SymbolCache symbols;
  const schem::SearchPath search;
  platform::MemoryFileSystem fs;
  CHECK(schem::NetlistEnvironment{symbols, search, fs, nullptr, {}, 4}.bus_budget == schem::kMaxBusTotalBytes);
}

// Regression (round-2 review): the `.subckt` header template looked every
// key up in `extra` with a find() and a scan of its word list, so 20,000
// template keys against an `extra` of 20,000 words took about a minute.
// Both lookups now use structures built once per definition, and a key found
// only inside a word is reported once with `extra` cut to 80 bytes.
TEST_CASE("the .subckt header template is built in linear time",
          "[cap:schematic-netlisting][req:Subcircuit definitions]") {
  constexpr int kKeys = 20000;
  std::string templ = "name=x1";
  std::string extra;
  for (int i = 0; i < kKeys; ++i) {
    templ += " k" + std::to_string(i) + "=1";
    extra += (i == 0 ? "kz" : " kz") + std::to_string(i);
  }
  extra += " ak1b ak1b";
  MemoryProject p;
  two_pin_cell(p, "cell",
               "type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"" + templ + " k1=2\"\nextra=\"" + extra +
                   " k7\"",
               "1k");
  p.put("t.sch", std::string{kHeader} + "C {cell.sym} 0 0 0 0 {name=x1}\n");
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    const auto start = std::chrono::steady_clock::now();
    const auto r = p.netlist("t.sch", {}, on);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    CHECK(elapsed < std::chrono::seconds{10});
    CHECK(r.text.find(" k19999=1") != std::string::npos);
    CHECK(r.text.find(" k7=1") == std::string::npos);  // a whole word of extra
    CHECK((r.text.find(" k1=1") == std::string::npos) == on);  // inside ak1b only: ON drops it
    const auto inside = with_code(r, hefesto::schem::net_codes::kExtraSubstring);
    REQUIRE(inside.size() == 1);  // k1 twice in the template: one warning
    CHECK(inside.front()->text.find("template key k1 of ") == 0);
    CHECK(inside.front()->text.size() < 200);
  }
}

TEST_CASE("Interrupted netlist run", "[cap:schematic-netlisting][req:In-process netlisting]") {
  MemoryProject p;
  two_pin_cell(p, "cell", "type=subcircuit\nformat=\"@name @pinlist @symname\"\ntemplate=\"name=x1\"", "1k");
  p.put("t.sch", std::string{kHeader} + "C {cell.sym} 0 0 0 0 {name=x1}\n");
  const schem::SearchPath search{std::vector<Path>{kDevices, p.folder}};
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    schem::ReadOptions read;
    read.xschem_compat = on;
    auto document = schem::load_document(p.fs, p.folder / "t.sch", read);
    REQUIRE(document);
    schem::SymbolCache symbols;
    schem::NetlistEnvironment environment{symbols, search, p.fs, nullptr, {}, 4};
    int polls = 0;
    // The top level is placed twice (before and after the options); the
    // interrupt fires before the subcircuit's schematic is placed.
    environment.interrupted = [&polls] { return ++polls > 2; };
    const auto r = schem::netlist(environment, document.value(), schem::NetlistOptions{}, compat(on));
    CHECK(polls > 2);
    CHECK(r.failed);
    CHECK(r.text.empty());
    CHECK(r.labels.empty());
    CHECK(has_code(r, hefesto::schem::net_codes::kInterrupted));

    environment.interrupted = [] { return false; };
    const auto whole = schem::netlist(environment, document.value(), schem::NetlistOptions{}, compat(on));
    CHECK_FALSE(whole.failed);
    CHECK(whole.text.find(".subckt cell A Y") != std::string::npos);
  }
}
