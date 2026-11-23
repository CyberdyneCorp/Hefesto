// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// Symbol property and types, templates, instance names and floater texts
// (capability schematic-symbols-and-libraries "Symbol property", "Symbol
// types", "Templates and instance attributes", "Instance names", "Floater
// texts").
//
// Owner: SYMLIB.

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <utility>

#include "hefesto/schem/file/properties.hpp"
#include "hefesto/schem/sym/instance.hpp"
#include "hefesto/schem/sym/types.hpp"

namespace schem = hefesto::schem;

namespace {

constexpr const char* kNames = "[cap:schematic-symbols-and-libraries][req:Instance names]";
constexpr const char* kTemplates = "[cap:schematic-symbols-and-libraries][req:Templates and instance attributes]";
constexpr const char* kFloaters = "[cap:schematic-symbols-and-libraries][req:Floater texts]";
constexpr const char* kTypes = "[cap:schematic-symbols-and-libraries][req:Symbol types]";
constexpr const char* kProperty = "[cap:schematic-symbols-and-libraries][req:Symbol property]";

schem::Symbol symbol_with(std::string k) {
  schem::Symbol symbol;
  symbol.document.kind = schem::FileKind::symbol;
  symbol.document.globals.k = std::move(k);
  return symbol;
}

}  // namespace

TEST_CASE("Copy of R1", kNames) {
  schem::InstanceNamer names;
  names.add("R1");
  names.add("R2");
  CHECK(names.unique("R1") == "R3");
  CHECK(names.used("R3"));
}

TEST_CASE("Vector instance copy", kNames) {
  schem::InstanceNamer names;
  names.add("x1[3:0]");
  CHECK(names.unique("x1[3:0]") == "x2[3:0]");
}

// Q-SCHEM-41 regression (review of PR #88), both modes (the namer has no
// mode): the bits a namer keeps were never charged to the editing session's
// bus budget, so a document of names just under kMaxBusBytes each held
// about 64 MiB per name. They now take from the budget given; past it a
// name is kept unexpanded.
TEST_CASE("instance names take their bits from the session's bus budget", kNames) {
  schem::BusBudget budget{30};
  schem::InstanceNamer names{&budget};
  names.add("A[3:0]");  // A[3],A[2],A[1],A[0]: 19 bytes
  CHECK(budget.remaining() == 11);
  CHECK(names.used("A[2]"));
  names.add("B[3:0]");  // 19 more do not fit: kept as written
  CHECK(budget.remaining() == 11);
  CHECK(names.used("b[3:0]"));
  CHECK_FALSE(names.used("B[2]"));
  schem::Document document;
  document.instances.push_back(schem::InstanceRecord{});
  document.instances.back().props = "name=C[1:0]";
  schem::BusBudget small{3};
  const schem::InstanceNamer loaded{document, &small};
  CHECK(small.remaining() == 3);  // C[1],C[0] is 9 bytes
  CHECK(loaded.used("c[1:0]"));
}

TEST_CASE("names compare ASCII case-insensitively", kNames) {
  schem::InstanceNamer names;
  names.add("r1");
  CHECK(names.used("R1"));
  CHECK(names.unique("R1") == "R2");
}

TEST_CASE("renaming starts from the last index used for the base and suffix", kNames) {
  schem::InstanceNamer names;
  names.add("R1");
  CHECK(names.unique("R1") == "R2");
  CHECK(names.unique("R1") == "R3");
  CHECK(names.unique("R7") == "R7");  // unused: kept
  names.add("C1");
  CHECK(names.unique("C1") == "C2");  // another base starts at 1 again
}

TEST_CASE("an unused name is kept and a name without digits gets an index", kNames) {
  schem::InstanceNamer names;
  CHECK(names.unique("Vmeas") == "Vmeas");
  CHECK(names.unique("Vmeas") == "Vmeas1");
  CHECK(names.unique("").empty());
}

TEST_CASE("a name splits into a base and a bracket suffix", kNames) {
  CHECK(schem::split_instance_name("R12") == std::pair<std::string, std::string>{"R", ""});
  CHECK(schem::split_instance_name("x1[3:0]") == std::pair<std::string, std::string>{"x", "[3:0]"});
  CHECK(schem::split_instance_name("bus[1]") == std::pair<std::string, std::string>{"bus", "[1]"});
  CHECK(schem::split_instance_name("M1a") == std::pair<std::string, std::string>{"M", ""});
  CHECK(schem::split_instance_name("noindex") == std::pair<std::string, std::string>{"noindex", ""});
}

TEST_CASE("properties without a name stay unchanged", kNames) {
  schem::InstanceNamer names;
  CHECK(names.unique_properties("value=1k m=1") == "value=1k m=1");
  CHECK(names.unique_properties("") == "");
}

TEST_CASE("the names of a document are recorded", kNames) {
  schem::Document document;
  document.instances.push_back({"res.sym", {0, 0}, 0, 0, "name=R1 value=1k"});
  document.instances.push_back({"res.sym", {0, 40}, 0, 0, "name=R2 value=1k"});
  schem::InstanceNamer names(document);
  CHECK(names.unique_properties("name=R1 value=2k") == "name=R3 value=2k");
}

TEST_CASE("Instance value wins over template", kTemplates) {
  const schem::Symbol res = symbol_with("type=resistor format=\"@name @pinlist @value m=@m\" template=\"name=R1 value=1k m=1\"");
  CHECK(schem::symbol_template(res) == "name=R1 value=1k m=1");
  CHECK(schem::instance_attribute("name=R5 value=2k", res, "value") == std::optional<std::string>{"2k"});
  CHECK(schem::instance_attribute("name=R5 value=2k", res, "m") == std::optional<std::string>{"1"});
  CHECK(schem::instance_attribute("name=R5 value=2k extra=7", res, "extra") == std::optional<std::string>{"7"});
  CHECK_FALSE(schem::instance_attribute("name=R5", res, "undefined").has_value());
}

TEST_CASE("a new instance takes the template with a unique name", kTemplates) {
  const schem::Symbol res = symbol_with("type=resistor template=\"name=R1 value=1k m=1\"");
  schem::InstanceNamer names;
  CHECK(schem::new_instance_properties(res, names) == "name=R1 value=1k m=1");
  CHECK(schem::new_instance_properties(res, names) == "name=R2 value=1k m=1");
}

TEST_CASE("Floater shows instance width: the owner", kFloaters) {
  CHECK(schem::resolve_without_owner("W=@W"));
  CHECK(schem::resolve_without_owner("100%"));
  CHECK(schem::resolve_without_owner("tcleval([pwd])"));
  CHECK_FALSE(schem::resolve_without_owner("plain text"));
  schem::Document document;
  document.instances.push_back({"nmos.sym", {0, 0}, 0, 0, "name=M1 W=2"});
  const schem::TextRecord by_name{"W=@W", {0, 0}, 0, 0, 0.4, 0.4, "name=M1"};
  const schem::TextRecord by_floater{"x", {0, 0}, 0, 0, 0.4, 0.4, "floater=M1 layer=4"};
  const schem::TextRecord plain{"x", {0, 0}, 0, 0, 0.4, 0.4, "layer=4"};
  CHECK(schem::floater_info(by_name).floater);
  CHECK(schem::floater_info(by_name).owner == "M1");
  CHECK(schem::floater_info(by_floater).owner == "M1");
  CHECK_FALSE(schem::floater_info(plain).floater);
  CHECK(schem::floater_owner(document, "M1") == &document.instances[0]);
  CHECK(schem::floater_owner(document, "M2") == nullptr);
}

TEST_CASE("floater owners match exactly or by index", kFloaters) {
  schem::Document document;
  document.instances.push_back({"res.sym", {0, 0}, 0, 0, "name=R1 value=1k"});
  document.instances.push_back({"res.sym", {0, 0}, 0, 0, "name=R2 value=2k"});
  document.instances.push_back({"res.sym", {0, 0}, 0, 0, "name=true"});
  CHECK(schem::floater_owner(document, "r1") == nullptr);  // case-sensitive, unlike "Instance names"
  CHECK(schem::floater_owner(document, "R1") == &document.instances[0]);
  CHECK(schem::floater_owner(document, "1") == &document.instances[1]);
  CHECK(schem::floater_owner(document, "0001") == &document.instances[1]);
  CHECK(schem::floater_owner(document, "3") == nullptr);
  CHECK(schem::floater_owner(document, "99999999999999999999999999") == nullptr);
  CHECK(schem::floater_owner(document, "") == nullptr);
  // A true `floater` value is the text `true`, not an index.
  const schem::TextRecord by_true{"x", {0, 0}, 0, 0, 0.4, 0.4, "floater=1"};
  CHECK(schem::floater_info(by_true).owner == "true");
  CHECK(schem::floater_owner(document, schem::floater_info(by_true).owner) == &document.instances[2]);
}

TEST_CASE("Descend only into hierarchical types", kTypes) {
  CHECK_FALSE(schem::can_descend("nmos"));
  CHECK(schem::can_descend("subcircuit"));
  CHECK(schem::can_descend("primitive"));
  CHECK_FALSE(schem::can_descend("label"));
}

TEST_CASE("type classes of the Symbol types table", kTypes) {
  CHECK(schem::is_port_type("ipin"));
  CHECK(schem::port_direction("opin") == "out");
  CHECK(schem::port_direction("iopin") == "inout");
  CHECK_FALSE(schem::is_port_type("label"));
  for (const char* type : {"logo", "netlist_commands", "netlist_options", "arch_declarations", "architecture",
                           "attributes", "package", "port_attributes", "use", "launcher", "verilog_preprocessor",
                           "timescale"}) {
    CHECK(schem::ignored_in_schematic_symbol(type));
  }
  CHECK_FALSE(schem::ignored_in_schematic_symbol("subcircuit"));
  CHECK_FALSE(schem::ignored_in_schematic_symbol("probe"));
  CHECK(schem::is_device_type("nmos"));
  CHECK(schem::is_device_type("resistor"));
  CHECK_FALSE(schem::is_device_type("bus_tap"));
  CHECK_FALSE(schem::is_device_type("subcircuit"));
}

TEST_CASE("the symbol property is the K record and keeps every key", kProperty) {
  const schem::Symbol nfet = symbol_with(
      "type=nmos lvs_format=\"@spiceprefix@name @pinlist sky130_fd_pr__@model L=@L W=@W nf=@nf m=@mult\" "
      "template=\"name=M1 model=nfet_01v8 spiceprefix=X\" vendor_key=7");
  CHECK(nfet.property() == nfet.document.globals.k);
  CHECK(schem::symbol_type(nfet) == "nmos");
  CHECK(schem::get_token(nfet.property(), "lvs_format") ==
        std::optional<std::string>{"@spiceprefix@name @pinlist sky130_fd_pr__@model L=@L W=@W nf=@nf m=@mult"});
  CHECK(schem::instance_attribute("name=M3", nfet, "model") == std::optional<std::string>{"nfet_01v8"});
  CHECK(schem::instance_attribute("name=M3", nfet, "spiceprefix") == std::optional<std::string>{"X"});
  CHECK(schem::get_token(nfet.property(), "vendor_key") == std::optional<std::string>{"7"});
}
