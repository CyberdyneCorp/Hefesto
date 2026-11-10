// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The `undo` and `redo` commands through the dispatcher (capability
// layout-undo "undo command", "redo command"), scripted like the MODEL
// golden cases MODEL-G-040, 058 and 059.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/undo/diag_codes.hpp"
#include "undo_fixture.hpp"

namespace db = hefesto::db;
namespace undo = hefesto::undo;
using db::Rect;
using hefesto::undo::test::texts;
using hefesto::undo::test::UndoSession;

namespace {

// The S1, S2, S3 paints of the spec scenarios.
void three_paints(UndoSession& s) {
  s.paint("metal1", 0, 0, 10, 10);
  s.paint("metal1", 20, 0, 30, 10);
  s.paint("metal1", 40, 0, 50, 10);
}

std::string failure(const hefesto::cmd::CommandResult& r, std::string_view code) {
  REQUIRE_FALSE(r.ok());
  REQUIRE_FALSE(r.diagnostics.empty());
  CHECK(r.diagnostics.back().code == code);
  return r.diagnostics.back().text;
}

}  // namespace

TEST_CASE("undo with a count undoes that many steps", "[cap:layout-undo][req:undo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  three_paints(s);
  const auto r = s.run("undo 2");
  REQUIRE(r.ok());
  CHECK(r.diagnostics.empty());
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{0, 0, 10, 10}});
  s.ok("undo +1");  // a sign is part of an integer
  CHECK(s.rects(mt.metal1).empty());
}

TEST_CASE("undo past the start of the log", "[cap:layout-undo][req:undo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.paint("metal1", 0, 0, 10, 10);
  const auto first = s.run("undo 5");
  REQUIRE(first.ok());
  CHECK(first.diagnostics.empty());
  CHECK(s.rects(mt.metal1).empty());
  const auto again = s.run("undo");
  REQUIRE(again.ok());
  CHECK(texts(again) == "Nothing more to undo");
  CHECK(again.diagnostics.back().code == undo::codes::kReport);
}

TEST_CASE("undo rejects bad arguments without changing anything", "[cap:layout-undo][req:undo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.paint("metal1", 0, 0, 10, 10);
  CHECK(failure(s.run("undo -1"), undo::codes::kBadUsage) == "Count must be a positive integer");
  CHECK(failure(s.run("undo two"), undo::codes::kBadUsage) == "Option must be a count (integer)");
  CHECK(failure(s.run("undo enab"), undo::codes::kBadUsage) == "Option must be a count (integer)");
  CHECK(failure(s.run("undo list 3"), undo::codes::kBadUsage) == "Usage: undo print count");
  CHECK(failure(s.run("undo print x"), undo::codes::kBadUsage) == "Usage: undo print count");
  CHECK(failure(s.run("undo 1 2 3"), undo::codes::kBadUsage) ==
        "Usage: undo [count]\n       undo print [count]\n       undo enable|disable");
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{0, 0, 10, 10}});
  CHECK(s.editor.undo.position() == 1);
}

TEST_CASE("undo disable and enable nest", "[cap:layout-undo][req:undo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.ok("undo disable");
  s.ok("undo disable");
  s.ok("undo enable");
  s.paint("metal1", 0, 0, 10, 10);  // not recorded
  s.ok("undo enable");
  s.paint("metal2", 0, 0, 10, 10);  // recorded
  CHECK(s.editor.undo.step_count() == 1);
  s.ok("undo enable");  // never below zero
  CHECK(s.editor.undo.suspend_depth() == 0);
  s.ok("undo");
  CHECK(s.rects(mt.metal2).empty());
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{0, 0, 10, 10}});
}

TEST_CASE("undo while recording is disabled replays nothing", "[cap:layout-undo][req:undo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.paint("metal1", 0, 0, 10, 10);
  s.ok("undo disable");
  const auto r = s.run("undo");
  REQUIRE(r.ok());
  REQUIRE(r.diagnostics.size() == 2);
  CHECK(r.diagnostics[0].code == undo::codes::kUndoDisabled);
  CHECK(r.diagnostics[0].text == "Attempted undo with undo disabled. . . abort function.");
  CHECK(r.diagnostics[1].text == "Nothing more to undo");
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{0, 0, 10, 10}});
}

TEST_CASE("undo 0 resumes recording with magic-compat on", "[cap:layout-undo][req:undo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.ok("undo disable");
  const auto r = s.run("undo 0");
  REQUIRE(r.ok());
  CHECK(r.diagnostics.empty());  // silently
  CHECK_FALSE(s.editor.undo.suspended());
}

TEST_CASE("undo print lists the log without changing it", "[cap:layout-undo][req:undo command][req:redo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.paint("metal1", 0, 0, 10, 10);
  s.paint("metal2", 0, 0, 10, 10);
  const auto r = s.run("undo print 0");
  REQUIRE(r.ok());
  CHECK(texts(r) ==
        "(delimiter)\npaint top: 1 tile change(s)\n(delimiter)\npaint top: 1 tile change(s)");
  CHECK(s.editor.undo.position() == 2);
  CHECK(s.run("undo printing 1").ok());  // the first five characters are compared
  s.ok("undo");
  const auto after = s.run("redo print 1");
  REQUIRE(after.ok());
  CHECK(texts(after) == "paint top: 1 tile change(s)");
  CHECK(s.run("redo print -4").ok());
  CHECK(failure(s.run("redo prin 1"), undo::codes::kBadUsage) == "Usage: redo print count");
  CHECK(failure(s.run("redo print x"), undo::codes::kBadUsage) == "Usage: redo print count");
}

TEST_CASE("redo with nothing undone", "[cap:layout-undo][req:redo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.paint("metal1", 0, 0, 10, 10);
  const auto r = s.run("redo");
  REQUIRE(r.ok());
  CHECK(texts(r) == "Nothing more to redo");
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{0, 0, 10, 10}});
}

TEST_CASE("redo after undo, and a new edit discards the redo tail (MODEL-G-058)",
          "[cap:layout-undo][req:redo command][req:Undo and redo semantics]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  three_paints(s);
  s.ok("undo");
  s.ok("undo");
  s.ok("redo");
  CHECK(s.rects(mt.metal1).size() == 2);
  s.ok("undo");
  s.paint("metal2", 0, 0, 10, 10);
  CHECK(texts(s.run("redo")) == "Nothing more to redo");
  s.ok("undo 2");
  CHECK(s.rects(mt.metal1).empty());
  CHECK(s.rects(mt.metal2).empty());
  CHECK(texts(s.run("undo")) == "Nothing more to undo");
}

TEST_CASE("redo rejects bad arguments", "[cap:layout-undo][req:redo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.paint("metal1", 0, 0, 10, 10);
  s.ok("undo");
  CHECK(failure(s.run("redo -2"), undo::codes::kBadUsage) == "Count must be a positive integer");
  CHECK(failure(s.run("redo x"), undo::codes::kBadUsage) == "Count must be numeric");
  CHECK(failure(s.run("redo enable"), undo::codes::kBadUsage) == "Count must be numeric");
  CHECK(failure(s.run("redo 1 2 3"), undo::codes::kBadUsage) == "Usage: redo [count]");
  CHECK(s.rects(mt.metal1).empty());
  s.ok("redo 3");
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{0, 0, 10, 10}});
}

TEST_CASE("redo 0 suspends recording with magic-compat on", "[cap:layout-undo][req:redo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  const auto r = s.run("redo 0");
  REQUIRE(r.ok());
  CHECK(r.diagnostics.empty());
  s.paint("metal1", 0, 0, 10, 10);
  CHECK(s.editor.undo.step_count() == 0);
  s.ok("undo 0");
  s.paint("metal2", 0, 0, 10, 10);
  CHECK(s.editor.undo.step_count() == 1);
}

TEST_CASE("undo 0 and redo 0 are no-ops with magic-compat off",
          "[cap:layout-undo][req:undo command][req:redo command]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  s.project.compat.magic_compat = false;
  s.paint("metal1", 0, 0, 10, 10);
  const auto redo0 = s.run("redo 0");
  REQUIRE(redo0.ok());
  CHECK(redo0.diagnostics.empty());
  s.paint("metal2", 0, 0, 10, 10);
  CHECK(s.editor.undo.step_count() == 2);
  s.ok("undo disable");
  const auto undo0 = s.run("undo 0");
  REQUIRE(undo0.ok());
  CHECK(undo0.diagnostics.empty());
  CHECK(s.editor.undo.suspended());  // undo 0 did not resume either
  s.ok("undo enable");
  CHECK(s.editor.undo.step_count() == 2);
  CHECK(s.editor.undo.position() == 2);
}

TEST_CASE("1001 paints keep the last 1000 undoable (MODEL-G-059)", "[cap:layout-undo][req:History limit]") {
  HEFESTO_REQUIRE_MT(mt);
  UndoSession s(mt);
  for (db::Coord i = 0; i < 1001; ++i) {
    s.paint("metal1", i * 4, 0, i * 4 + 2, 2);
  }
  const auto r = s.run("undo 1001");
  REQUIRE(r.ok());
  CHECK(r.diagnostics.empty());
  CHECK(s.rects(mt.metal1) == std::vector<Rect>{{0, 0, 2, 2}});
  CHECK(texts(s.run("undo")) == "Nothing more to undo");
}

TEST_CASE("undo and redo need an editor session", "[cap:layout-undo][req:undo command][req:redo command]") {
  hefesto::platform::PortableServices services{hefesto::platform::PortableOptions{{}, {}, true, true}};
  hefesto::settings::ProjectSettings project;
  hefesto::cmd::ScriptSession script{project};
  hefesto::cmd::CommandRegistry registry;
  REQUIRE(undo::register_undo_commands(registry));
  CHECK_FALSE(undo::register_undo_commands(registry));  // names taken
  hefesto::cmd::Dispatcher dispatcher(registry, services.view(), project, script);
  hefesto::cmd::Invocation inv;
  inv.name = "undo";
  CHECK_FALSE(dispatcher.execute(inv).ok());
  inv.name = "redo";
  CHECK_FALSE(dispatcher.execute(inv).ok());
}
