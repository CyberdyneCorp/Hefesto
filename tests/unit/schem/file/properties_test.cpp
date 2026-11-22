// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The property-string grammar and boolean values (capability
// schematic-file-format "Property strings", "Boolean values"; golden case
// SCHEM-PR-01; openspec/changes/m8-schematic tasks.md 6.1).
//
// Owner: FORMAT.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "file_test_util.hpp"
#include "hefesto/schem/file/properties.hpp"

namespace schem = hefesto::schem;

namespace {

std::string token(std::string_view props, std::string_view key, schem::ValueForm form = schem::ValueForm::decoded) {
  return schem::get_token(props, key, form).value_or("<none>");
}

// The `-> [result]` parts of SCHEM-PR-01's stdout, in order. Every call
// starts a line with `xschem `; a result may span lines.
std::vector<std::string> pr01_results(const std::string& stdout_text) {
  std::vector<std::string> entries;
  std::size_t at = 0;
  while (at < stdout_text.size()) {
    auto next = stdout_text.find("\nxschem ", at);
    next = next == std::string::npos ? stdout_text.size() : next + 1;
    entries.push_back(stdout_text.substr(at, next - at));
    at = next;
  }
  std::vector<std::string> results;
  for (const auto& entry : entries) {
    const auto open = entry.find(" -> [");
    const auto close = entry.rfind(']');
    REQUIRE(open != std::string::npos);
    REQUIRE(close != std::string::npos);
    results.push_back(entry.substr(open + 5, close - open - 5));
  }
  return results;
}

}  // namespace

TEST_CASE("quoted value with spaces", "[cap:schematic-file-format][req:Property strings]") {
  const std::string props = "name=C4 m=1 device=\"tantalium capacitor\"";
  CHECK(token(props, "device") == "tantalium capacitor");
  CHECK(token(props, "device", schem::ValueForm::raw) == "\"tantalium capacitor\"");
  CHECK(token(props, "name") == "C4");
  CHECK(token(props, "Name") == "<none>");  // keys are case-sensitive
}

TEST_CASE("spaces around equals and semicolon separator", "[cap:schematic-file-format][req:Property strings]") {
  CHECK(token("a = 1;b=2", "a") == "1");
  CHECK(token("a = 1;b=2", "b") == "2");
  CHECK(token("x=1\ty=2\nz=3", "z") == "3");
}

TEST_CASE("attributes without a value, escapes and repeated keys", "[cap:schematic-file-format][req:Property strings]") {
  CHECK(token("k name=x1", "k") == "");
  CHECK(token("k name=x1", "name") == "x1");
  CHECK(token("k=\\\"q\\\"", "k") == "\"q\"");
  CHECK(token("k=\\\"q\\\"", "k", schem::ValueForm::raw) == "\\\"q\\\"");
  CHECK(token("drc=\"f {@name\\}\"", "drc") == "f {@name}");
  CHECK(token("a=1 a=2", "a") == "1");
  CHECK(token("v=\"a b\"c d=1", "v") == "a bc");
  CHECK(token("v=tcleval(x)", "v") == "tcleval(x)");  // returned unevaluated
  const auto list = schem::parse_properties("k a = 1;b=\"x y\" ");
  REQUIRE(list.size() == 3);
  CHECK(list[0] == schem::Property{"k", "", false});
  CHECK(list[1] == schem::Property{"a", "1", true});
  CHECK(list[2] == schem::Property{"b", "\"x y\"", true});
  CHECK(schem::parse_properties("").empty());
  CHECK(schem::decode_value("a\\\\b\\\"\"c\"") == "a\\b\"c");
}

TEST_CASE("setting a new key", "[cap:schematic-file-format][req:Property strings]") {
  CHECK(schem::set_token("name=p1", "lab", "VDD") == "name=p1\nlab=VDD");
  CHECK(schem::set_token("name=p1 ", "lab", "VDD") == "name=p1 lab=VDD");
  CHECK(schem::set_token("", "lab", "VDD") == "lab=VDD");
}

TEST_CASE("setting a value with spaces", "[cap:schematic-file-format][req:Property strings]") {
  CHECK(schem::set_token("name=x1 value=1", "value", ".lib a b") == "name=x1 value=\".lib a b\"");
  CHECK(schem::set_token("name=x1 value=1", "name", "a b") == "name=a b value=1");
  CHECK(schem::set_token("name=x1 value=1", "value", "\"a b\"") == "name=x1 value=\"a b\"");
  CHECK(schem::set_token("a=1;b=2", "b", "3") == "a=1;b=3");
  CHECK(schem::set_token("name=x1\nvalue=1", "value", "2") == "name=x1\nvalue=2");
}

TEST_CASE("setting a key written without equals", "[cap:schematic-file-format][req:Property strings]") {
  CHECK(schem::set_token("k name=x1", "k", "5") == "k =5 name=x1");
}

TEST_CASE("setting a key written without equals at the end", "[cap:schematic-file-format][req:Property strings]") {
  CHECK(schem::set_token("k", "k", "5") == "k=5 ");
  CHECK(schem::set_token("a=1 k", "k", "5") == "a=1 k=5 ");
}

TEST_CASE("removing a key written without equals at the end", "[cap:schematic-file-format][req:Property strings]") {
  CHECK(schem::set_token("a=1 k", "k", "") == "a=1 ");
  CHECK(schem::set_token("k", "k", std::nullopt).empty());
}

TEST_CASE("removing a key", "[cap:schematic-file-format][req:Property strings]") {
  CHECK(schem::set_token("name=x1 value=1 m=2", "value", "") == "name=x1 m=2");
  CHECK(schem::set_token("name=x1 value=1 m=2", "value", std::nullopt) == "name=x1 m=2");
  CHECK(schem::set_token("name=x1 value=1 m=2", "absent", std::nullopt) == "name=x1 value=1 m=2");
  CHECK(schem::set_token("k name=x1", "k", std::nullopt) == "name=x1");
}

TEST_CASE("SCHEM-PR-01 get_tok and subst_tok results replay", "[cap:schematic-file-format][req:Property strings]") {
  const auto expected = pr01_results(schem::test::slurp(schem::test::golden("SCHEM-PR-01", "expected/stdout.txt")));
  // The strings and keys of tests/golden/SCHEM/cases/SCHEM-PR-01/input/case.tcl, in its order. Mode 1 is the
  // raw value; no mode and mode 2 are the decoded value (no tcleval is involved).
  const std::vector<std::string> strings = {
      "name=C4 m=1 device=\"tantalium capacitor\"", "a = 1;b=2", "a=1;b=2", "k=\"x y\"", "k=\\\"q\\\"", "k", "",
      "name=p1", "name=x1 value=1", "type=nmos\nformat=\"@name\"", "drc=\"f {@name\\}\""};
  const std::vector<std::string> keys = {"name", "m", "device", "a", "b", "k", "type", "format", "drc", "value", "lab"};
  std::vector<std::string> actual;
  for (const auto& s : strings) {
    for (const auto& key : keys) {
      const std::string decoded = schem::get_token(s, key).value_or("");
      actual.push_back(decoded);
      actual.push_back(schem::get_token(s, key, schem::ValueForm::raw).value_or(""));
      actual.push_back(decoded);
    }
  }
  actual.push_back(schem::set_token("name=p1", "lab", "VDD"));
  actual.push_back(schem::set_token("name=p1 ", "lab", "VDD"));
  actual.push_back(schem::set_token("", "lab", "VDD"));
  actual.push_back(schem::set_token("name=x1 value=1", "value", ".lib a b"));
  actual.push_back(schem::set_token("name=x1 value=1", "name", "a b"));
  actual.push_back(schem::set_token("name=x1 value=1 m=2", "value", ""));
  actual.push_back(schem::set_token("k name=x1", "k", "5"));
  actual.push_back(schem::set_token("a=1;b=2", "b", "3"));
  actual.push_back(schem::set_token("name=x1\nvalue=1", "value", "2"));
  REQUIRE(actual.size() == expected.size());
  for (std::size_t i = 0; i < actual.size(); ++i) {
    INFO("call " << i);
    CHECK(actual[i] == expected[i]);
  }
}

TEST_CASE("numeric boolean", "[cap:schematic-file-format][req:Boolean values]") {
  CHECK(schem::is_true("1"));
  CHECK(schem::is_true("007"));
  CHECK(schem::is_false("0"));
  CHECK(schem::is_false("000"));
  CHECK_FALSE(schem::is_true("0"));
  CHECK(schem::is_true(schem::get_token("spice_ignore=1", "spice_ignore").value_or("")));
}

TEST_CASE("boolean words and other values", "[cap:schematic-file-format][req:Boolean values]") {
  for (const char* word : {"true", "TRUE", "On", "yes", "YeS"}) {
    CHECK(schem::is_true(word));
    CHECK_FALSE(schem::is_false(word));
  }
  for (const char* word : {"false", "Off", "NO"}) {
    CHECK(schem::is_false(word));
    CHECK_FALSE(schem::is_true(word));
  }
  for (const char* other : {"TRUEISH", "", "1.0", "-1", " true", "y"}) {
    CHECK_FALSE(schem::is_true(other));
    CHECK_FALSE(schem::is_false(other));
  }
}
