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

#include "cmd_fixture.hpp"
#include "hefesto/schem/file/read_write.hpp"

namespace schem = hefesto::schem;
using hefesto::test::SchemSession;

namespace {

constexpr const char* kXschem = "[cap:schematic-command][req:xschem compatibility command]";

void open_two_instances(SchemSession& s) {
  s.folder().write("w/top.sch", hefesto::test::kTwoInstances);
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
  CHECK(caught(s, "xschem getprop instance M9 w") == "1 {schematic: no instance \"M9\"}");
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
  // Until NETLIST lands the run fails: xschem returns 1 and raises nothing.
  CHECK(caught(s, "xschem netlist") == "0 1");
}

TEST_CASE("Launcher saves and netlists: xschem save writes only a modified document", kXschem) {
  SchemSession s;
  open_two_instances(s);
  const std::string original = s.folder().read("w/top.sch");
  CHECK(caught(s, "xschem save") == "0 {}");
  CHECK(s.folder().read("w/top.sch") == original);  // not modified: nothing written
  s.schem().documents.back().document.modified = true;
  CHECK(caught(s, "xschem set netlist_type spice; xschem save; xschem netlist") == "0 1");
  CHECK(s.folder().read("w/top.sch") == schem::write_document(s.schem().documents.back().document));
  CHECK_FALSE(s.schem().documents.back().document.modified);
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
  // Until EDIT implements `schematic ascend`, `xschem go_back` reports its
  // CORE-NOTIMPL-007 error, which shows the mapping.
  CHECK(caught(s, "xschem go_back").find("not implemented yet: schematic ascend") != std::string::npos);
  CHECK(caught(s, "xschem unselect_all").find("not implemented yet: schematic select") != std::string::npos);
  CHECK(caught(s, "xschem descend 1 0 R1").find("not implemented yet: schematic descend") != std::string::npos);
  CHECK(caught(s, "xschem change_sch_path 1") == "1 {xschem change_sch_path: no instance descended into}");
}

TEST_CASE("xschem passes hostile arguments to schematic as single words", kXschem) {
  // Arguments reach `::magic::schematic` as list elements: brackets, braces,
  // `$` and `;` are never substituted or split.
  SchemSession s;
  s.folder().write("w/a [set ::pwned 1] $x;.sch", hefesto::test::kTwoInstances);
  CHECK(caught(s, "xschem load {" + s.folder().str("w/a [set ::pwned 1] $x;.sch") + "}") == "0 1");
  CHECK(s.tcl("info exists ::pwned").value.to_string() == "0");
  CHECK(caught(s, "xschem setprop instance R1 value {[set ::pwned 1];}").find("not implemented yet") !=
        std::string::npos);
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
