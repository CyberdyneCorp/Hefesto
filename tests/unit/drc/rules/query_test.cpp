// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The introspection of a compiled style (tasks.md 4.8, 4.10): the
// printrules text (rules, illegal overlaps, exact-overlap types), the
// rulestats histogram, the `tech drc` command ([cap:tech-command][req:tech
// drc]) and the default width / spacing / surround values.

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/drc/rules/query.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/tech/commands.hpp"
#include "rules_fixture.hpp"

namespace cmd = hefesto::cmd;
namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
using drc::test::compile;

namespace {

std::string printed(const drc::test::Compiled& c) {
  std::ostringstream out;
  drc::print_rules(*c.rules, *c.tech, out);
  return out.str();
}

}  // namespace

TEST_CASE("printrules prints each pair's rules in Magic's format",
          "[cap:drc-rule-language][req:Overlap rules and the DRC paint table]") {
  const auto c = compile(R"(width poly 2 "w")");
  const std::string out = printed(c);
  // drcGetName: the first 8 characters of the short name ("space" for space).
  CHECK(out.find("space    poly      2 x 2   poly (active)\n"
                 "                           poly\n") != std::string::npos);
  CHECK(out.find("poly     space     2 x 2   poly (active)\n"
                 "                           poly\n"
                 "                           reverse\n") != std::string::npos);
  // Default illegal overlaps are only those of plane 0 (none here).
  CHECK(out.find("can't overlap") == std::string::npos);
}

TEST_CASE("printrules lists no_overlap pairs and exact-overlap types",
          "[cap:drc-rule-language][req:Overlap rules and the DRC paint table]") {
  const auto c = compile(R"(no_overlap m1 poly
exact_overlap m1c,ndc)");
  const std::string out = printed(c);
  CHECK(out.find("Tile type poly can't overlap type m1.\n") != std::string::npos);
  CHECK(out.find("Tile type m1 can't overlap type poly.\n") != std::string::npos);
  CHECK(out.find("Types that must overlap exactly: ndc,m1c\n") != std::string::npos);
  REQUIRE(c.rules->details()->no_overlap.size() == 1);
}

TEST_CASE("printrules continuation lines and flag words",
          "[cap:drc-rule-language][req:Overlap rules and the DRC paint table]") {
  const auto c = compile(R"(widespacing m1 10 m1 4 touching_ok "wide")");
  const std::string out = printed(c);
  CHECK(out.find("m1       space     10 x 4   m1 (metal1)\n"
                 "                           m1\n"
                 "                           reverse trigger maxwidth bends\n"
                 "                   4 x 4   space,ndc,m1c (metal1)\n") != std::string::npos);
  CHECK(out.find("both-corners\n") != std::string::npos);
}

TEST_CASE("rulestats prints the rule histogram", "[cap:drc-rule-language][req:Overlap rules and the DRC paint table]") {
  const auto c = compile(R"(width m1 2 "small"
width m1 3 "big"
frob x)");
  std::ostringstream out;
  drc::rule_stats(*c.rules, out);
  const std::string s = out.str();
  CHECK(s.find("Total number of rules specifed in tech file: 3\n") != std::string::npos);
  CHECK(s.find("Edge rules optimized away: ") != std::string::npos);
  CHECK(s.find("Edge rules left in database: " + std::to_string(c.rules->all_rules().size()) + "\n") !=
        std::string::npos);
  CHECK(s.find("Histogram of # edges vs. rules per edge:\n   0 rules/edge: ") != std::string::npos);
  CHECK(s.find(" >10 rules/edge: 0.\n") != std::string::npos);
}

TEST_CASE("default width, spacing and surround from the rule set", "[cap:drc-rule-language][req:width compilation]") {
  const auto c = compile(R"(width m1 3 "w"
width m2 4 "w2"
spacing m1 m1 5 touching_ok "s"
surround m1c m1 1 absence_illegal "s"
surround m1c m1 1 2 directional "d"
widespacing m2 10 m2 7 touching_ok "ws")");
  CHECK(drc::default_width(*c.rules, *c.tech, c.type("m1")) == 3);
  CHECK(drc::default_width(*c.rules, *c.tech, c.type("poly")) == 0);
  CHECK(drc::default_spacing(*c.rules, *c.tech, c.type("m1"), c.type("m1")) == 5);
  CHECK(drc::default_surround(*c.rules, *c.tech, c.type("m1c"), c.type("m1")) == 1);
  CHECK(drc::directional_surround(*c.rules, c.type("m1c"), c.type("m1")) == 2);
  // A trigger checked on another plane is not a directional surround.
  const auto other = compile(R"(surround m1c m2 1 2 directional "d")");
  CHECK(drc::directional_surround(*other.rules, other.type("m1c"), other.type("m2")) == 0);
  CHECK(drc::default_wide_spacing(*c.rules, *c.tech, c.type("m2"), 0) == 7);
  CHECK(drc::default_wide_spacing(*c.rules, *c.tech, c.type("m2"), 5) == 0);
}

namespace {

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {"/pdk"}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  explicit Session(const std::string& drc_body) {
    REQUIRE(tech::register_tech_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    std::string text = drc::test::read_text(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "rules.tech");
    text += "\ndrc\n" + drc_body + "\nend\n\nextract\nend\n";
    REQUIRE(services.view().files.write_atomic("/t/rules.tech", text).ok());
    REQUIRE(run("tech", {"load", "/t/rules", "-noprompt"}).ok());
  }

  cmd::CommandResult run(std::string name, std::vector<std::string> args = {}) {
    return dispatcher->execute(cmd::Invocation{std::move(name), std::move(args), cmd::Origin::script, std::nullopt});
  }
};

bool says(const cmd::CommandResult& r, std::string_view needle) {
  for (const auto& d : r.diagnostics) {
    if (d.text.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("tech drc queries the compiled rule set", "[cap:tech-command][req:tech drc]") {
  Session s(R"(width metal1 3 "w"
spacing metal1 metal1 4 touching_ok "s"
spacing metal1 metal2 6 touching_illegal "s2"
surround m1c metal1 1 absence_illegal "s"
surround m1c metal1 1 2 directional "d")");
  CHECK(s.run("tech", {"drc", "width", "metal1"}).value.to_string() == "3");
  CHECK(s.run("tech", {"drc", "spacing", "metal1"}).value.to_string() == "4");
  CHECK(s.run("tech", {"drc", "spac", "metal1", "metal1"}).value.to_string() == "4");
  CHECK(s.run("tech", {"drc", "surround", "m1c", "metal1"}).value.to_string() == "1");
  CHECK(s.run("tech", {"drc", "direc", "m1c", "metal1"}).value.to_string() == "2");
  // No rule: 0; unknown keyword: nothing, silently.
  CHECK(s.run("tech", {"drc", "width", "poly"}).value.to_string() == "0");
  const auto unknown = s.run("tech", {"drc", "frob", "metal1"});
  CHECK(unknown.ok());
  CHECK(unknown.value.is_none());
  CHECK(unknown.diagnostics.empty());
}

TEST_CASE("tech drc argument errors", "[cap:tech-command][req:tech drc]") {
  Session s(R"(width metal1 3 "w")");
  CHECK(says(s.run("tech", {"drc", "width"}), "wrong number of arguments"));
  const auto bad = s.run("tech", {"drc", "width", "nosuch"});
  CHECK_FALSE(bad.ok());
  CHECK(says(bad, "No such layer nosuch"));
  CHECK(bad.diagnostics.at(0).code == "TECH-CMD-006");
  const auto two = s.run("tech", {"drc", "surround", "m1c"});
  CHECK_FALSE(two.ok());
  CHECK(says(two, "Requires two layer types."));
  CHECK(says(s.run("tech", {"drc", "spacing", "metal1", "nosuch"}), "No such layer nosuch"));
  // TECH-CMD-005 is retired.
  CHECK(s.run("tech", {"drc", "width", "metal1"}).ok());
}

TEST_CASE("tech drc reports values in the display units", "[cap:tech-command][req:tech drc]") {
  Session s(R"(width metal1 3 "w")");
  REQUIRE(s.run("scalegrid", {"1", "2"}).ok());  // rescales the rules (drc::rescale_rules, tasks.md 4.7)
  CHECK(s.run("tech", {"drc", "width", "metal1"}).value.to_string() == "6");  // default units: internal
  REQUIRE(s.run("tech", {"lambda"}).ok());
}
