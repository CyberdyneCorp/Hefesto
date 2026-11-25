// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// lvs/net: the netlist database, device classes, name matching, SI values
// and property expressions (capability lvs-netlist-input; tasks.md 6.1).
// Written from the spec text only (netgen clean room, Q-LVS-30).
//
// Owner: LVSREAD.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <string>

#include "hefesto/lvs/net/device_class.hpp"
#include "hefesto/lvs/net/expression.hpp"
#include "hefesto/lvs/net/names.hpp"
#include "hefesto/lvs/net/netlist_db.hpp"
#include "hefesto/lvs/net/si_value.hpp"

namespace lvs = hefesto::lvs;

TEST_CASE("cells are identified by name and file number",
          "[cap:lvs-netlist-input][req:Netlist database model]") {
  lvs::NetlistDb db;
  lvs::Cell first{"inv", 0};
  first.flags.case_insensitive = true;
  lvs::Cell second{"inv", 1};
  second.flags.case_insensitive = true;
  const auto a = db.add_cell(first);
  const auto b = db.add_cell(second);
  CHECK(a != b);
  CHECK(db.find_cell("inv", 0) == a);
  CHECK(db.find_cell("INV", 1) == b);
  CHECK_FALSE(db.find_cell("inv", 2).has_value());
  CHECK_FALSE(db.find_cell_exact("INV", 0).has_value());
  db.rename_cell(a, "inv[[0]]");
  CHECK(db.find_cell_exact("inv[[0]]", 0) == a);
  CHECK_FALSE(db.find_cell("inv", 0).has_value());
}

TEST_CASE("a cell holds ordered ports, nets and instances",
          "[cap:lvs-netlist-input][req:Netlist database model]") {
  lvs::Cell cell{"buf"};
  cell.flags.case_insensitive = true;
  const auto in = cell.add_net("in");
  const auto out = cell.add_net("out");
  const auto mid = cell.add_net("mid");
  CHECK(in == 1);  // nets are positive integers
  cell.ports = {lvs::Port{"in", in, false}, lvs::Port{"out", out, false}};
  cell.instances.push_back(lvs::Instance{0, "inv:1", {in, mid}, {}});
  CHECK(cell.find_net("MID") == mid);
  CHECK(cell.net_named("mid") == mid);
  CHECK(cell.internal_nets() == std::vector<lvs::NetId>{mid});

  // Dropping a net renumbers the others and unconnects its users.
  cell.compact_nets({true, false, true});
  CHECK(cell.nets.size() == 2);
  CHECK(cell.ports[1].net == lvs::kNoNet);
  CHECK(cell.instances[0].connections == std::vector<lvs::NetId>{1, 2});
  CHECK(cell.find_net("mid") == 2);
  CHECK(cell.find_net("out") == lvs::kNoNet);
}

TEST_CASE("properties are looked up by key", "[cap:lvs-netlist-input][req:Netlist database model]") {
  lvs::Cell cell{"nfet"};
  cell.properties.push_back(lvs::PropertyDef{"W", lvs::PropertyType::real, 0.01, "0"});
  REQUIRE(cell.find_property_def("w") != nullptr);
  CHECK(cell.find_property_def("w")->tolerance == 0.01);
  const lvs::PropertyRecord record{{"W", "1u"}, {"L", "0.15u"}};
  REQUIRE(lvs::find_property(record, "l") != nullptr);
  CHECK(lvs::find_property(record, "l")->value == "0.15u");
  CHECK(lvs::find_property(record, "M") == nullptr);
}

TEST_CASE("every device class has its pin counts", "[cap:lvs-netlist-input][req:Device classes]") {
  using lvs::DeviceClass;
  CHECK(lvs::device_class_pins(DeviceClass::nmos).min == 3);
  CHECK(lvs::device_class_pins(DeviceClass::pmos4).max == 4);
  CHECK(lvs::device_class_pins(DeviceClass::fet).min == 3);
  CHECK(lvs::device_class_pins(DeviceClass::fet).max == 4);
  CHECK(lvs::device_class_pins(DeviceClass::res3).min == 3);
  CHECK(lvs::device_class_pins(DeviceClass::ecap).min == 3);
  CHECK(lvs::device_class_pins(DeviceClass::xline).min == 4);
  CHECK(lvs::device_class_name(DeviceClass::xline) == "transmission line");
  CHECK(lvs::find_device_class("current source") == DeviceClass::isource);
  CHECK_FALSE(lvs::find_device_class("mosfet").has_value());
}

TEST_CASE("module is a black box and only subcircuits flatten",
          "[cap:lvs-netlist-input][req:Device classes]") {
  using lvs::DeviceClass;
  CHECK(lvs::is_flattenable(DeviceClass::subcircuit));
  CHECK_FALSE(lvs::is_flattenable(DeviceClass::module));
  CHECK_FALSE(lvs::is_primitive(DeviceClass::module));
  CHECK_FALSE(lvs::is_primitive(DeviceClass::subcircuit));
  CHECK(lvs::is_primitive(DeviceClass::capacitor));
  CHECK(lvs::is_primitive(DeviceClass::undefined));
}

TEST_CASE("bus delimiters fold to one pair", "[cap:lvs-netlist-input][req:Name matching and case sensitivity]") {
  CHECK(lvs::names_equal("data[3]", "DATA<3>", true));
  CHECK(lvs::names_equal("a{3}", "A<3>", true));
  CHECK_FALSE(lvs::names_equal("data[3]", "DATA<3>", false));
  CHECK(lvs::names_equal("Vdd", "Vdd", false));
  CHECK_FALSE(lvs::names_equal("Vdd", "VDD", false));
  CHECK(lvs::names_equal("Vdd", "VDD", true));
}

TEST_CASE("escaped names match their plain form", "[cap:lvs-netlist-input][req:Name matching and case sensitivity]") {
  CHECK(lvs::names_equal("\\net1 ", "net1", true));
  CHECK(lvs::names_equal("NET1", "\\net1", true));
  CHECK_FALSE(lvs::names_equal("\\net1 ", "net1", false));
}

TEST_CASE("a null name equals nothing", "[cap:lvs-netlist-input][req:Name matching and case sensitivity]") {
  CHECK_FALSE(lvs::names_equal("", "", true));
  CHECK_FALSE(lvs::names_equal("", "", false));
  CHECK_FALSE(lvs::names_equal("a", "", true));
}

TEST_CASE("high bytes fold to themselves (Q-LVS-3)",
          "[cap:lvs-netlist-input][req:Name matching and case sensitivity]") {
  // Regression: netgen indexes its fold tables with a signed char, so bytes
  // >= 0x80 read out of bounds. Hefesto indexes by unsigned byte.
  for (unsigned b = 0x80; b <= 0xFF; ++b) {
    CHECK(lvs::fold_name_byte(static_cast<unsigned char>(b)) == b);
  }
  const std::string accented = "n\xC3\xA9t";
  CHECK(lvs::names_equal(accented, "N\xC3\xA9T", true));
  CHECK_FALSE(lvs::names_equal(accented, "N\xC3\x89T", true));
}

TEST_CASE("lookup keys fold brackets as equality does (Q-LVS-3)",
          "[cap:lvs-netlist-input][req:Name matching and case sensitivity]") {
  // Regression: netgen's hash folds letters only, so `a[1]` and `A<1>`
  // compare equal yet miss each other in a lookup.
  CHECK(lvs::name_key("a[1]") == lvs::name_key("A<1>"));
  lvs::Cell cell{"top"};
  cell.flags.case_insensitive = true;
  const auto net = cell.add_net("a[1]");
  CHECK(cell.find_net("A<1>") == net);
  lvs::NetlistDb db;
  lvs::Cell bus{"reg{0}"};
  bus.flags.case_insensitive = true;
  const auto id = db.add_cell(bus);
  CHECK(db.find_cell("REG[0]", 0) == id);
}

TEST_CASE("compared cells match case-insensitively only if both do",
          "[cap:lvs-netlist-input][req:Name matching and case sensitivity]") {
  lvs::Cell spice{"a"};
  spice.flags.case_insensitive = true;
  lvs::Cell verilog{"a"};
  CHECK(lvs::compare_case_insensitive(spice, spice));
  CHECK_FALSE(lvs::compare_case_insensitive(spice, verilog));
  CHECK_FALSE(lvs::names_equal("Vdd", "VDD", lvs::compare_case_insensitive(verilog, spice)));
}

TEST_CASE("Q-LVS-24 placeholder test, netgen-compat ON: any flag counts", "[cap:lvs-hierarchy][req:Pin matching]") {
  lvs::Cell cell{"x"};
  CHECK_FALSE(lvs::placeholder_pin_equivalence(cell, true));
  cell.flags.case_insensitive = true;  // a SPICE cell, not a placeholder
  CHECK(lvs::placeholder_pin_equivalence(cell, true));
}

TEST_CASE("Q-LVS-24 placeholder test, netgen-compat OFF: the placeholder flag only", "[cap:lvs-hierarchy][req:Pin matching]") {
  lvs::Cell cell{"x"};
  cell.flags.case_insensitive = true;
  cell.flags.top = true;
  CHECK_FALSE(lvs::placeholder_pin_equivalence(cell, false));
  cell.flags.placeholder = true;
  CHECK(lvs::placeholder_pin_equivalence(cell, false));
}

TEST_CASE("mega versus milli", "[cap:lvs-netlist-input][req:SI value parsing]") {
  CHECK(lvs::parse_si_value("1meg") == 1e6);
  CHECK(lvs::parse_si_value("1m") == 1e-3);
  CHECK(lvs::parse_si_value("1MEG") == 1e6);
}

TEST_CASE("scale suffixes and unit words", "[cap:lvs-netlist-input][req:SI value parsing]") {
  CHECK(lvs::parse_si_value("10pF") == 1e-11);
  CHECK(lvs::parse_si_value("2kohms") == 2000.0);
  CHECK(lvs::parse_si_value("1g") == 1e9);
  CHECK(lvs::parse_si_value("3c") == 3e-2);
  CHECK(lvs::parse_si_value("0.15u") == 0.15e-6);
  CHECK(lvs::parse_si_value("4n") == 4e-9);
  CHECK(lvs::parse_si_value("5f") == 5e-15);
  CHECK(lvs::parse_si_value("6a") == 6e-18);
  CHECK(lvs::parse_si_value("2.5e-3k") == 2.5);
  CHECK(lvs::parse_si_value("7 V") == 7.0);
  CHECK(lvs::parse_si_value("1uH") == 1e-6);
  CHECK(lvs::parse_si_value("1uhenries") == 1e-6);
  CHECK(lvs::parse_si_value("3volts") == 3.0);
  CHECK(lvs::parse_si_value("1e3") == 1000.0);
  CHECK(lvs::parse_si_value("-2k") == -2000.0);
  CHECK(lvs::parse_si_value("+.5") == 0.5);
}

TEST_CASE("unknown unit leaves the string", "[cap:lvs-netlist-input][req:SI value parsing]") {
  CHECK_FALSE(lvs::parse_si_value("3x").has_value());
  CHECK_FALSE(lvs::parse_si_value("").has_value());
  CHECK_FALSE(lvs::parse_si_value("k").has_value());
  CHECK_FALSE(lvs::parse_si_value("1kk").has_value());
  CHECK_FALSE(lvs::parse_si_value(".").has_value());
}

TEST_CASE("the result is the correctly rounded decimal", "[cap:lvs-netlist-input][req:SI value parsing]") {
  // 0.1u is 1e-7 exactly as a decimal, which a product 0.1 * 1e-6 in double
  // arithmetic misses.
  CHECK(lvs::parse_si_value("0.1u") == 1e-7);
  CHECK(lvs::parse_si_value("3.3n") == 3.3e-9);
  CHECK(lvs::parse_si_value("1e999999999999") == HUGE_VAL);
  CHECK(lvs::parse_si_value("1e-999999999999") == 0.0);
}

TEST_CASE("number test for value tokens", "[cap:lvs-netlist-input][req:SI value parsing]") {
  CHECK(lvs::is_si_number("1k"));
  CHECK(lvs::is_si_number("10"));
  CHECK(lvs::is_si_number("10pF"));
  CHECK(lvs::is_si_number("1 m"));
  CHECK_FALSE(lvs::is_si_number("3x"));
  CHECK_FALSE(lvs::is_si_number("rpoly"));
  CHECK_FALSE(lvs::is_si_number("nfet"));
}

TEST_CASE("expressions resolve through instance, cell and .PARAM",
          "[cap:lvs-netlist-input][req:Globals, includes and parameters]") {
  lvs::ParameterTable parameters{{"WMIN", "0.5u"}, {"K", "2"}};
  lvs::Cell parent{"cell"};
  parent.properties.push_back(lvs::PropertyDef{"l", lvs::PropertyType::string, 0.0, "0.15u"});
  const lvs::PropertyRecord instance{{"nf", "4"}};
  const lvs::ParameterScope scope{&instance, &parent, &parameters};

  const auto w = lvs::resolve_value("'wmin*k'", scope);
  CHECK(w.type == lvs::PropertyType::value);
  CHECK(w.number == Catch::Approx(1e-6));
  CHECK(lvs::resolve_value("{l * nf + (1u)}", scope).number == Catch::Approx(1.6e-6));
  CHECK(lvs::resolve_value("-(2*-3)", scope).number == 6.0);
  CHECK(lvs::resolve_value("1k", scope).number == 1000.0);
}

TEST_CASE("an unresolved expression stays an expression",
          "[cap:lvs-netlist-input][req:Globals, includes and parameters]") {
  const lvs::ParameterScope empty;
  const auto unresolved = lvs::resolve_value("'w*2'", empty);
  CHECK(unresolved.type == lvs::PropertyType::expression);
  CHECK(unresolved.text == "'w*2'");
  CHECK(lvs::resolve_value("rpoly", empty).type == lvs::PropertyType::string);
  CHECK(lvs::resolve_value("'1/0'", empty).type == lvs::PropertyType::expression);
}

TEST_CASE("cyclic and deep expressions end unresolved",
          "[cap:lvs-netlist-input][req:Globals, includes and parameters]") {
  // Regression: a parameter defined through itself, or thousands of nested
  // parentheses, must not recurse without bound.
  lvs::ParameterTable parameters{{"A", "'b+1'"}, {"B", "'a+1'"}};
  const lvs::ParameterScope scope{nullptr, nullptr, &parameters};
  CHECK(lvs::resolve_value("'a'", scope).type == lvs::PropertyType::expression);
  const std::string deep = "'" + std::string(100000, '(') + "1" + std::string(100000, ')') + "'";
  CHECK(lvs::resolve_value(deep, scope).type == lvs::PropertyType::expression);
  const std::string shallow = "'" + std::string(10, '(') + "1" + std::string(10, ')') + "'";
  CHECK(lvs::resolve_value(shallow, scope).number == 1.0);
}
