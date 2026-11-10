// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// mask-hints "Setting mask hints with the property command" through the
// dispatcher (MISC-MH-01's command half).

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/editor/editor_state.hpp"
#include "hefesto/hints/hints.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/settings/project_settings.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
namespace hints = hefesto::hints;
namespace tech = hefesto::tech;
using db::Rect;
using Rects = std::vector<Rect>;

namespace {

struct Session {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  cmd::ScriptSession script{project};
  cmd::CommandRegistry registry;
  hefesto::editor::EditorState editor;
  std::unique_ptr<cmd::Dispatcher> dispatcher;

  explicit Session(const db::test::MtTech& mt) {
    REQUIRE(db::register_db_commands(registry));
    dispatcher = std::make_unique<cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&editor);
    editor.technology = mt.tech;
    db::CellDef* def = editor.cells.create("top", editor.technology->plane_count()).value();
    def->flags().available = true;
    editor.edit_cell = def;
  }

  cmd::CommandResult run(std::vector<std::string> args) {
    return dispatcher->execute(cmd::Invocation{"property", std::move(args), {}, std::nullopt});
  }

  const hints::HintRegion* hint(std::string_view name) const { return hints::find_hint(*editor.edit_cell, name); }
};

}  // namespace

#define SET "[cap:mask-hints][req:Setting mask hints with the property command]"

TEST_CASE("property sets and queries a hint in lambda units", SET) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  REQUIRE(s.run({"MASKHINTS_SRAM", "0 0 10 10"}).ok());
  REQUIRE(s.hint("SRAM") != nullptr);
  CHECK(s.hint("SRAM")->strips() == Rects{{0, 0, 10, 10}});
  CHECK(s.run({"MASKHINTS_SRAM"}).value.to_string() == "0 0 10 10");
  CHECK(s.editor.edit_cell->flags().modified);
}

TEST_CASE("property hints take one argument or many and ignore an incomplete group", SET) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  REQUIRE(s.run({"MASKHINTS_X", "0 0 10 10 20 20"}).ok());
  CHECK(s.hint("X")->strips() == Rects{{0, 0, 10, 10}});
  REQUIRE(s.run({"MASKHINTS_Y", "0", "0", "10", "10", "5", "5", "15", "15", "1"}).ok());
  CHECK(s.hint("Y")->strips() == Rects{{5, 10, 15, 15}, {0, 5, 15, 10}, {0, 0, 10, 5}});
  CHECK(s.run({"MASKHINTS_Y"}).value.to_string() == "5 10 15 15 0 5 15 10 0 0 10 5");
  // A new value replaces the old one.
  REQUIRE(s.run({"MASKHINTS_Y", "1 1 2 2"}).ok());
  CHECK(s.hint("Y")->strips() == Rects{{1, 1, 2, 2}});
}

TEST_CASE("property forces the hint type whatever keyword is given", SET) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  REQUIRE(s.run({"string", "MASKHINTS_A", "0 0 4 4"}).ok());
  REQUIRE(s.run({"integer", "MASKHINTS_B", "0 0 4 4"}).ok());
  REQUIRE(s.run({"dimension", "MASKHINTS_C", "0", "0", "4", "4"}).ok());
  for (const char* name : {"A", "B", "C"}) {
    INFO(name);
    REQUIRE(s.hint(name) != nullptr);
    CHECK(s.hint(name)->strips() == Rects{{0, 0, 4, 4}});
  }
}

TEST_CASE("property hint values are user-unit coordinates", SET) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  s.editor.grid = tech::GridScale{2, 1};  // 2 internal units per lambda
  REQUIRE(s.run({"MASKHINTS_A", "0 0 5 5"}).ok());
  CHECK(s.hint("A")->strips() == Rects{{0, 0, 10, 10}});
  // Reported values follow `units`: internal units by default (DBWPrintValue).
  CHECK(s.run({"MASKHINTS_A"}).value.to_string() == "0 0 10 10");
  REQUIRE(s.run({"MASKHINTS_B", "1i 1i 3i 3i"}).ok());  // unit suffixes as for any coordinate
  CHECK(s.hint("B")->strips() == Rects{{1, 1, 3, 3}});
}

TEST_CASE("property hint groups with no area paint nothing", SET) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  // DBPaintPlane ignores an inverted rectangle (no canonicalisation).
  REQUIRE(s.run({"MASKHINTS_A", "10 10 0 0 20 20 30 30"}).ok());
  CHECK(s.hint("A")->strips() == Rects{{20, 20, 30, 30}});
  // Fewer than four values: an empty region is stored, as in Magic.
  REQUIRE(s.run({"MASKHINTS_E", "1 2"}).ok());
  REQUIRE(s.hint("E") != nullptr);
  CHECK(s.hint("E")->empty());
  CHECK(s.run({"MASKHINTS_E"}).value.to_string().empty());
}

TEST_CASE("property with an empty hint value removes it", "[cap:mask-hints][req:Mask-hint property data model]") {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  REQUIRE(s.run({"MASKHINTS_A", "0 0 4 4"}).ok());
  REQUIRE(s.hint("A") != nullptr);
  REQUIRE(s.run({"MASKHINTS_A", ""}).ok());
  CHECK(s.editor.edit_cell->properties().find("MASKHINTS_A") == nullptr);
  CHECK(s.hint("A") == nullptr);
  CHECK(s.run({}).value.to_string().find("MASKHINTS_A") == std::string::npos);
  // An empty region (fewer than four values) is removed the same way, and
  // removing an absent hint creates nothing (DBPropPut with NULL).
  REQUIRE(s.run({"MASKHINTS_E", "1 2"}).ok());
  REQUIRE(s.hint("E") != nullptr);
  REQUIRE(s.run({"MASKHINTS_E", ""}).ok());
  CHECK(s.hint("E") == nullptr);
  REQUIRE(s.run({"MASKHINTS_Z", ""}).ok());
  CHECK(s.hint("Z") == nullptr);
}

TEST_CASE("property lists hints as groups of four", SET) {
  HEFESTO_REQUIRE_MT(mt);
  Session s(mt);
  REQUIRE(s.run({"MASKHINTS_X", "0 0 4 4 6 6 8 8"}).ok());
  const std::string all = s.run({}).value.to_string();
  CHECK(all.find("{MASKHINTS_X 6 6 8 8 0 0 4 4}") != std::string::npos);
}
