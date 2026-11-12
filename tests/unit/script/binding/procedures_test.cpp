// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Script-defined procedures in the engine namespace (tasks.md 5.7;
// capability scripting-api "Script-defined procedures in the engine
// namespace"; Q-SCRIPT-41, replicate Tcl semantics, one interpreter per
// open project). The PDK files are excerpts of the pinned PDK scripts in
// tests/data/script/binding/pdk_root/<variant>/libs.tech/magic/.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

#include "binding_fixture.hpp"
#include "hefesto/script/events.hpp"

namespace script = hefesto::script;
using script::test::BindingFixture;

namespace {

std::filesystem::path pdk_file(const char* relative) {
  return std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "pdk_root" / relative;
}

struct RedefinitionLog final : script::ScriptObserver {
  std::vector<script::HelperRedefinition> seen;
  void on_helper_redefined(const script::HelperRedefinition& r) override { seen.push_back(r); }
};

}  // namespace

TEST_CASE("PDK helper callable", "[cap:scripting-api][req:Script-defined procedures in the engine namespace]") {
  BindingFixture f;
  REQUIRE(f.interp->source(pdk_file("sky130A/libs.tech/magic/sky130A.tcl")).ok());
  CHECK(f.value("magic::3digitpastdecimal 0.12345") == "0.123");
  CHECK(f.value("magic::i2u 34") == "0.34");
  // Not imported: the import ran before any startup file.
  CHECK(f.value("info commands 3digitpastdecimal") == "");
}

TEST_CASE("Procedures and variables in the magic namespace",
          "[cap:scripting-api][req:Script-defined procedures in the engine namespace]") {
  BindingFixture f;
  CHECK(f.value("proc magic::twice {x} {expr {2 * $x}}; magic::twice 4") == "8");
  CHECK(f.value("namespace eval magic {proc thrice {x} {expr {3 * $x}}}; magic::thrice 2") == "6");
  CHECK(f.value("info commands thrice") == "");
  CHECK(f.value("set magic::ctot_val 1; set magic::ctot_val") == "1");
  // A later definition replaces an earlier one; a framework procedure too.
  CHECK(f.value("proc magic::twice {x} {expr {20 * $x}}; magic::twice 4") == "80");
  CHECK(f.value("proc magic::gencell_defaults {args} {return mine}; magic::gencell_defaults") == "mine");
}

TEST_CASE("Helper name not reserved", "[cap:scripting-api][req:Script-defined procedures in the engine namespace]") {
  BindingFixture f;
  for (const char* name : {"u2l", "l2u", "i2u", "u2i", "float2spice", "spice2float", "3digitpastdecimal",
                           "searchcellondisk", "iscellnameunique", "query_mylib_ip", "query_my_projects", "magceil",
                           "magfloor"}) {
    INFO(name);
    CHECK(f.value(std::string{"info commands magic::"} + name) == "");
  }
}

TEST_CASE("Cross-PDK helper redefinition", "[cap:scripting-api][req:Script-defined procedures in the engine namespace]") {
  BindingFixture f;
  REQUIRE(f.interp->source(pdk_file("sky130A/libs.tech/magic/sky130A.tcl")).ok());
  CHECK(f.value("magic::i2u 34") == "0.34");
  REQUIRE(f.interp->source(pdk_file("ihp-sg13g2/libs.tech/magic/ihp-sg13g2-util.tcl")).ok());
  CHECK(f.value("magic::i2u 34") == "0.340");  // the sg13g2 definition took effect
}

// bind_registry installs PDKINT's origin hook (HelperOriginTracker) with its
// warnings reported through the dispatcher.
TEST_CASE("Cross-PDK helper redefinition warns", "[cap:scripting-api][req:Script-defined procedures in the engine namespace]") {
  BindingFixture f;
  RedefinitionLog log;
  f.editor.script.events.add(log);
  REQUIRE(f.interp->source(pdk_file("sky130A/libs.tech/magic/sky130A.tcl")).ok());
  f.take_printed();
  REQUIRE(f.interp->source(pdk_file("ihp-sg13g2/libs.tech/magic/ihp-sg13g2-util.tcl")).ok());
  const std::string printed = f.take_printed();
  f.editor.script.events.remove(log);
  REQUIRE(log.seen.size() == 1);
  CHECK(log.seen.front().procedure == "::magic::i2u");
  CHECK(printed.find("magic::i2u") != std::string::npos);
  CHECK(printed.find("sky130A") != std::string::npos);
  CHECK(printed.find("sg13g2") != std::string::npos);
  // Redefinitions inside one PDK are not reported.
  REQUIRE(f.interp->source(pdk_file("ihp-sg13g2/libs.tech/magic/ihp-sg13g2-util.tcl")).ok());
  CHECK(f.take_printed().empty());
}
