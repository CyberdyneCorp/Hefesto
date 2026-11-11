// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Result formats through the binding (tasks.md 5.5; capability
// scripting-api "Numeric value formatting in results", "Result formats of
// commands used by scripts"; Q-SCRIPT-6 `port last`, Q-SCRIPT-7 `cif
// scale`, both magic-compat settings).

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "binding_fixture.hpp"

using hefesto::script::test::BindingFixture;

TEST_CASE("Microns formatting", "[cap:scripting-api][req:Numeric value formatting in results]") {
  BindingFixture f;  // cifoutput style `lambda`: 0.01 um per internal unit
  f.value("box 0 0 30 100");
  f.value("units microns noprint");
  CHECK(f.value("box values") == "0 0 0.3 1");
  CHECK(f.value("units microns print; box width") == "0.3um");
}

TEST_CASE("Units round trip", "[cap:scripting-api][req:Numeric value formatting in results]") {
  BindingFixture f;
  f.value("units lambda print");
  CHECK(f.value("set u [units]; units internal; units {*}$u; units") == "lambda print");
}

TEST_CASE("Default units", "[cap:scripting-api][req:Numeric value formatting in results]") {
  BindingFixture f;
  CHECK(f.value("units") == "default");
  CHECK(f.value("box values") == "0 0 10 20");
}

TEST_CASE("Existence test idiom", "[cap:scripting-api][req:Result formats of commands used by scripts]") {
  BindingFixture f;
  REQUIRE(f.editor.cells.create("inv", f.editor.technology->plane_count()).ok());
  CHECK(f.value("cellname list exists inv") == "inv");
  CHECK(f.value("cellname list exists nand2") == "0");
}

TEST_CASE("Instance lookup failure", "[cap:scripting-api][req:Result formats of commands used by scripts]") {
  BindingFixture f;
  f.take_printed();
  CHECK(f.value("instance list celldef nosuch") == "");
}

TEST_CASE("port last with magic-compat ON", "[cap:scripting-api][req:Result formats of commands used by scripts]") {
  BindingFixture f{true};
  f.value("box 0 0 4 4; paint m1; label A");
  CHECK(f.value("port first") == "-1");
  CHECK(f.value("port last") == "0");
}

TEST_CASE("port last with magic-compat OFF", "[cap:scripting-api][req:Result formats of commands used by scripts]") {
  BindingFixture f{false};
  f.value("box 0 0 4 4; paint m1; label A");
  CHECK(f.value("port first") == "-1");
  CHECK(f.value("port last") == "-1");
  f.value("port make 3");
  CHECK(f.value("port last") == "3");
}

TEST_CASE("cif scale with magic-compat ON", "[cap:scripting-api][req:Result formats of commands used by scripts]") {
  BindingFixture f{true};
  CHECK(f.value("cif scale out") == "0.009999999776482582");
  CHECK(f.value("cif scale output") == "0.009999999776482582");
  // cifinput `scalefactor 0.5`: 10 * 1 / (2 * 1000) in single precision.
  CHECK(f.value("cif scale in") == "0.004999999888241291");
}

TEST_CASE("cif scale with magic-compat OFF", "[cap:scripting-api][req:Result formats of commands used by scripts]") {
  BindingFixture f{false};
  CHECK(f.value("cif scale out") == "0.01");
  CHECK(f.value("cif scale in") == "0.005");
}

TEST_CASE("cif style queries", "[cap:scripting-api][req:Result formats of commands used by scripts]") {
  BindingFixture f;
  CHECK(f.value("cif list ostyle") == "lambda");
  CHECK(f.value("cif listall ostyle") == "lambda coarse");
  CHECK(f.value("cif list istyle") == "in(a)");
  CHECK(f.value("cif listall istyle") == "in(a) in(b)");
  f.take_printed();
  CHECK(f.value("cif ostyle") == "");
  CHECK(f.take_printed() == "The current style is \"lambda\".\n");
  CHECK(f.value("cif listall") == "");  // no option: the help
  // The CIF reader and writer are M5's.
  CHECK(f.value("catch {cif write x}") == "0");
}
