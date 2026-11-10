// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cmd-box: the `box` command and the box cursor forms.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "edit_session.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"
#include "hefesto/db/transform.hpp"

namespace db = hefesto::db;
using db::Rect;
using db::test::EditSession;

#define CAP "[cap:cmd-box]"

namespace {

struct Box : EditSession {
  explicit Box(const db::test::MtTech& mt) : EditSession(mt) { ok("units internal"); }
};

}  // namespace

TEST_CASE("box changes are undoable events", CAP "[req:Box state]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  s.ok("box values 0 0 10 10");
  s.editor.undo.close_step();
  s.ok("box values 5 5 20 20");
  s.editor.undo.close_step();
  REQUIRE(s.editor.undo.undo(s.editor).ok());
  CHECK(s.box() == Rect{0, 0, 10, 10});
  (void)s.editor.undo.redo(s.editor);
  CHECK(s.box() == Rect{5, 5, 20, 20});
}

TEST_CASE("box option syntax", CAP "[req:Box command syntax]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession s(mt);
  s.ok("box values 0i 0i 4i 6i");
  CHECK(s.value("box h") == "6");  // `h` means height
  CHECK(s.value("box he") == "6");
  const auto help = s.ok("box values 1 2 3 4 5 6");
  CHECK(EditSession::texts(help).find("set or return box width") != std::string::npos);
  CHECK(s.box() == Rect{0, 0, 4, 6});
  s.ok("box width 1 2");  // wrong count: help, no change
  CHECK(s.box() == Rect{0, 0, 4, 6});
}

TEST_CASE("box queries", CAP "[req:Box queries]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession s(mt);
  CHECK(s.value("box exists") == "0");
  CHECK(s.run("box width").diagnostics.back().text == "Box tool must be present");
  s.ok("box values -2i 0i 8i 5i");
  CHECK(s.value("box values") == "-2 0 8 5");
  CHECK(s.value("box size") == "10 5");
  CHECK(s.value("box position") == "-2 0");
  CHECK(s.value("box width") == "10");
  CHECK(s.value("box exists") == "1");
}

TEST_CASE("box setters", CAP "[req:Box size and position setters]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  s.ok("box values 10 10 20 30");
  s.ok("box width 5");
  CHECK(s.box() == Rect{10, 10, 15, 30});
  s.ok("box values 10 10 20 30");
  s.ok("box height -4");
  CHECK(s.box() == Rect{10, 6, 20, 10});
  s.ok("box values 10 10 20 30");
  s.ok("box position 0 -5");
  CHECK(s.box() == Rect{0, -5, 10, 15});
  s.ok("box size 7 8");
  CHECK(s.box() == Rect{0, -5, 7, 3});
}

TEST_CASE("box select takes the selection bounding box", CAP "[req:Box size and position setters]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  s.ok("box values 0 0 1 1");
  s.editor.selection.set_root(s.editor.edit_cell, mt.tech->plane_count());
  REQUIRE(db::paint(*s.editor.selection.def(), *mt.tech, Rect{3, 4, 9, 12}, mt.m1).ok());
  db::recompute_bbox(*s.editor.selection.def());
  s.ok("box select");
  CHECK(s.box() == Rect{3, 4, 9, 12});
}

TEST_CASE("four numbers set the box and print the report", CAP "[req:Setting the box directly]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  const auto r = s.ok("box 0 0 10 20");
  CHECK(s.box() == Rect{0, 0, 10, 20});
  CHECK(EditSession::texts(r).find("Root cell box:") != std::string::npos);
}

TEST_CASE("box move, grow and shrink", CAP "[req:Box move, grow and shrink]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  s.ok("box values 10 10 20 20");
  s.ok("box grow center 2");
  CHECK(s.box() == Rect{8, 8, 22, 22});
  s.ok("box values 0 0 4 4");
  s.ok("box shrink n 6");
  CHECK(s.box() == Rect{0, -2, 4, 0});
  s.ok("box move ne 3");
  CHECK(s.box() == Rect{3, 1, 7, 3});
  s.ok("box grow sw 1");
  CHECK(s.box() == Rect{2, 0, 7, 3});
  s.ok("box shrink center 1");
  CHECK(s.box() == Rect{3, 1, 6, 2});
  const auto none = s.ok("box move n 0");
  CHECK(none.diagnostics.empty());
  CHECK(s.box() == Rect{3, 1, 6, 2});
  CHECK_FALSE(s.run("box move zz 3").ok());
}

TEST_CASE("box move by lambda", CAP "[req:Box move, grow and shrink]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession s(mt);
  s.editor.grid.internal = 2;
  s.ok("box values 0i 0i 4i 4i");
  s.ok("box move e 3");
  CHECK(s.box() == Rect{6, 0, 10, 4});
}

TEST_CASE("box cursor forms use the snapped pointer", CAP "[req:Box cursor forms]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  auto w = s.editor.windows.open_frame(s.editor.edit_cell);
  REQUIRE(w.ok());
  s.ok("snap internal");
  s.ok("box values 0 0 10 10");
  s.editor.command_point = hefesto::cmd::PointerState{w.value()->id(), db::Point{30, 40}, Rect{30, 40, 30, 40}};
  s.ok("box move ne cursor");
  CHECK(s.box() == Rect{20, 30, 30, 40});
  s.ok("box values 0 0 10 10");
  s.ok("box grow ne cursor");  // Q-CMDEDIT-11: no change
  CHECK(s.box() == Rect{0, 0, 10, 10});
  s.ok("box corner ne cursor");
  CHECK(s.box() == Rect{0, 0, 30, 40});
  s.ok("box values 0 0 10 10");
  s.ok("box move n cursor");  // Q-CMDEDIT-10: Manhattan acts as sw
  CHECK(s.box() == Rect{30, 40, 40, 50});
  s.editor.command_point.reset();
  CHECK_FALSE(s.run("box move ne cursor").ok());
}

TEST_CASE("box corner moves one corner to a point", CAP "[req:Box corner command]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  s.ok("box values 0 0 10 10");
  s.ok("box corner ne 20 30");
  CHECK(s.box() == Rect{0, 0, 20, 30});
  s.ok("box values 0 0 10 10");
  s.ok("box corner ne -5 4");
  CHECK(s.box() == Rect{-5, 0, 0, 4});
  s.ok("box values 0 0 10 10");
  const auto r = s.ok("box corner ne 5");
  CHECK(EditSession::texts(r).find("Usage: box corner <direction> <x> <y> | box corner <direction> cursor") !=
        std::string::npos);
  CHECK(s.box() == Rect{0, 0, 10, 10});
}

TEST_CASE("default box report", CAP "[req:Default box report]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  s.ok("box values 0 0 100 50");
  const std::string report = EditSession::texts(s.ok("box"));
  CHECK(report.find("Root cell box:") != std::string::npos);
  CHECK(report.find("area (units^2)") != std::string::npos);
  CHECK(report.find("\nlambda:      100 x 50  ") != std::string::npos);  // L0 = L1: one integer line
  CHECK(report.find("  5000") != std::string::npos);
  CHECK(report.find("microns:  100.000 x 50.000") != std::string::npos);  // no output style: scale 1
  s.ok("box values 0 0 10 10");
  s.ok("box north 5");  // Q-CMDEDIT-8: an unknown first word only reports
  CHECK(s.box() == Rect{0, 0, 10, 10});
  s.editor.grid.internal = 2;
  const std::string two = EditSession::texts(s.ok("box"));
  CHECK(two.find("\ninternal:") != std::string::npos);
}

TEST_CASE("the edit switch reads and reports in edit coordinates", CAP "[req:Edit-coordinate switch]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession s(mt);
  db::CellDef* top = s.editor.edit_cell;
  db::CellDef* child = s.cell("child");
  s.editor.edit_root = top;
  s.editor.edit_cell = child;
  s.editor.edit_transform = db::Transform{0, 1, 0, -1, 0, 0};  // 90 degrees
  s.ok("box values 0i 0i 10i 20i");
  CHECK(s.value("box width -edit") == "20");
  s.ok("box values 0i 0i 5i 5i -edit");
  CHECK(s.box() == db::apply(s.editor.edit_transform, Rect{0, 0, 5, 5}));
}

TEST_CASE("box bounds are checked on both corners", CAP "[req:Box normalization and bounds]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  s.ok("box values 0 0 10 10");
  auto r = s.run("box position 2000000000i 0");
  CHECK(r.diagnostics.back().text == "Box out of bounds.");
  CHECK(s.box() == Rect{0, 0, 10, 10});
  r = s.run("box values 0 0 2000000000i 1");
  CHECK(r.diagnostics.back().code == db::codes::kBoxOutOfBounds);
  CHECK(s.box() == Rect{0, 0, 10, 10});
}

TEST_CASE("box remove and help", CAP "[req:Box remove and help]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  s.ok("box values 0 0 10 10");
  s.ok("box remove");
  CHECK_FALSE(s.editor.box.has_value());
  CHECK(s.run("box width").diagnostics.back().text == "Box tool must be present");
  const std::string help = EditSession::texts(s.ok("box help"));
  CHECK(help.find("move\t\t<direction> <distance>") != std::string::npos);
}

TEST_CASE("corner dragging past the opposite side", CAP "[req:Box corner dragging]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  auto w = s.editor.windows.open_frame(s.editor.edit_cell);
  REQUIRE(w.ok());
  s.ok("snap internal");
  s.ok("box values 0 0 10 10");
  s.editor.command_point = hefesto::cmd::PointerState{w.value()->id(), db::Point{-5, 4}, Rect{}};
  s.ok("box corner ne cursor");
  CHECK(s.box() == Rect{-5, 0, 0, 4});
  // A point in a window on another root moves the whole box there.
  db::CellDef* other = s.cell("other");
  auto w2 = s.editor.windows.open_frame(other);
  REQUIRE(w2.ok());
  s.editor.command_point = hefesto::cmd::PointerState{w2.value()->id(), db::Point{100, 100}, Rect{}};
  s.ok("box corner ne cursor");
  CHECK(s.box() == Rect{95, 96, 100, 100});
  CHECK(s.editor.box_root == other);
}

TEST_CASE("pointer button gestures issue box commands", CAP "[req:Box tool pointer gestures]") {
  // The core part of the gestures: the left button is `box move bl cursor`,
  // the right button `box corner tr cursor` (DISPATCH binds the buttons).
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  auto w = s.editor.windows.open_frame(s.editor.edit_cell);
  REQUIRE(w.ok());
  s.ok("snap internal");
  s.ok("box values 0 0 10 10");
  s.editor.command_point = hefesto::cmd::PointerState{w.value()->id(), db::Point{100, 50}, Rect{}};
  s.ok("box move bl cursor");
  CHECK(s.box() == Rect{100, 50, 110, 60});
  s.ok("box values 0 0 10 10");
  s.editor.command_point = hefesto::cmd::PointerState{w.value()->id(), db::Point{30, 40}, Rect{}};
  s.ok("box corner tr cursor");
  CHECK(s.box() == Rect{0, 0, 30, 40});
}

TEST_CASE("the box is usable off-screen", CAP "[req:Box availability to other commands]") {
  HEFESTO_REQUIRE_MT(mt);
  Box s(mt);
  REQUIRE(s.editor.windows.open_frame(s.editor.edit_cell).ok());
  s.ok("box values 1000000 1000000 1000010 1000010");
  s.ok("paint metal1");
  CHECK(db::test::rects_of(*s.editor.edit_cell, mt.metal1, mt.m1) == std::vector<Rect>{{1000000, 1000000, 1000010,
                                                                                       1000010}});
}
