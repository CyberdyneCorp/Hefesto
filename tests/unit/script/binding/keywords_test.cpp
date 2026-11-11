// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The option keyword rule (tasks.md 5.6; capability scripting-api "Option
// keyword matching"; Q-SCRIPT-14, compat: a tab ends a keyword only with
// magic-compat OFF).

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <string_view>

#include "binding_fixture.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/script/binding/keyword_rule.hpp"
#include "hefesto/ui/magic_help.hpp"

namespace cmd = hefesto::cmd;
namespace ui = hefesto::ui;
using hefesto::script::test::BindingFixture;

TEST_CASE("Prefix selection", "[cap:scripting-api][req:Option keyword matching]") {
  BindingFixture f;
  REQUIRE(f.editor.cells.create("inv", f.editor.technology->plane_count()).ok());
  const std::string tops = f.value("cellname list top");
  CHECK(tops.find("inv") != std::string::npos);
  CHECK(tops == f.value("cellname list topcells"));
}

TEST_CASE("Case-insensitive keyword", "[cap:scripting-api][req:Option keyword matching]") {
  BindingFixture f;
  f.value("units MICRONS");
  CHECK(f.value("units") == "microns noprint");
  CHECK(f.value("cellname list ::magic::exists top") == "top");
}

namespace {

/// One instance `x1` of cell `inv` in `top`, selected.
std::string selected_instance(BindingFixture& f) {
  hefesto::db::CellDef* inv = f.editor.cells.create("inv", f.editor.technology->plane_count()).value();
  inv->flags().available = true;
  REQUIRE(f.eval("box 0 0 4 4; getcell inv; identify x1").ok());
  const auto uses = hefesto::db::uses_in_search_order(*f.top);
  REQUIRE(uses.size() == 1);
  return uses.front()->id;
}

std::string instance_id(const BindingFixture& f) {
  return hefesto::db::uses_in_search_order(*f.top).front()->id;
}

}  // namespace

TEST_CASE("Tab-terminated alias with magic-compat ON", "[cap:scripting-api][req:Option keyword matching]") {
  BindingFixture f{true};
  CHECK(selected_instance(f) == "x1");
  // `id` and `get` are ambiguous in Magic's tables: error status, nothing
  // printed, nothing renamed.
  f.take_printed();
  CHECK(f.error("magic::id x2") == "");
  CHECK(f.error("magic::get x2") == "");
  CHECK(f.take_printed().empty());
  CHECK(instance_id(f) == "x1");
}

TEST_CASE("Tab-terminated alias with magic-compat OFF", "[cap:scripting-api][req:Option keyword matching]") {
  BindingFixture f{false};
  CHECK(selected_instance(f) == "x1");
  CHECK(f.eval("magic::id x2").ok());
  CHECK(instance_id(f) == "x2");
}

TEST_CASE("The terminator rule of each lookup follows magic-compat", "[cap:scripting-api][req:Option keyword matching]") {
  constexpr std::array<std::string_view, 2> table{"id\tidentify alias", "identify name"};
  {
    const cmd::KeywordTerminatorScope on{true};
    CHECK(cmd::keyword_terminator() == cmd::KeywordTerminator::space);
    CHECK(cmd::lookup("id", table).match == cmd::Match::ambiguous);
    CHECK(cmd::match_name("id", {table[0], table[1]}).status == cmd::LookupStatus::ambiguous);
    CHECK(ui::magic_lookup_ambiguous("layout", "id"));
    CHECK(ui::magic_lookup_ambiguous("layout", "get"));
  }
  {
    const cmd::KeywordTerminatorScope off{false};
    CHECK(cmd::lookup("id", table).match == cmd::Match::found);
    CHECK(cmd::lookup("id", table).index == 0);
    CHECK(cmd::match_name("id", {table[0], table[1]}).status == cmd::LookupStatus::found);
    CHECK_FALSE(ui::magic_lookup_ambiguous("layout", "id"));
    CHECK_FALSE(ui::magic_lookup_ambiguous("layout", "get"));
    // Space-terminated keywords behave the same in both settings.
    constexpr std::array<std::string_view, 2> spaced{"topcells list", "top level"};
    CHECK(cmd::lookup("top", spaced).index == 1);
  }
  CHECK(cmd::keyword_terminator() == cmd::KeywordTerminator::space);  // the scope ended
}
