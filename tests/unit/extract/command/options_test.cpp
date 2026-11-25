// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The option parsers of `extract` (tasks.md 9.1, 9.3, 9.4; capability
// extract-commands): do/no, warn, length, path, style, halo, stepsize and
// their listings, with Magic's messages.
//
// Owner: EXTCMD.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/extract/command/session.hpp"
#include "run_fixture.hpp"

using extcmd_test::Session;

namespace {

const hefesto::extract::ExtractOptions& options(Session& s) {
  return s.editor.extract.options;
}

}  // namespace

TEST_CASE("an ambiguous option prefix is reported", "[cap:extract-commands][req:Command syntax and option matching]") {
  Session s;
  REQUIRE(s.run("extract s").ok());
  CHECK(s.take_printed() ==
        "\"s\" isn't a valid extract option.  Type \"extract help\" for a list of valid options.\n");
  REQUIRE(s.run("extract STY").ok());  // case-insensitive prefix
  CHECK(s.take_printed() == "The current style is \"exttest\".\n");
}

TEST_CASE("extract do lists Magic's start-up option settings", "[cap:extract-commands][req:Option defaults]") {
  Session s;
  REQUIRE(s.run("extract do").ok());
  CHECK(s.take_printed() ==
        "The following are the extractor option settings:\n"
        "do adjust\ndo capacitance\nno corners (scale factor 1)\ndo coupling\ndo fringehalo\ndo length\n"
        "do lumped R\ndo label check\ndo aliases\ndo unique\nno unique notopports\nno resistance (extresist)\n");
  REQUIRE(s.run("extract warn").ok());
  CHECK(s.take_printed() == "The following extractor warnings are enabled:\n    do dup\n    do fets\n    no labels\n");
  REQUIRE(s.run("extract path").ok());
  CHECK(s.take_printed() == "(none)\n");
}

TEST_CASE("extract no all keeps fringehalo, then do capacitance leaves coupling off",
          "[cap:extract-commands][req:Extraction options]") {
  Session s;
  REQUIRE(s.run("extract no all").ok());
  REQUIRE(s.run("extract do capacitance").ok());
  CHECK(options(s).fringehalo);
  CHECK_FALSE(options(s).coupling);
  CHECK(options(s).capacitance);
  CHECK_FALSE(options(s).unique);
  CHECK(options(s).labelcheck);
  CHECK(options(s).aliases);
  REQUIRE(s.run("extract do all").ok());
  CHECK(options(s).adjust);
  CHECK(options(s).coupling);
  CHECK(options(s).lumped);
  CHECK(options(s).length);
  CHECK(options(s).unique);
  CHECK_FALSE(options(s).extresist);
  CHECK_FALSE(options(s).corners);
}

TEST_CASE("extract do corners sets the factor; a non-number changes nothing",
          "[cap:extract-commands][req:Extraction options]") {
  Session s;
  REQUIRE(s.run("extract do corners 0.8").ok());
  CHECK(options(s).corners);
  CHECK(options(s).corner_factor == 0.8);
  REQUIRE(s.run("extract no corners abc").ok());
  CHECK(s.take_printed() == "Usage:  extract do corners [value]\n");
  CHECK(options(s).corners);
  CHECK(options(s).corner_factor == 0.8);
}

TEST_CASE("unique notopports, resistance and local", "[cap:extract-commands][req:Extraction options]") {
  Session s;
  REQUIRE(s.run("extract no unique").ok());
  REQUIRE(s.run("extract do unique notop").ok());
  CHECK(options(s).unique);
  CHECK(options(s).unique_notopports);
  REQUIRE(s.run("extract no unique notopports").ok());
  CHECK_FALSE(options(s).unique);
  CHECK_FALSE(options(s).unique_notopports);
  REQUIRE(s.run("extract do resistance").ok());
  CHECK(options(s).extresist);
  REQUIRE(s.run("extract no extresist").ok());
  CHECK_FALSE(options(s).extresist);
  REQUIRE(s.run("extract do local").ok());
  CHECK(options(s).path == std::optional<std::string>{"."});
  REQUIRE(s.run("extract no local").ok());
  CHECK_FALSE(options(s).path.has_value());
  REQUIRE(s.run("extract do bogus").ok());
  const std::string usage = s.take_printed();
  CHECK(usage.starts_with("Usage: extract do option\n   or  extract no option\nwhere option is one of:\n  adjust"));
}

// Proposed Q-EXTRACT-38: Magic applies the table index of `unique` (11 =
// 0xb) as a bit mask after the usage message.
TEST_CASE("extract do unique with a bad modifier, magic-compat ON: Magic's bit-mask side effect",
          "[cap:extract-commands][req:Extraction options]") {
  Session s(true);
  REQUIRE(s.run("extract no all").ok());
  REQUIRE(s.run("extract do unique xyz").ok());
  CHECK(s.take_printed() == "Usage:  extract do unique [notopports]\n");
  CHECK(options(s).adjust);
  CHECK(options(s).capacitance);
  CHECK(options(s).lumped);
  CHECK_FALSE(options(s).unique);
}

TEST_CASE("extract do unique with a bad modifier, magic-compat OFF: no change",
          "[cap:extract-commands][req:Extraction options]") {
  Session s(false);
  REQUIRE(s.run("extract no all").ok());
  REQUIRE(s.run("extract do unique xyz").ok());
  CHECK(s.take_printed() == "Usage:  extract do unique [notopports]\n");
  CHECK_FALSE(options(s).adjust);
  CHECK_FALSE(options(s).capacitance);
  CHECK_FALSE(options(s).lumped);
  CHECK_FALSE(options(s).unique);
}

TEST_CASE("extract warn enables and disables warning classes", "[cap:extract-commands][req:Warnings]") {
  Session s;
  REQUIRE(s.run("extract warn no fets").ok());
  CHECK_FALSE(options(s).warn_fets);
  CHECK(options(s).warn_dup);
  REQUIRE(s.run("extract warn all").ok());
  CHECK(options(s).warn_fets);
  CHECK(options(s).warn_labels);
  REQUIRE(s.run("extract warn no all").ok());
  CHECK_FALSE(options(s).warn_dup);
  CHECK_FALSE(options(s).warn_fets);
  CHECK_FALSE(options(s).warn_labels);
  REQUIRE(s.run("extract warn nonsense").ok());
  CHECK(s.take_printed().starts_with("Usage: extract warnings [no] option\nwhere option is one of:\n  all"));
}

TEST_CASE("extract path sets, prints and clears the output path", "[cap:extract-commands][req:Output path and file location]") {
  Session s;
  REQUIRE(s.run("extract path ext/").ok());
  REQUIRE(s.run("extract path").ok());
  CHECK(s.take_printed() == "The extraction path is: ext/\n");
  CHECK(s.run("extract list path").value.to_string() == "ext/");
  for (const auto* none : {"NONE", "(none)", "Null"}) {
    REQUIRE(s.run("extract path ext/").ok());
    REQUIRE(s.run(std::string{"extract path "} + none).ok());
    CHECK_FALSE(options(s).path.has_value());
  }
  CHECK(s.run("extract list path").value.to_string() == "(none)");
}

TEST_CASE("extract length keeps the driver and receiver lists", "[cap:extract-commands][req:Path length extraction]") {
  Session s;
  REQUIRE(s.run("extract length driver a b a").ok());
  REQUIRE(s.run("extract length receiver r").ok());
  CHECK(s.editor.extract.length.drivers == std::vector<std::string>{"a", "b"});
  CHECK(s.editor.extract.length.receivers == std::vector<std::string>{"r"});
  REQUIRE(s.run("extract length driver").ok());
  CHECK(s.take_printed() == "Must specify one or more terminal names\n");
  REQUIRE(s.run("extract length clear").ok());
  CHECK(s.editor.extract.length.drivers.empty());
  CHECK(s.editor.extract.length.receivers.empty());
}

TEST_CASE("extract style prints, lists and selects styles", "[cap:extract-commands][req:Style, halo and step size]") {
  Session s(true, true, "/w", extcmd_test::case_input("EX-002", "exttest.tech"));
  REQUIRE(s.run("extract style").ok());
  CHECK(s.take_printed() == "The current style is \"exttest\".\n");
  CHECK(s.run("extract list style").value.to_string() == "exttest");
  CHECK(s.run("extract listall style").value.to_string() == "exttest exttest1000");
  REQUIRE(s.run("extract style exttest").ok());
  CHECK(s.take_printed() ==
        "Extraction style \"exttest\" is ambiguous.\nThe current style is \"exttest\".\n"
        "The extraction styles are: exttest, exttest1000.\n");
  REQUIRE(s.run("extract style exttest1").ok());
  // ExtLoadStyle re-reads the style and prints its non-electrical list first.
  CHECK(s.take_printed().ends_with("Extraction style is now \"exttest1\"\n"));
  CHECK(s.editor.extract.current->name == "exttest1000");
  REQUIRE(s.run("extract style nosuch").ok());
  CHECK(s.take_printed().starts_with("\"nosuch\" is not one of the extraction styles Magic knows.\n"));
}

TEST_CASE("extract halo and stepsize print and set the style's values",
          "[cap:extract-commands][req:Style, halo and step size]") {
  Session s;
  REQUIRE(s.run("snap internal").ok());
  REQUIRE(s.run("extract halo 8").ok());
  CHECK(s.editor.extract.current->side_halo == 8);
  CHECK(s.run("extract halo").value.to_string() == "8");
  REQUIRE(s.run("extract halo -1").ok());
  CHECK(s.take_printed().ends_with("Bad halo distance.  Halo must be non-negative.\n"));
  CHECK(s.editor.extract.current->side_halo == 8);
  REQUIRE(s.run("extract stepsize 0").ok());
  CHECK(s.take_printed().ends_with("Bad step size.  Step size must be strictly positive.\n"));
  REQUIRE(s.run("extract stepsize 50").ok());
  CHECK(s.editor.extract.current->step_size == 50);
}

// Q-EXTRACT-25 (replicate): the overrides change the loaded style, so a
// grid rescale scales them and a style reselection restores the tech file.
TEST_CASE("a halo override survives a rescale scaled and is lost on a style reload",
          "[cap:extract-commands][req:Style, halo and step size]") {
  Session s;
  const std::int32_t tech_halo = s.editor.extract.current->side_halo;
  REQUIRE(s.run("snap internal").ok());
  REQUIRE(s.run("extract halo 8").ok());
  REQUIRE(s.run("scalegrid 1 2").ok());
  CHECK(s.editor.extract.current->side_halo == 16);
  REQUIRE(s.run("extract style exttest").ok());
  CHECK(s.editor.extract.current->side_halo == 2 * tech_halo);
  CHECK_FALSE(s.editor.extract.overrides.halo.has_value());
}

TEST_CASE("extract options other than style and help need a window",
          "[cap:extract-commands][req:Command syntax and option matching]") {
  Session s;
  s.editor.command_point.reset();
  const auto result = s.run("extract do");
  REQUIRE_FALSE(result.ok());
  CHECK(result.diagnostics.front().text == "Point to a window first");
  CHECK(s.run("extract style").ok());
}
