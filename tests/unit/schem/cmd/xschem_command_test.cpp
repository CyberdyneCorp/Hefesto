// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `xschem` Tcl compatibility command (capability schematic-command
// "xschem compatibility command"; Q-SCHEM-10 inventory test).
//
// Owner: CMD.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include "../sim/sim_support.hpp"
#include "cmd_fixture.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace schem = hefesto::schem;
using hefesto::test::SchemSession;

namespace {

constexpr const char* kXschem = "[cap:schematic-command][req:xschem compatibility command]";

void open_two_instances(SchemSession& s) {
  s.folder().write("w/top.sch", hefesto::test::kTwoInstances);
  hefesto::test::sim::copy_devices(s.folder().path() / "w", {"res.sym"});
  REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
}

// `list [catch {<script>} msg] $msg` as text.
std::string caught(SchemSession& s, const std::string& script) {
  const auto result = s.tcl("list [catch {" + script + "} msg] $msg");
  REQUIRE(result.ok());
  return result.value.to_string();
}

// Every `xschem_subcommands` entry of the committed inventory
// (tests/data/schem/inventory.json, beside this area's data folder).
std::set<std::string> inventoried_subcommands() {
  std::ifstream in(std::filesystem::path{HEFESTO_TEST_DATA_DIR}.parent_path() / "inventory.json");
  std::ostringstream text;
  text << in.rdbuf();
  const std::string json = text.str();
  std::set<std::string> names;
  const std::regex list(R"re("xschem_subcommands"\s*:\s*\[([^\]]*)\])re");
  const std::regex name(R"re("([^"]+)")re");
  for (std::sregex_iterator it(json.begin(), json.end(), list), end; it != end; ++it) {
    const std::string items = (*it)[1];
    for (std::sregex_iterator n(items.begin(), items.end(), name); n != end; ++n) names.insert((*n)[1]);
  }
  return names;
}

}  // namespace

TEST_CASE("Instance property from Tcl", kXschem) {
  SchemSession s;
  open_two_instances(s);
  CHECK(s.tcl("xschem getprop instance M1 w").value.to_string() == "1.0u");
  CHECK(s.tcl("xschem getprop instance 1 model").value.to_string() == "nch");  // by index, as xschem
  CHECK(caught(s, "xschem getprop instance M9 w") == "1 {xschem getprop: instance not found:M9}");
}

// Regression (round 7): a digits-only `<inst>` was looked up as a name
// first, so `getprop instance 3` read the instance named `3`, and a missing
// instance raised `schematic: no instance "<inst>"`.
TEST_CASE("Instance addressed by index", kXschem) {
  SchemSession s;
  s.folder().write("w/top.sch", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n"
                                "C {res.sym} 0 0 0 0 {name=R7 value=4.7k}\n"
                                "C {res.sym} 100 0 0 0 {name=3 value=1}\n"
                                "C {nmos4.sym} 200 0 0 0 {name=M1}\n");
  REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
  CHECK(s.tcl("xschem getprop instance 1 name").value.to_string() == "3");
  CHECK(caught(s, "xschem getprop instance 3 name") == "1 {xschem getprop: instance not found:3}");
  CHECK(caught(s, "xschem getprop instance R9 name") == "1 {xschem getprop: instance not found:R9}");
  CHECK(s.tcl("xschem getprop instance R7 value").value.to_string() == "4.7k");
  CHECK(caught(s, "xschem getprop rect 4 0 name") == "1 {xschem getprop rect: no such box}");
}

namespace {

// A resistor symbol whose template names instances R1, R2, ... in `w/lib`.
void write_res_library(SchemSession& s) {
  s.folder().write("w/lib/res.sym", "v {xschem version=3.4.8RC file_version=1.3}\n"
                                    "K {type=resistor\nformat=\"@name @pinlist @value\"\n"
                                    "template=\"name=R1 value=1k\"}\n"
                                    "B 5 -2.5 -32.5 2.5 -27.5 {name=P dir=inout}\n"
                                    "B 5 -2.5 27.5 2.5 32.5 {name=M dir=inout}\n");
  REQUIRE(s.schematic({"path", "set", s.folder().str("w/lib")}).ok());
}

std::string instance_names(SchemSession& s) {
  return s.tcl("set out {}; for {set i 0} {$i < [xschem get instances]} {incr i} "
               "{ lappend out [xschem getprop instance $i name] }; set out")
      .value.to_string();
}

}  // namespace

TEST_CASE("Place, copy and paste from Tcl", kXschem) {
  SchemSession s;
  write_res_library(s);
  REQUIRE(caught(s, "xschem clear force schematic") == "0 {}");
  CHECK(s.tcl("xschem get current_name").value.to_string() == "untitled.sch");
  CHECK(caught(s, "xschem instance res.sym 0 0 0 0") == "0 {}");
  CHECK(instance_names(s) == "R1");
  CHECK(caught(s, "xschem instance res.sym 100 0 0 0") == "0 {}");
  CHECK(instance_names(s) == "R1 R2");
  CHECK(caught(s, "xschem unselect_all; xschem select instance R1; xschem copy; xschem paste 200 0") == "0 {}");
  CHECK(instance_names(s) == "R1 R2 R3");
  CHECK(caught(s, "xschem instance res.sym 0 200 0 0 {name=x1[3:0] value=1k}") == "0 {}");
  CHECK(instance_names(s) == "R1 R2 R3 {x1[3:0]}");
  CHECK(caught(s, "xschem unselect_all; xschem select instance {x1[3:0]}; xschem copy; xschem paste 200 200") ==
        "0 {}");
  CHECK(instance_names(s) == "R1 R2 R3 {x1[3:0]} {x2[3:0]}");
  CHECK(s.tcl("xschem getprop instance 4 value").value.to_string() == "1k");
  // Without a point, paste and saveas need the editor; wrong counts are usage errors.
  CHECK(caught(s, "xschem paste") == "1 {xschem command 'paste' needs the editor}");
  CHECK(caught(s, "xschem saveas") == "1 {xschem command 'saveas' needs the editor}");
  CHECK(caught(s, "xschem instance res.sym 0 0") == "1 {xschem instance: wrong number of arguments}");
  CHECK(caught(s, "xschem copy x") == "1 {xschem copy: wrong number of arguments}");
  CHECK(caught(s, "xschem clear force schematic x") == "1 {xschem clear: wrong number of arguments}");
}

TEST_CASE("xschem clear force symbol makes an empty untitled symbol at the top", kXschem) {
  SchemSession s;
  open_two_instances(s);
  s.schem().documents.back().document.modified = true;
  REQUIRE(caught(s, "xschem clear force symbol") == "0 {}");
  CHECK(s.schem().documents.size() == 1);
  const schem::Document& doc = s.schem().documents.back().document;
  CHECK(doc.kind == schem::FileKind::symbol);
  CHECK(doc.instances.empty());
  CHECK(doc.path.empty());
  CHECK_FALSE(doc.modified);
  CHECK(s.tcl("xschem get current_name").value.to_string() == "untitled.sym");
  CHECK(s.tcl("xschem get instances").value.to_string() == "0");
  CHECK(s.folder().read("w/top.sch") == hefesto::test::kTwoInstances);  // force: never saved
}

// Scenario "clear without force in a headless run", one case per mode.
TEST_CASE("clear without force in a headless run", kXschem) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << (on ? "ON" : "OFF"));
    SchemSession s;
    s.project().compat.xschem_compat = on;
    s.folder().write("w/a.sch", hefesto::test::kTwoInstances);
    REQUIRE(s.schematic({"open", s.folder().str("w/a.sch")}).ok());
    REQUIRE(caught(s, "xschem setprop instance R1 value 2k").starts_with("0 "));
    if (on) {
      CHECK(caught(s, "xschem clear") == "0 {}");
      CHECK(s.folder().read("w/a.sch").find("name=R1 value=2k") != std::string::npos);
      CHECK(s.tcl("xschem get current_name").value.to_string() == "untitled.sch");
      CHECK(s.tcl("xschem get instances").value.to_string() == "0");
    } else {
      CHECK(caught(s, "xschem clear") == "1 {schematic: unsaved changes}");
      CHECK(s.folder().read("w/a.sch") == hefesto::test::kTwoInstances);
      CHECK(s.tcl("xschem get current_name").value.to_string() == "a.sch");
      CHECK(s.tcl("xschem getprop instance R1 value").value.to_string() == "2k");
    }
  }
}

TEST_CASE("clear without force drops a modified document that was never saved", kXschem) {
  SchemSession s;
  write_res_library(s);
  REQUIRE(caught(s, "xschem clear force; xschem instance res.sym 0 0 0 0") == "0 {}");
  REQUIRE(s.schem().documents.back().document.modified);
  CHECK(caught(s, "xschem clear") == "0 {}");
  CHECK(s.tcl("xschem get instances").value.to_string() == "0");
}

// Scenario "saveas with the symbol word": the kind word changes nothing, the
// document is written as the kind it is (spec row "xschem saveas").
TEST_CASE("saveas with the symbol word", kXschem) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << (on ? "ON" : "OFF"));
    SchemSession s;
    s.project().compat.xschem_compat = on;
    s.folder().write("w/a.sch", hefesto::test::kTwoInstances);
    REQUIRE(s.schematic({"open", s.folder().str("w/a.sch")}).ok());
    CHECK(caught(s, "xschem saveas " + s.folder().str("w/s.sch") + " schematic") == "0 1");
    CHECK(caught(s, "xschem saveas " + s.folder().str("w/y.sch") + " symbol") == "0 1");
    CHECK(s.schem().documents.back().document.kind == schem::FileKind::schematic);
    CHECK(s.folder().read("w/y.sch") == s.folder().read("w/s.sch"));
  }
}

TEST_CASE("Counts and saveas after a malformed file", kXschem) {
  // The SCHEM-RT-06 file: one wire and one instance load.
  SchemSession s;
  s.folder().write("w/bad.sch", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nF {}\nE {}\n"
                                "Z 1 2 {a\nb} 3\n# a comment record\nN 0 0 10\nN 0 10 20 10 {}\n"
                                "L 30 0 0 10 10 {}\nL 4 0 0 10 10 {}\n"
                                "C {res.sym} 0 100 0 0 {name=R1 value=1k}\nN 0 0 10 0 {lab=A\n");
  REQUIRE(caught(s, "xschem load " + s.folder().str("w/bad.sch")) == "0 1");
  std::filesystem::create_directories(s.folder().path() / "w/out");
  CHECK(s.tcl("xschem get wires").value.to_string() == "1");
  CHECK(s.tcl("xschem get instances").value.to_string() == "1");
  CHECK(caught(s, "xschem saveas " + s.folder().str("w/out/bad.sch") + " schematic") == "0 1");
  CHECK(s.folder().read("w/out/bad.sch") == schem::write_document(s.schem().documents.back().document));
  CHECK(s.tcl("xschem get current_name").value.to_string() == "bad.sch");
  CHECK(s.tcl("xschem get schname").value.to_string() == s.folder().str("w/out/bad.sch"));
  // A file that cannot be written gives 0 and raises nothing.
  const hefesto::test::TempFolder outside;
  CHECK(caught(s, "xschem saveas " + outside.str("bad.sch")) == "0 0");
  CHECK_FALSE(std::filesystem::exists(outside.path() / "bad.sch"));
}

// Q-SCHEM-62 (a): the third argument of get_tok, both modes.
TEST_CASE("xschem get_tok keeps or strips the quotes of a value", kXschem) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << (on ? "ON" : "OFF"));
    SchemSession s;
    s.project().compat.xschem_compat = on;
    // p = name=C4 m=1 device="tantalium capacitor" drc="f {@name\}"
    REQUIRE(s.tcl(R"(set p "name=C4 m=1 device=\"tantalium capacitor\" drc=\"f {@name\\}\"")").ok());
    const std::string props = "$p";
    CHECK(s.tcl("xschem get_tok " + props + " device").value.to_string() == "tantalium capacitor");
    CHECK(s.tcl("xschem get_tok " + props + " device 1").value.to_string() == "\"tantalium capacitor\"");
    CHECK(s.tcl("xschem get_tok " + props + " device 2").value.to_string() == "tantalium capacitor");
    CHECK(s.tcl("xschem get_tok " + props + " drc 1").value.to_string() == "\"f {@name\\}\"");
    CHECK(s.tcl("xschem get_tok " + props + " drc 2").value.to_string() == "f {@name}");
    CHECK(s.tcl("xschem get_tok " + props + " m 1").value.to_string() == "1");
    CHECK(s.tcl("xschem get_tok " + props + " m 2").value.to_string() == "1");
    CHECK(s.tcl("xschem get_tok {k} k 1").value.to_string().empty());
    // Any other third argument acts as 0 (spec row "xschem get_tok"); it
    // used to raise `bad argument` for a word that is not an integer.
    CHECK(caught(s, "xschem get_tok " + props + " device x") == "0 {tantalium capacitor}");
    CHECK(caught(s, "xschem get_tok " + props + " device 7") == "0 {tantalium capacitor}");
    CHECK(caught(s, "xschem get_tok " + props + " device 0") == "0 {tantalium capacitor}");
    CHECK(caught(s, "xschem get_tok " + props + " m 1 2") == "1 {xschem get_tok: wrong number of arguments}");
  }
}

namespace {

// Two labels name one wire A and B: the netlist run reports the short and fails.
void open_shorted(SchemSession& s) {
  s.folder().write("w/lbl.sym", "v {xschem version=3.4.8RC file_version=1.3}\n"
                                "K {type=label format=\"*.alias @lab\" template=\"name=l1 lab=xxx\"}\n"
                                "B 5 -2.5 -2.5 2.5 2.5 {name=p dir=inout}\n");
  s.folder().write("w/top.sch", "v {xschem version=3.4.8RC file_version=1.3}\nG {}\nK {}\nV {}\nS {}\nE {}\n"
                                "N 0 0 100 0 {}\n"
                                "C {lbl.sym} 0 0 0 0 {name=l1 lab=A}\n"
                                "C {lbl.sym} 100 0 0 0 {name=l2 lab=B}\n");
  REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
}

}  // namespace

// Q-SCHEM-61 (a), one case per mode: with xschem-compat ON a failed run
// returns 1, still writes the file and makes the exit status 10; OFF raises.
TEST_CASE("xschem netlist after a run that reports an error", kXschem) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << (on ? "ON" : "OFF"));
    SchemSession s;
    s.project().compat.xschem_compat = on;
    open_shorted(s);
    const std::string result = caught(s, "xschem netlist");
    if (on) {
      CHECK(result == "0 1");
      CHECK(s.folder().read("w/top.spice").find(".end") != std::string::npos);
      CHECK(s.script().exit_status == 10);
      CHECK_FALSE(s.script().quit_requested);  // the script goes on
    } else {
      CHECK(result.starts_with("1 "));
      CHECK(result.find("shorted: A - B") != std::string::npos);
      CHECK_FALSE(std::filesystem::exists(s.folder().path() / "w/top.spice"));
      CHECK(s.script().exit_status == 0);
    }
    // `schematic netlist` raises in both modes.
    CHECK(caught(s, "schematic netlist").starts_with("1 "));
  }
}

TEST_CASE("xschem netlist after a clean run returns 0 and keeps the exit status", kXschem) {
  SchemSession s;
  open_two_instances(s);
  CHECK(caught(s, "xschem netlist") == "0 0");
  CHECK(s.script().exit_status == 0);
}

TEST_CASE("Unsupported subcommand", kXschem) {
  SchemSession s;
  CHECK(caught(s, "xschem fork_sch") == "1 {xschem command 'fork_sch' is not supported}");
  CHECK(caught(s, "xschem get bogus") == "1 {xschem command 'get bogus' is not supported}");
  CHECK(caught(s, "xschem set bogus 1") == "1 {xschem command 'set bogus' is not supported}");
}

TEST_CASE("Non-SPICE netlist type", kXschem) {
  SchemSession s;
  CHECK(caught(s, "xschem set netlist_type verilog") ==
        "1 {xschem set netlist_type: only spice is supported}");
  CHECK(caught(s, "xschem set netlist_type spice") == "0 {}");
}

TEST_CASE("xschem get answers the listed keys", kXschem) {
  SchemSession s;
  open_two_instances(s);
  CHECK(s.tcl("xschem get current_name").value.to_string() == "top.sch");
  CHECK(s.tcl("xschem get schname").value.to_string() == s.folder().str("w/top.sch"));
  CHECK(s.tcl("xschem get netlist_type").value.to_string() == "spice");
  CHECK(s.tcl("xschem get raw_level").value.to_string() == "0");
  CHECK(s.tcl("xschem get topwindow").value.to_string() == ".");
  CHECK(s.tcl("xschem get netlist_dir").value.to_string() == s.folder().str("w"));
  CHECK(s.tcl("xschem get version").value.to_string() == std::string{schem::kWriterVersion});
  CHECK(s.tcl("xschem version").value.to_string() == std::string{schem::kWriterVersion});
}

TEST_CASE("xschem property helpers", kXschem) {
  SchemSession s;
  CHECK(s.tcl("xschem get_tok {name=R1 value=1k} value").value.to_string() == "1k");
  CHECK(s.tcl("xschem get_tok {name=R1} value").value.to_string().empty());
  CHECK(s.tcl("xschem subst_tok {name=R1 value=1k} value 2k").value.to_string() == "name=R1 value=2k");
  CHECK(s.tcl("lindex [xschem expandlabel {A[1:0]}] 0").value.to_string() == "A[1],A[0]");
  CHECK(s.tcl("lindex [xschem expandlabel A\\[1:0\\]] 1").value.to_string() == "2");
}

TEST_CASE("xschem set accepts GUI variables and maps netlist_name onto the next netlist", kXschem) {
  SchemSession s;
  open_two_instances(s);
  CHECK(caught(s, "xschem set cadsnap 10") == "0 {}");
  CHECK(caught(s, "xschem set netlist_name custom.spice") == "0 {}");
  // A successful run returns 0 and writes the file named by netlist_name.
  CHECK(caught(s, "xschem netlist") == "0 0");
  CHECK(s.folder().read("w/custom.spice").find(".end") != std::string::npos);
}

TEST_CASE("Launcher saves and netlists: xschem save writes only a modified document", kXschem) {
  SchemSession s;
  open_two_instances(s);
  const std::string original = s.folder().read("w/top.sch");
  CHECK(caught(s, "xschem save") == "0 {}");
  CHECK(s.folder().read("w/top.sch") == original);  // not modified: nothing written
  s.schem().documents.back().document.modified = true;
  CHECK(caught(s, "xschem set netlist_type spice; xschem save") == "0 {}");
  CHECK(s.folder().read("w/top.sch") == schem::write_document(s.schem().documents.back().document));
  CHECK_FALSE(s.schem().documents.back().document.modified);
  CHECK(caught(s, "xschem netlist") == "0 0");
  CHECK(s.folder().read("w/top.spice").find(".end") != std::string::npos);
}

// Regression: the computed lab values of a netlist run (Q-SCHEM-29) were all
// stored on instances, so a wire's net name landed on the instance with the
// wire's index. They now go to the wire or instance ComputedLabel names.
TEST_CASE("computed lab values are stored on wires, not on the instance with the wire's index", kXschem) {
  SchemSession s;
  open_two_instances(s);
  REQUIRE(s.schematic({"netlist"}).ok());
  const schem::Document& document = s.schem().documents.back().document;
  CHECK(document.instances[0].props == "name=R1 value=1k");
  CHECK(document.wires[0].props.find("lab=") != std::string::npos);
  CHECK_FALSE(document.modified);
}

TEST_CASE("xschem load and load_new_window open documents", kXschem) {
  SchemSession s;
  s.folder().write("w/a.sch", hefesto::test::kTwoInstances);
  REQUIRE(caught(s, "xschem load " + s.folder().str("w/a.sch")) == "0 1");
  CHECK(caught(s, "xschem load_new_window {}") == "0 2");
  CHECK(s.schem().documents.size() == 2);
  CHECK(s.tcl("xschem instance_list").ok());
}

TEST_CASE("xschem instance_list gives name, symbol and type per instance", kXschem) {
  SchemSession s;
  s.folder().write("w/lib/res.sym", "v {xschem version=3.4.8RC file_version=1.3}\nK {type=resistor}\n");
  s.folder().write("w/top.sch", hefesto::test::kTwoInstances);
  REQUIRE(s.schematic({"path", "set", s.folder().str("w/lib")}).ok());
  REQUIRE(s.schematic({"open", s.folder().str("w/top.sch")}).ok());
  CHECK(s.tcl("xschem instance_list").value.to_string() == "{R1 res.sym resistor} {M1 nmos4.sym {}}");
}

TEST_CASE("xschem subcommands of M9 and the editor fail as the table says", kXschem) {
  SchemSession s;
  open_two_instances(s);
  CHECK(s.tcl("xschem raw loaded").value.to_string() == "-1");
  CHECK(caught(s, "xschem raw values x").find("not implemented yet") != std::string::npos);
  CHECK(caught(s, "xschem simulate").find("not implemented yet") != std::string::npos);
  CHECK(caught(s, "xschem place_symbol res.sym") == "1 {xschem command 'place_symbol' needs the editor}");
  CHECK(caught(s, "xschem logic_set 1") == "0 {}");
  CHECK(caught(s, "xschem update_all_sym_bboxes") == "0 {}");
  CHECK(caught(s, "xschem redraw") == "0 {}");
}

TEST_CASE("xschem subcommands that need EDIT pass through schematic", kXschem) {
  SchemSession s;
  open_two_instances(s);
  // The errors of EDIT's `schematic ascend` and `descend` show the mapping.
  CHECK(caught(s, "xschem go_back") == "1 {schematic: already at the top of the hierarchy}");
  CHECK(caught(s, "xschem unselect_all") == "0 {}");
  CHECK(caught(s, "xschem descend 1 0 R1").find("schematic: cannot descend into res.sym") != std::string::npos);
  CHECK(caught(s, "xschem change_sch_path 1") == "1 {xschem change_sch_path: no instance descended into}");
}

TEST_CASE("xschem passes hostile arguments to schematic as single words", kXschem) {
  // Arguments reach `::magic::schematic` as list elements: brackets, braces,
  // `$` and `;` are never substituted or split.
  SchemSession s;
  s.folder().write("w/a [set ::pwned 1] $x;.sch", hefesto::test::kTwoInstances);
  CHECK(caught(s, "xschem load {" + s.folder().str("w/a [set ::pwned 1] $x;.sch") + "}") == "0 1");
  CHECK(s.tcl("info exists ::pwned").value.to_string() == "0");
  CHECK(caught(s, "xschem setprop instance R1 value {[set ::pwned 1];}").starts_with("0 "));
  CHECK(s.tcl("xschem getprop instance R1 value").value.to_string() == "[set ::pwned 1];");
  CHECK(s.tcl("info exists ::pwned").value.to_string() == "0");
}

TEST_CASE("every inventoried xschem subcommand is supported (Q-SCHEM-10)", kXschem) {
  const auto inventoried = inventoried_subcommands();
  REQUIRE(inventoried.size() >= 30);  // the committed inventory of 2026-11-21
  const auto supported = schem::xschem_subcommands();
  const std::set<std::string> table(supported.begin(), supported.end());
  for (const std::string& name : inventoried) {
    INFO(name);
    CHECK(table.count(name) == 1);
  }
  SchemSession s;
  for (const auto name : supported) {
    INFO(name);
    CHECK(caught(s, "xschem " + std::string{name}).find("is not supported") == std::string::npos);
  }
  CHECK(std::is_sorted(supported.begin(), supported.end()));
}

// Review of PR #80, point 6: an `xschemrc` chooses `netlist_dir`, and a
// script chooses `netlist_name`, but `xschem netlist` writes through
// `::magic::schematic netlist` with script origin, so the sandboxed file
// system refuses any path outside the granted folders (security finding
// S-02) and xschem's failure status 1 is returned.
TEST_CASE("xschem netlist writes only inside the granted folders", kXschem) {
  SchemSession s;
  const hefesto::test::TempFolder outside;
  s.folder().write("w/xschemrc", "set netlist_dir {" + outside.str() + "}\n");
  open_two_instances(s);
  REQUIRE(s.tcl("xschem get netlist_dir").value.to_string() == outside.str());
  CHECK(caught(s, "xschem netlist") == "0 1");
  CHECK_FALSE(std::filesystem::exists(outside.path() / "top.spice"));

  s.schem().xschemrc.reset();
  CHECK(caught(s, "xschem set netlist_name {" + outside.str("abs.spice") + "}") == "0 {}");
  CHECK(caught(s, "xschem netlist") == "0 1");
  CHECK_FALSE(std::filesystem::exists(outside.path() / "abs.spice"));
  CHECK(caught(s, "xschem set netlist_name ../../escape.spice") == "0 {}");
  CHECK(caught(s, "xschem netlist") == "0 1");
  CHECK_FALSE(std::filesystem::exists(s.folder().path().parent_path() / "escape.spice"));
  // Inside the granted folder the same command writes.
  CHECK(caught(s, "xschem netlist") == "0 0");
  CHECK(s.folder().read("w/top.spice").find(".end") != std::string::npos);
}
