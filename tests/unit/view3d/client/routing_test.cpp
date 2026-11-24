// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// N1 (Q-MISC-82, approved 2026-11-22): the window a script command with no
// window path goes to once a 3D window is open. Magic's _tk_dispatch sets
// the current window (TxSetPoint) for every later command, so after a
// command on the 3D window's path WindSendCommand sends `box`, `load` or
// `cif` to the 3D window (MISC-3D-05, MISC-3D-10). Owner: WIND3D.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "client_fixture.hpp"

namespace {

using hefesto::db::Rect;
using hefesto::view3d::current_factor;
using hefesto::view3d::test::ClientSession;

// Opens the window at .render.magic and sends it one command, as the
// MISC-3D cases do.
void open_and_address(ClientSession& s) {
  s.ok("specialopen wind3d c3d .render.magic");
  CHECK(s.value(".render.magic cutbox none").empty());
  s.take_printed();
}

}  // namespace

TEST_CASE("script commands go to the open 3D window with magic-compat ON", "[cap:view-3d][req:3D view commands]") {
  ClientSession s(true);
  open_and_address(s);
  // `box` is no command of the 3D window: a quiet error with an empty
  // result, the box unchanged (MISC-3D-05 `1 {}`).
  const auto box = s.eval("box 20 10 70 45");
  REQUIRE_FALSE(box.ok());
  CHECK(box.diagnostics.front().text.empty());
  CHECK(s.editor.box == Rect{0, 0, 100, 50});
  CHECK(s.take_printed().empty());
  // `cif ostyle coarse` reaches the 3D window's `cif` (MISC-3D-10).
  CHECK(s.value("cif ostyle coarse").empty());
  CHECK(s.take_printed().find("Usage: cif") != std::string::npos);
  CHECK(current_factor(s.editor) == 10.0F);
  // A window command still runs, and the 3D window's `view` wins over the
  // window table's.
  CHECK(s.value("view") == "0.0 0.0 0.0");
  CHECK(s.value("level") == "1");
  s.take_printed();
  static_cast<void>(s.eval("specialopen wind3d c3d .other"));
  CHECK(s.take_printed().find("Only one 3D window allowed.") != std::string::npos);
}

TEST_CASE("script commands stay in the layout window with magic-compat OFF", "[cap:view-3d][req:3D view commands]") {
  ClientSession s(false);
  open_and_address(s);
  CHECK(s.eval("box 20 10 70 45").ok());
  CHECK(s.editor.box == Rect{20, 10, 70, 45});
  CHECK(s.eval("cif ostyle coarse").ok());
  CHECK(current_factor(s.editor) == 5.0F);
  CHECK(s.take_printed().find("Usage: cif") == std::string::npos);
}

TEST_CASE("routing to the 3D window starts with a command on its path and follows a reopened window",
          "[cap:view-3d][req:3D view commands]") {
  ClientSession s(true);
  s.ok("specialopen wind3d c3d .render.magic");
  // Opening alone does not make it the current window (no TxSetPoint).
  CHECK(s.eval("box 1 1 5 5").ok());
  CHECK(s.value(".render.magic level") == "1");
  CHECK_FALSE(s.eval("box 2 2 6 6").ok());
  // Closed: the stale window id names no window, so the layout client runs it.
  CHECK(s.eval(".render.magic closewindow").ok());
  CHECK(s.eval("box 2 2 6 6").ok());
  CHECK(s.editor.box == Rect{2, 2, 6, 6});
  // A new 3D window reuses the freed id (WindCreate takes the lowest one).
  CHECK(s.eval("specialopen wind3d c3d .render.magic").ok());
  CHECK_FALSE(s.eval("box 3 3 7 7").ok());
  // A command on a layout window's path makes that window current.
  s.value("::hefesto::tk::window_command .layout1");
  static_cast<void>(s.eval(".layout1 box values"));
  CHECK(s.eval("box 3 3 7 7").ok());
  CHECK(s.editor.box == Rect{3, 3, 7, 7});
}

// Review of PR #86 (finding 8; Magic behaviour no Q row covered, standing
// rule): `setpoint x y <window>` makes the named window current
// (windCmdSZ.c:286 TxSetPoint), so naming the 3D window's path routes later
// script commands there with ON, and naming another window routes them back.
TEST_CASE("setpoint naming a window switches where script commands go with magic-compat ON",
          "[cap:view-3d][req:3D view commands]") {
  ClientSession s(true);
  s.ok("specialopen wind3d c3d .render.magic");
  CHECK(s.eval("box 1 1 5 5").ok());
  CHECK(s.eval("setpoint 10 10 .render.magic").ok());
  CHECK_FALSE(s.eval("box 2 2 6 6").ok());
  CHECK(s.editor.box == Rect{1, 1, 5, 5});
  // setpoint is a window command, so it still runs while routed to the 3D window.
  CHECK(s.eval("setpoint 10 10 .layout1").ok());
  CHECK(s.eval("box 2 2 6 6").ok());
  CHECK(s.editor.box == Rect{2, 2, 6, 6});
  // Without a window argument the current window stays current.
  CHECK(s.eval("setpoint 10 10 .render.magic").ok());
  CHECK(s.eval("setpoint 10 10").ok());
  CHECK_FALSE(s.eval("box 3 3 7 7").ok());
}

TEST_CASE("setpoint naming the 3D window leaves script commands in the layout window with magic-compat OFF",
          "[cap:view-3d][req:3D view commands]") {
  ClientSession s(false);
  s.ok("specialopen wind3d c3d .render.magic");
  CHECK(s.eval("setpoint 10 10 .render.magic").ok());
  CHECK(s.eval("box 2 2 6 6").ok());
  CHECK(s.editor.box == Rect{2, 2, 6, 6});
}
