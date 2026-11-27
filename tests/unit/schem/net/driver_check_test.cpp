// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The net driver check of a netlist run and the stderr report of a failed
// run (capability schematic-netlisting "Errors and warnings", paragraphs
// "Net driver check", "Report order of the driver check", "Report text on
// stderr"; golden cases SCHEM-NL-09, NL-10, NL-11; tasks.md 8.5, M8 round
// 7), and the flag the simulation deck reads (Q-SCHEM-53, approved
// 2026-11-27).
//
// Owner: NETLIST.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "hefesto/compat/compat_settings.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/platform/file_system.hpp"
#include "hefesto/schem/file/read_write.hpp"
#include "hefesto/schem/file/save.hpp"
#include "hefesto/schem/lib/search_path.hpp"
#include "hefesto/schem/lib/symbol_cache.hpp"
#include "hefesto/schem/net/codes.hpp"
#include "hefesto/schem/net/netlist.hpp"
#include "hefesto/schem/tcl/tcl_environment.hpp"

namespace schem = hefesto::schem;
namespace platform = hefesto::platform;
namespace codes = hefesto::schem::net_codes;
using platform::Path;

#define DRIVER "[cap:schematic-netlisting][req:Errors and warnings]"

namespace {

const Path kDevices = Path{HEFESTO_TEST_DATA_DIR} / "devices";

constexpr const char* kHeader = "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n";

std::string read_file(const Path& path) {
  platform::LocalFileSystem fs;
  auto text = fs.read(path);
  REQUIRE(text);
  return text.value();
}

/// A symbol with one 5x5 pin box per (name, dir, x, y).
struct PinSpec {
  std::string name;
  std::string dir;
  int x = 0;
  int y = 0;
};

std::string symbol(const std::string& properties, const std::vector<PinSpec>& pins) {
  std::string text = "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {" + properties + "}\nV {}\nS {}\nE {}\n";
  for (const PinSpec& pin : pins) {
    text += "B 5 " + std::to_string(pin.x - 2.5) + " " + std::to_string(pin.y - 2.5) + " " +
            std::to_string(pin.x + 2.5) + " " + std::to_string(pin.y + 2.5) + " {name=" + pin.name +
            (pin.dir.empty() ? std::string{} : " dir=" + pin.dir) + "}\n";
  }
  return text;
}

std::string label(const std::string& name, const std::string& lab, int x, int y) {
  return "C {lab_pin.sym} " + std::to_string(x) + " " + std::to_string(y) + " 0 0 {name=" + name + " lab=" + lab +
         "}\n";
}

/// A memory project with the test devices and an `nfet` like gf180's
/// `nfet_03v3`: D and S `inout`, G and B `in`.
struct Project {
  platform::MemoryFileSystem fs;
  Path folder{"/p"};
  bool open_net_is_error = false;
  bool shorted_output_is_error = false;
  const schem::TclBridge* tcl = nullptr;

  Project() {
    platform::LocalFileSystem local;
    auto names = local.list(kDevices);
    REQUIRE(names);
    for (const Path& file : names.value()) {
      REQUIRE(fs.write_atomic(kDevices / file.filename(), read_file(kDevices / file.filename())));
    }
    put("nfet.sym", symbol("type=nmos format=\"@name @pinlist nfet\" template=\"name=M1\"",
                           {{"D", "inout", 20, -30}, {"G", "in", -20, 0}, {"S", "inout", 20, 30}, {"B", "in", 20, 0}}));
    put("gate.sym", symbol("type=subcircuit format=\"@name @pinlist gate\" template=\"name=x1\"",
                           {{"A", "in", -20, 0}, {"Y", "out", 20, 0}}));
  }

  void put(const std::string& name, const std::string& text) { REQUIRE(fs.write_atomic(folder / name, text)); }

  schem::NetlistResult netlist(const std::string& top, bool xschem_compat) {
    const schem::SearchPath search{{kDevices, folder}};
    schem::SymbolCache symbols;
    schem::ReadOptions read;
    read.xschem_compat = xschem_compat;
    auto document = schem::load_document(fs, folder / top, read);
    REQUIRE(document);
    schem::NetlistEnvironment environment{symbols, search, fs, tcl, {}, 4};
    environment.erc_open_net_is_error = open_net_is_error;
    environment.erc_shorted_output_is_error = shorted_output_is_error;
    hefesto::compat::CompatSettings compat;
    compat.xschem_compat = xschem_compat;
    return schem::netlist(environment, document.value(), schem::NetlistOptions{}, compat);
  }
};

/// The gf180 testbench `tests/test_nfet_03v3.sch` reduced to its driver
/// check: an nfet whose four pins are on nets carried only by labels.
std::string nfet_testbench() {
  return std::string{kHeader} + "C {nfet.sym} 100 100 0 0 {name=M1}\n" + label("l1", "D", 120, 70) +
         label("l2", "G", 80, 100) + label("l3", "S", 120, 130) + label("l4", "B", 120, 100);
}

std::vector<std::string> texts_of(const schem::NetlistResult& r, std::string_view code) {
  std::vector<std::string> out;
  for (const auto& d : r.diagnostics) {
    if (d.code == code) out.push_back(d.text);
  }
  return out;
}

bool has_driver_finding(const schem::NetlistResult& r) {
  return std::any_of(r.diagnostics.begin(), r.diagnostics.end(), [](const auto& d) {
    return d.code == codes::kUndrivenNode || d.code == codes::kOpenNet || d.code == codes::kShortedOutput;
  });
}

}  // namespace

TEST_CASE("Undriven gate and open drain: the report of a failed run, xschem-compat ON", DRIVER) {
  Project p;
  p.put("tb.sch", nfet_testbench());
  const auto r = p.netlist("tb.sch", true);
  CHECK(r.failed);
  CHECK(r.driver_check_only);
  CHECK(r.text.find("\nM1 D G S B nfet\n") != std::string::npos);  // the netlist is still written
  CHECK(texts_of(r, codes::kUndrivenNode) == std::vector<std::string>{"undriven node: B", "undriven node: G"});
  CHECK(texts_of(r, codes::kOpenNet).empty());  // ON: a warning is in the report only
  CHECK(schem::driver_report_text(r) ==
        "-----------/p/tb.sch\nError: undriven node: B\nWarning: open net: D\nError: undriven node: G\n"
        "Warning: open net: S\n-----------/p/tb.sch\n");
}

TEST_CASE("Undriven gate, xschem-compat OFF", DRIVER) {
  Project p;
  p.put("tb.sch", nfet_testbench());
  const auto r = p.netlist("tb.sch", false);
  CHECK(r.failed);
  CHECK(r.driver_check_only);
  CHECK(texts_of(r, codes::kUndrivenNode) == std::vector<std::string>{"undriven node: B", "undriven node: G"});
  CHECK(texts_of(r, codes::kOpenNet) == std::vector<std::string>{"open net: D", "open net: S"});
  for (const auto& d : r.diagnostics) {
    if (d.code == codes::kOpenNet) CHECK(d.severity == hefesto::diag::Severity::warning);
    if (d.code == codes::kUndrivenNode) CHECK(d.severity == hefesto::diag::Severity::error);
  }
}

TEST_CASE("Report order follows xschem's key", DRIVER) {
  CHECK(schem::driver_report_key("#net10") == 2496);
  CHECK(schem::driver_report_key("B") == 19504);
  CHECK(schem::driver_report_key("#net2") == 21787);
  CHECK(schem::driver_report_key("net1") == 27468);
  // Four undriven gates on the nets `net1`, `B`, `Z` and `A1` (keys 27468,
  // 19504, 19528 and 11108): ON reports them by key, OFF by name, which
  // here is the same order.
  Project p;
  std::string tb{kHeader};
  const std::vector<std::string> nets{"net1", "B", "Z", "A1"};
  for (std::size_t i = 0; i < nets.size(); ++i) {
    const int x = 200 * static_cast<int>(i);
    tb += "C {gate.sym} " + std::to_string(x) + " 0 0 0 {name=x" + std::to_string(i) + "}\n" +
          label("l" + std::to_string(i), nets[i], x - 20, 0) + label("o" + std::to_string(i), "out", x + 20, 0);
  }
  p.put("order.sch", tb);
  REQUIRE(schem::driver_report_key("Z") == 19528);
  REQUIRE(schem::driver_report_key("A1") == 11108);
  const auto on = p.netlist("order.sch", true);
  CHECK(texts_of(on, codes::kUndrivenNode) ==
        std::vector<std::string>{"undriven node: A1", "undriven node: B", "undriven node: Z", "undriven node: net1"});
  const auto off = p.netlist("order.sch", false);
  CHECK(texts_of(off, codes::kUndrivenNode) ==
        std::vector<std::string>{"undriven node: A1", "undriven node: B", "undriven node: Z", "undriven node: net1"});
  // `out` carries four `out` pins: a shorted output node, not an error.
  CHECK(texts_of(off, codes::kShortedOutput) == std::vector<std::string>{"shorted output node: out"});
}

TEST_CASE("Report order follows xschem's key: ON and OFF differ", DRIVER) {
  // `Z1` (key 11933) before `B` (19504) by key; after it by name.
  REQUIRE(schem::driver_report_key("Z1") == 11933);
  Project p;
  std::string tb{kHeader};
  const std::vector<std::string> nets{"B", "Z1"};
  for (std::size_t i = 0; i < nets.size(); ++i) {
    const int x = 200 * static_cast<int>(i);
    tb += "C {gate.sym} " + std::to_string(x) + " 0 0 0 {name=x" + std::to_string(i) + "}\n" +
          label("l" + std::to_string(i), nets[i], x - 20, 0) + label("o" + std::to_string(i), "y" + nets[i], x + 20, 0);
  }
  p.put("order.sch", tb);
  CHECK(texts_of(p.netlist("order.sch", true), codes::kUndrivenNode) ==
        std::vector<std::string>{"undriven node: Z1", "undriven node: B"});
  CHECK(texts_of(p.netlist("order.sch", false), codes::kUndrivenNode) ==
        std::vector<std::string>{"undriven node: B", "undriven node: Z1"});
  std::vector<std::string> names{"#net2", "B", "#net10", "net1"};
  std::stable_sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
    return schem::driver_report_key(a) < schem::driver_report_key(b);
  });
  CHECK(names == std::vector<std::string>{"#net10", "B", "#net2", "net1"});
}

TEST_CASE("Open net alone does not fail the run", DRIVER) {
  // A resistor whose two pins (both `inout`) are each alone on their net.
  Project p;
  p.put("r.sch", std::string{kHeader} + "C {res.sym} 0 0 0 0 {name=R1 value=1k}\n" + label("l1", "A", 0, -30) +
                     label("l2", "B", 0, 30));
  const auto on = p.netlist("r.sch", true);
  CHECK_FALSE(on.failed);
  CHECK_FALSE(has_driver_finding(on));  // ON: nothing printed unless the run fails
  CHECK(schem::driver_report_text(on).find("Warning: open net: A\nWarning: open net: B\n") != std::string::npos);
  const auto off = p.netlist("r.sch", false);
  CHECK_FALSE(off.failed);
  CHECK(texts_of(off, codes::kOpenNet) == std::vector<std::string>{"open net: A", "open net: B"});
}

TEST_CASE("Open net made an error", DRIVER) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Project p;
    p.open_net_is_error = true;
    p.put("r.sch", std::string{kHeader} + "C {res.sym} 0 0 0 0 {name=R1 value=1k}\n" + label("l1", "A", 0, -30) +
                       label("l2", "B", 0, 30));
    const auto r = p.netlist("r.sch", compat);
    CHECK(r.failed);
    CHECK(r.driver_check_only);
    CHECK(texts_of(r, codes::kOpenNet) == std::vector<std::string>{"open net: A", "open net: B"});
    CHECK(schem::driver_report_text(r).find("\nWarning: open net: A\n") != std::string::npos);
  }
}

TEST_CASE("Open net made an error by the xschemrc variable", DRIVER) {
  hefesto::editor::EditorState editor;
  platform::MemoryFileSystem files;
  schem::TclEnvironment tcl(editor, hefesto::script::SandboxPolicy{}, files);
  const schem::TclBridge bridge = tcl.bridge();
  for (const char* value : {"1", "yes", "true", "On"}) {
    CAPTURE(value);
    REQUIRE(tcl.interpreter().evaluate(std::string{"set ::erc_open_net_is_error "} + value).ok());
    CHECK(bridge.global_variable("erc_open_net_is_error") == std::optional<std::string>{value});
    Project p;
    p.tcl = &bridge;
    p.put("r.sch", std::string{kHeader} + "C {res.sym} 0 0 0 0 {name=R1 value=1k}\n" + label("l1", "A", 0, -30) +
                       label("l2", "B", 0, 30));
    CHECK(p.netlist("r.sch", true).failed);
    CHECK(p.netlist("r.sch", false).failed);
  }
  REQUIRE(tcl.interpreter().evaluate("set ::erc_open_net_is_error 0").ok());
  Project p;
  p.tcl = &bridge;
  p.put("r.sch", std::string{kHeader} + "C {res.sym} 0 0 0 0 {name=R1 value=1k}\n" + label("l1", "A", 0, -30) +
                     label("l2", "B", 0, 30));
  CHECK_FALSE(p.netlist("r.sch", true).failed);
}

TEST_CASE("Driving port and global net", DRIVER) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Project p;
    // IN: an ipin (pin dir=out) and the gate input; VDD: global, only `in`
    // pins; the gate output goes to an opin and a second gate's input.
    p.put("t.sch", std::string{kHeader} + "C {ipin.sym} -20 0 0 0 {name=p1 lab=IN}\n" +
                       "C {gate.sym} 0 0 0 0 {name=x1}\n" + label("l1", "Y", 20, 0) +
                       "C {gate.sym} 0 200 0 0 {name=x2}\n" + label("l2", "Y", -20, 200) +
                       "C {opin.sym} 20 200 0 0 {name=p2 lab=OUT}\n" +
                       "C {gate.sym} 0 400 0 0 {name=x3}\n" + "C {vdd.sym} -20 400 0 0 {name=l3 lab=VDD}\n" +
                       label("l4", "OUT2", 20, 400) + "C {gate.sym} 0 600 0 0 {name=x4}\n" +
                       label("l5", "OUT2", -20, 600) + "C {gate.sym} 0 800 0 0 {name=x5}\n" +
                       "C {vdd.sym} -20 800 0 0 {name=l6 lab=VDD}\n" + label("l7", "OUT", 20, 800));
    const auto r = p.netlist("t.sch", compat);
    const std::string report = schem::driver_report_text(r);
    INFO(report);
    CHECK(report.find(": IN\n") == std::string::npos);
    CHECK(report.find(": VDD\n") == std::string::npos);
    CHECK_FALSE(r.failed);
  }
}

TEST_CASE("Two outputs on one net", DRIVER) {
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Project p;
    // Two gate outputs and one gate input on N; the inputs are driven by an
    // ipin each.
    p.put("t.sch", std::string{kHeader} + "C {gate.sym} 0 0 0 0 {name=x1}\n" + label("l1", "N", 20, 0) +
                       "C {ipin.sym} -20 0 0 0 {name=p1 lab=I1}\n" + "C {gate.sym} 0 200 0 0 {name=x2}\n" +
                       label("l2", "N", 20, 200) + "C {ipin.sym} -20 200 0 0 {name=p2 lab=I2}\n" +
                       "C {gate.sym} 0 400 0 0 {name=x3}\n" + label("l3", "N", -20, 400) +
                       "C {opin.sym} 20 400 0 0 {name=p3 lab=O}\n");
    const auto r = p.netlist("t.sch", compat);
    CHECK_FALSE(r.failed);
    CHECK(schem::driver_report_text(r).find("\nWarning: shorted output node: N\n") != std::string::npos);
    if (!compat) CHECK(texts_of(r, codes::kShortedOutput) == std::vector<std::string>{"shorted output node: N"});
    p.shorted_output_is_error = true;
    const auto error = p.netlist("t.sch", compat);
    CHECK(error.failed);
    CHECK(error.driver_check_only);
  }
}

TEST_CASE("a short is not a driver finding: no deck from such a run (Q-SCHEM-53)", DRIVER) {
  // Two labels on one wire: SCHEM-NL-11's short, an error the deck stops at.
  for (const bool compat : {true, false}) {
    CAPTURE(compat);
    Project p;
    p.put("t.sch", std::string{kHeader} + "N 0 0 100 0 {}\n" + label("l1", "A", 0, 0) + label("l2", "B", 100, 0) +
                       "C {res.sym} 50 30 0 0 {name=R1 value=1}\n");
    const auto r = p.netlist("t.sch", compat);
    CHECK(r.failed);
    CHECK_FALSE(r.driver_check_only);
    // The short is printed after the last separator; B, the name that lost
    // the short, carries no pin.
    const std::string report = schem::driver_report_text(r);
    CHECK(report.ends_with("-----------/p/t.sch\nError: Net shorted: A - B\n"));
    CHECK(report.find("Error: undriven node: B\n") != std::string::npos);
  }
}

TEST_CASE("the report lists each subcircuit level after the top", DRIVER) {
  Project p;
  p.put("cell.sym", symbol("type=subcircuit format=\"@name @pinlist @symname\" template=\"name=x1\"",
                           {{"A", "inout", -20, 0}}));
  p.put("cell.sch", std::string{kHeader} + "C {iopin.sym} 0 0 0 0 {name=p1 lab=A}\n" +
                        "C {nfet.sym} 100 100 0 0 {name=M1}\n" + label("l1", "A", 120, 70) +
                        label("l2", "A", 80, 100) + label("l3", "A", 120, 130) + label("l4", "Q", 120, 100));
  p.put("top.sch", std::string{kHeader} + "C {cell.sym} 0 0 0 0 {name=x1}\n" + label("l1", "N", -20, 0));
  const auto r = p.netlist("top.sch", true);
  CHECK(r.failed);
  REQUIRE(r.report.size() == 2);
  CHECK(r.report[0].schematic == Path{"/p/top.sch"});
  CHECK(r.report[1].schematic == Path{"/p/cell.sch"});
  CHECK(schem::driver_report_text(r) ==
        "-----------/p/top.sch\nWarning: open net: N\n-----------/p/cell.sch\nError: undriven node: Q\n"
        "-----------/p/top.sch\n");
}
