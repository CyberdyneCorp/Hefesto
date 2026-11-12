// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// capability pcell-toolkit "Parameter dictionary model": ordered Tcl dict
// semantics and the Tcl string form of parameter values, checked against
// the embedded Tcl 8.6 (the `parameters` property is saved byte for byte).

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/editor/editor_state.hpp"
#include "hefesto/pcell/framework/parameters.hpp"
#include "hefesto/pcell/framework/tcl_list.hpp"
#include "hefesto/script/tcl/tcl_interpreter.hpp"

namespace pcell = hefesto::pcell;

namespace {

struct Tcl {
  hefesto::editor::EditorState state;
  std::unique_ptr<hefesto::script::Interpreter> interp =
      hefesto::script::create_project_interpreter(state, hefesto::script::SandboxPolicy{});

  std::string eval(std::string_view script) {
    const auto r = interp->evaluate(script);
    REQUIRE(r.ok());
    return pcell::tcl_string(r.value);
  }
};

}  // namespace

TEST_CASE("Defaults merge", "[cap:pcell-toolkit][req:Parameter dictionary model]") {
  const pcell::ParameterDict defaults{{{"w", "1"}, {"l", "0.15"}, {"nf", "1"}}};
  const auto merged = defaults.merged(pcell::ParameterDict{{{"nf", "4"}}});
  CHECK(merged.to_tcl() == "w 1 l 0.15 nf 4");
  const auto extra = defaults.merged(pcell::ParameterDict{{{"m", "2"}, {"w", "3"}}});
  CHECK(extra.to_tcl() == "w 3 l 0.15 nf 1 m 2");
}

TEST_CASE("a parsed dictionary keeps its text until modified, as a Tcl dict does",
          "[cap:pcell-toolkit][req:Parameter dictionary model]") {
  const std::string text = "w 1.0\n  l {0.15}   compatible {a b}";
  auto parsed = pcell::ParameterDict::from_tcl(text);
  REQUIRE(parsed.ok());
  auto d = parsed.value();
  CHECK(d.get("l") == "0.15");
  CHECK(d.get("compatible") == "a b");
  // dict merge with an empty dictionary keeps the string representation.
  CHECK(d.merged(pcell::ParameterDict{}).to_tcl() == text);
  // Removing an absent key keeps it; any put drops it.
  d.erase("gencell");
  CHECK(d.to_tcl() == text);
  d.set("w", "1.0");
  CHECK(d.to_tcl() == "w 1.0 l 0.15 compatible {a b}");

  Tcl tcl;
  CHECK(tcl.eval("set d {" + text + "}; format %s [dict merge $d {}]") == text);
  CHECK(tcl.eval("format %s [dict set d w 1.0]") == "w 1.0 l 0.15 compatible {a b}");
}

TEST_CASE("an odd or malformed list is not a dictionary", "[cap:pcell-toolkit][req:Parameter dictionary model]") {
  const auto odd = pcell::ParameterDict::from_tcl("w 1 l");
  REQUIRE_FALSE(odd.ok());
  CHECK(odd.error().code == "PCELL-LIST-001");
  CHECK(odd.error().text == "missing value to go with key");
  const auto brace = pcell::ParameterDict::from_tcl("w {1");
  REQUIRE_FALSE(brace.ok());
  CHECK(brace.error().text == "unmatched open brace in list");
}

TEST_CASE("Tcl list formatting and splitting match Tcl 8.6", "[cap:pcell-toolkit][req:Parameter dictionary model]") {
  Tcl tcl;
  const std::vector<std::string> elements = {
      "", "a", "a b", "#x", "{", "}", "a{b", "a}b", "{a}", "\"q\"", "a\"b", "x]", "[y]", "$v", "a;b", "a\\b",
      "a\\", "a\\\nb", "\t", "a\nb", "{a b} c", "#", "}{", "\\{", "x{y}z", "\xC3\xA9 x", "a\\}", "\"", "]",
      "{}", "a\rb", "\f\v"};
  for (std::size_t i = 0; i < elements.size(); ++i) {
    for (const bool first : {true, false}) {
      const std::string& e = elements[i];
      INFO("element #" << i << (first ? " first" : " second"));
      tcl.interp->set_var("e", hefesto::cmd::Value{e});
      const std::string expected = tcl.eval(first ? "format %s [list $e]" : "string range [list x $e] 2 end");
      CHECK(pcell::tcl_list_element(e, first) == expected);
      // Round trip through the splitter.
      const std::vector<std::string> words{"k", e};
      const auto split = pcell::split_tcl_list(pcell::merge_tcl_list(words));
      REQUIRE(split.ok());
      CHECK(split.value() == words);
    }
  }
  // Backslash and quote forms read by the splitter.
  const auto split = pcell::split_tcl_list(R"(a\x41 "b c" {d {e}} é \101 f\
   g)");
  REQUIRE(split.ok());
  CHECK(split.value() == std::vector<std::string>{"aA", "b c", "d {e}", "\xC3\xA9", "A", "f g"});
  CHECK(pcell::split_tcl_list("{a}b").error().text == "list element in braces followed by \"b\" instead of space");
  CHECK(pcell::split_tcl_list("\"a\"b").error().text == "list element in quotes followed by \"b\" instead of space");
  CHECK(pcell::split_tcl_list("\"a").error().text == "unmatched open quote in list");
}
