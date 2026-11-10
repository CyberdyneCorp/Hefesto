// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// drc-engine "Marking changed areas" through the commands: each editing
// path marks Magic's area (design D7), and the [m3] checklist test keeps
// the marking-site table of docs/diagnostics/codes/drc.md (ENGINE section)
// and the sources in step.

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <regex>

#include "engine_fixture.hpp"
#include "hefesto/cell/commands.hpp"
#include "hefesto/cmd/dispatcher.hpp"
#include "hefesto/cmd/registry.hpp"
#include "hefesto/db/commands.hpp"
#include "hefesto/platform/services.hpp"
#include "hefesto/select/commands.hpp"
#include "hefesto/settings/project_settings.hpp"
#include "hefesto/undo/commands.hpp"

namespace db = hefesto::db;
namespace drc = hefesto::drc;
namespace tech = hefesto::tech;
using drc::ErrorTile;

namespace {

struct CommandSession {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  hefesto::cmd::ScriptSession script{project};
  hefesto::cmd::CommandRegistry registry;
  drc::test::Session s;
  std::unique_ptr<hefesto::cmd::Dispatcher> dispatcher;
  db::CellDef* top = nullptr;

  CommandSession() {
    REQUIRE(db::register_db_commands(registry));
    REQUIRE(hefesto::cell::register_cell_commands(registry));
    REQUIRE(hefesto::select::register_select_commands(registry));
    REQUIRE(hefesto::undo::register_undo_commands(registry));
    dispatcher = std::make_unique<hefesto::cmd::Dispatcher>(registry, services.view(), project, script);
    dispatcher->set_editor(&s.ed);
    s.set_rules(s.rules(1, 1000).build());
    top = &s.cell("top");
    s.ed.edit_cell = top;
    ok("snap internal");
  }

  hefesto::cmd::CommandResult run(const std::string& line) {
    hefesto::cmd::Invocation inv;
    std::size_t pos = 0;
    while (pos < line.size()) {
      const std::size_t end = line.find(' ', pos);
      const std::string tok = line.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
      if (!tok.empty()) {
        (inv.name.empty() ? inv.name : inv.args.emplace_back()) = tok;
      }
      pos = end == std::string::npos ? line.size() : end + 1;
    }
    return dispatcher->execute(inv);
  }

  void ok(const std::string& line) {
    const auto r = run(line);
    INFO(line << " -> " << (r.diagnostics.empty() ? std::string{} : r.diagnostics.back().text));
    REQUIRE(r.ok());
  }

  /// Forgets every mark (check planes and the pending list).
  void reset_marks() {
    s.ed.drc.pending.clear();
    s.ed.cells.for_each([](db::CellDef& d) {
      (void)d.plane(tech::kPlaneDrcCheck).paint(db::whole_plane(), drc::identity_paint_row(tech::kSpace));
    });
  }

  [[nodiscard]] std::vector<ErrorTile> marks() const { return drc::test::checks(*top); }
};

std::vector<ErrorTile> mark(const db::Rect& r) {
  return {ErrorTile{r, tech::kCheckPaint}};
}

}  // namespace

TEST_CASE("paint, erase, fill and their DRC marks", "[cap:drc-engine][req:Marking changed areas]") {
  CommandSession c;
  c.ok("box values 0 0 10 10");

  SECTION("paint marks the box expanded by the halo") {
    c.ok("paint poly");
    CHECK(c.marks() == mark({-1, -1, 11, 11}));
    REQUIRE(c.s.ed.drc.pending.size() == 1);
    CHECK(c.s.ed.drc.pending.front() == c.top);
  }
  SECTION("paint does not mark while background DRC is off (CmdPaint's DRCBackGround test)") {
    c.s.ed.drc.background = drc::BackgroundMode::off;
    c.ok("paint poly");
    CHECK(c.marks().empty());
    c.ok("erase poly");  // erase marks regardless
    CHECK(c.marks() == mark({-1, -1, 11, 11}));
  }
  SECTION("erasing only labels marks nothing") {
    c.ok("erase labels");
    CHECK(c.marks().empty());
  }
  SECTION("erasing subcells marks each removed use's area") {
    db::CellDef& child = c.s.cell("child");
    c.s.paint(child, "metal1", db::Rect{0, 0, 4, 4});
    c.s.place(*c.top, child, db::translation(20, 0));
    c.ok("box values 18 0 22 4");
    c.ok("erase subcell");
    CHECK(c.top->uses().empty());
    CHECK(c.marks() == mark({17, -1, 25, 5}));
  }
  SECTION("undo and redo mark each replayed rectangle") {
    c.ok("paint poly");
    c.reset_marks();
    c.ok("undo");
    CHECK(c.marks() == mark({-1, -1, 11, 11}));
    c.reset_marks();
    c.ok("redo");
    CHECK(c.marks() == mark({-1, -1, 11, 11}));
  }
}

TEST_CASE("selection edits and their DRC marks", "[cap:drc-engine][req:Marking changed areas]") {
  CommandSession c;
  c.ok("box values 0 0 10 10");
  c.ok("paint metal1");
  c.ok("select area metal1");
  c.reset_marks();

  SECTION("delete marks the selection's extended area") {
    c.ok("delete");
    CHECK(c.marks() == mark({-1, -1, 11, 11}));
  }
  SECTION("move marks the old and the new area") {
    c.ok("move e 20");
    CHECK(c.marks() == std::vector<ErrorTile>{{{-1, -1, 11, 11}, tech::kCheckPaint}, {{19, -1, 31, 11}, tech::kCheckPaint}});
  }
  SECTION("copy marks the copy's area") {
    c.ok("copy n 30");
    CHECK(c.marks() == mark({-1, 29, 11, 41}));
  }
  SECTION("stretch marks everything between the old and new selection") {
    c.ok("stretch e 5");
    CHECK(c.marks() == mark({-1, -1, 16, 11}));
  }
}

TEST_CASE("The marking-site checklist matches the sources", "[cap:drc-engine][req:Marking changed areas][m3]") {
  const std::filesystem::path root{HEFESTO_SOURCE_DIR};
  const std::string doc = drc::test::read_text(root / "docs/diagnostics/codes/drc.md");
  const auto begin = doc.find("### Marking sites");
  REQUIRE(begin != std::string::npos);
  const std::string table = doc.substr(begin, doc.find("\n## ", begin) - begin);
  // Rows: | Magic call site | Hefesto site | Area |; a Hefesto site is a
  // `path` (with an optional `::function`) or a milestone ("M5", ...).
  const std::regex row(R"(\n\| `([^`]+)` \| ([^|]+) \| ([^|]+) \|)");
  std::size_t sites = 0;
  for (auto it = std::sregex_iterator(table.begin(), table.end(), row); it != std::sregex_iterator(); ++it) {
    const std::string hefesto = (*it)[2].str();
    std::smatch path;
    if (!std::regex_search(hefesto, path, std::regex(R"(`(src/core/src/[^`:]+))"))) {
      // a later milestone, or an open item of this milestone
      CHECK((std::regex_search(hefesto, std::regex(R"(M\d+)")) || hefesto.starts_with("open item")));
      continue;
    }
    ++sites;
    const std::string source = drc::test::read_text(root / path[1].str());
    INFO((*it)[1].str() << " -> " << path[1].str());
    CHECK((source.find("check_this(") != std::string::npos || source.find("paint_check_area(") != std::string::npos));
  }
  CHECK(sites >= 25);
}

TEST_CASE("A deleted cell leaves the pending list", "[cap:drc-engine][req:Pending list and background scheduling]") {
  CommandSession c;
  db::CellDef& gone = c.s.cell("gone");
  drc::check_this(c.s.ed, gone, db::Rect{0, 0, 4, 4});
  REQUIRE(c.s.ed.drc.pending.contains(gone));
  c.ok("cellname delete gone");
  CHECK(c.s.ed.cells.find("gone") == nullptr);
  CHECK(c.s.ed.drc.pending.empty());
  REQUIRE(drc::catchup(c.s.ed).ok());
}

TEST_CASE("Error tiles stay out of selection and copies", "[cap:drc-engine][req:DRC check and error planes]") {
  CommandSession c;
  c.ok("box values 0 0 10 10");
  c.ok("paint metal1");
  REQUIRE(c.top->plane(tech::kPlaneDrcError).paint(db::Rect{0, 0, 10, 10}, drc::error_paint_row(tech::kErrorP)).ok());
  c.ok("select area");
  CHECK(drc::test::errors(*c.s.ed.selection.def()).empty());
  c.ok("copy n 30");
  CHECK(drc::test::errors(*c.top) == std::vector<ErrorTile>{{{0, 0, 10, 10}, tech::kErrorP}});
}
