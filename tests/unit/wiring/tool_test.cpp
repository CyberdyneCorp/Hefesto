// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// wiring-tool "Wiring tool buttons": the wiring tool's default bindings,
// the tracking workflow of `trackwire` (start, preview with `wire show`,
// commit legs, finish or cancel) and the wiring undo events.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/diag/codes.hpp"
#include "hefesto/undo/undo_log.hpp"
#include "hefesto/wiring/wiring.hpp"
#include "wire_fixture.hpp"

namespace wiring = hefesto::wiring;
namespace db = hefesto::db;
using db::Rect;
using wiring::test::last_text;
using wiring::test::Session;

#define BUTTONS "[cap:wiring-tool][req:Wiring tool buttons]"

namespace {

std::string binding(std::string_view key) {
  for (const wiring::ToolBinding& b : wiring::tool_bindings()) {
    if (b.key == key) {
      return std::string{b.text};
    }
  }
  return {};
}

/// Runs a binding's command text (`a ; b`) the way a macro does.
void press(Session& s, std::string_view key) {
  const std::string text = binding(key);
  REQUIRE_FALSE(text.empty());
  std::size_t pos = 0;
  while (pos <= text.size()) {
    const std::size_t end = text.find(';', pos);
    const std::string part = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
    if (part.find_first_not_of(' ') != std::string::npos) {
      (void)s.run(part);
    }
    if (end == std::string::npos) {
      break;
    }
    pos = end + 1;
  }
}

std::vector<Rect> selected(const Session& s, std::string_view layer) {
  const db::CellDef* def = s.editor.selection.def();
  return def == nullptr ? std::vector<Rect>{} : wiring::test::rects_of(*def, *s.editor.technology, s.type(layer));
}

}  // namespace

TEST_CASE("the wiring tool's default button bindings", BUTTONS) {
  CHECK(binding("Button1") == "trackwire pick");
  CHECK(binding("Button2") == "trackwire done");
  CHECK(binding("Button3") == "trackwire cancel");
  CHECK(binding("Control_Button1") == "trackwire current");
  CHECK(binding("Shift_Button1") == "wire incr type ; wire show");
  CHECK(binding("Shift_Button2") == "wire switch");
  CHECK(binding("Shift_Button3") == "wire decr type ; wire show");
  CHECK(binding("Button4") == "wire incr width ; wire show");
  CHECK(binding("Button5") == "wire decr width ; wire show");
  CHECK(wiring::tool_bindings().size() == 9);
}

TEST_CASE("button 1 starts a wire, motion previews, button 1 commits, button 2 finishes", BUTTONS) {
  Session s;
  s.paint("metal1", 0, 0, 50, 4);
  s.point_at(40, 2);
  press(s, "Button1");  // wire type at the cursor, then tracking
  CHECK(s.editor.wire.tracking);
  CHECK(s.editor.wire.type == s.type("metal1"));
  CHECK(s.editor.wire.width == 4);
  CHECK(*s.editor.box == Rect{38, 0, 42, 4});

  s.point_at(80, 2);
  s.ok("wire show");  // what a front end runs on pointer motion while tracking
  CHECK(selected(s, "metal1") == std::vector<Rect>{{38, 0, 82, 4}});
  CHECK(s.rects("metal1") == std::vector<Rect>{{0, 0, 50, 4}});

  press(s, "Button1");  // commits the leg and goes on tracking
  CHECK(s.rects("metal1") == std::vector<Rect>{{0, 0, 82, 4}});
  CHECK(*s.editor.box == Rect{78, 0, 82, 4});
  CHECK(s.editor.wire.tracking);

  s.point_at(80, 30);
  press(s, "Button2");  // commits a last leg and stops tracking
  CHECK(wiring::test::sorted(s.rects("metal1")) == std::vector<Rect>{{0, 0, 82, 4}, {78, 4, 82, 32}});
  CHECK_FALSE(s.editor.wire.tracking);
  CHECK(s.editor.selection.empty());
}

TEST_CASE("button 3 cancels tracking and clears the preview", BUTTONS) {
  Session s;
  s.paint("metal1", 0, 0, 50, 4);
  s.point_at(40, 2);
  press(s, "Button1");
  s.point_at(80, 2);
  s.ok("wire show");
  REQUIRE_FALSE(s.editor.selection.empty());
  press(s, "Button3");
  CHECK_FALSE(s.editor.wire.tracking);
  CHECK(s.editor.selection.empty());
  CHECK(s.rects("metal1") == std::vector<Rect>{{0, 0, 50, 4}});  // nothing committed
  press(s, "Button3");  // not tracking: nothing happens
  CHECK_FALSE(s.editor.wire.tracking);
}

TEST_CASE("control-button 1 starts tracking with the current wire", BUTTONS) {
  Session s;
  s.ok("wire type metal2 6");
  s.point_at(10, 10);
  press(s, "Control_Button1");
  CHECK(s.editor.wire.tracking);
  CHECK(*s.editor.box == Rect{10, 10, 16, 16});
  CHECK(s.editor.wire.type == s.type("metal2"));
  CHECK(s.editor.wire.width == 6);
  s.point_at(40, 13);
  press(s, "Button1");
  CHECK(s.rects("metal2") == std::vector<Rect>{{10, 10, 43, 16}});
}

TEST_CASE("button 2 when not tracking places a contact; shift buttons step type and width", BUTTONS) {
  Session s;
  s.ok("wire type metal1 4");
  s.ok("box 48 0 52 4");
  s.ok("wire horizontal to 80 2");
  s.paint("metal2", 70, -20, 90, 20);
  s.editor.box = Rect{78, 0, 82, 4};
  s.editor.wire.last_direction = wiring::LegDirection::east;
  s.point_at(80, 2);
  press(s, "Shift_Button1");  // wire incr type ; wire show
  CHECK(s.editor.wire.type == s.type("metal2"));
  CHECK(s.rects("m2contact") == std::vector<Rect>{{78, 0, 82, 4}});
  press(s, "Button4");
  CHECK(s.editor.wire.width == 5);
  press(s, "Button5");
  CHECK(s.editor.wire.width == 4);
  press(s, "Shift_Button3");  // back to metal1 through the same contact
  CHECK(s.editor.wire.type == s.type("metal1"));

  Session t;
  t.paint("metal1", 0, 0, 50, 4);
  t.ok("box 46 0 50 4");
  t.ok("wire type metal2 4");
  t.point_at(48, 2);
  press(t, "Button2");  // `wire switch` at the cursor's material
  CHECK(t.editor.wire.type == t.type("metal1"));
  CHECK(t.rects("m2contact") == std::vector<Rect>{{46, 0, 50, 4}});
  CHECK_FALSE(t.editor.wire.tracking);
}

TEST_CASE("trackwire accepts Magic's window word", BUTTONS) {
  Session s;
  s.paint("metal1", 0, 0, 50, 4);
  s.point_at(40, 2);
  s.ok("trackwire .layout0 pick");
  CHECK(s.editor.wire.tracking);
  s.ok("trackwire .layout0 cancel");
  CHECK_FALSE(s.editor.wire.tracking);
  CHECK(last_text(s.run("trackwire a b c")).starts_with("Usage: trackwire"));
}

TEST_CASE("undo restores the wiring state", BUTTONS) {
  Session s;
  s.ok("wire type metal1 4");
  CHECK(s.editor.wire.remembered == wiring::WireParams{s.type("metal1"), 4, wiring::LegDirection::none});
  s.ok("wire type metal2 6");
  CHECK(s.editor.wire.remembered == wiring::WireParams{s.type("metal2"), 6, wiring::LegDirection::none});

  // The event recorded by the second `wire type`, replayed directly.
  const wiring::WireParams before{s.type("metal1"), 4, wiring::LegDirection::none};
  const wiring::WireParams after{s.type("metal2"), 6, wiring::LegDirection::none};
  auto event = wiring::make_wire_event(before, after);
  event->undo(s.editor);
  CHECK(s.editor.wire.type == s.type("metal1"));
  CHECK(s.editor.wire.width == 4);
  CHECK(s.editor.wire.remembered == before);
  event->redo(s.editor);
  CHECK(s.editor.wire.params() == after);
  CHECK(event->describe() == "wiring parameters");

  // Through the undo log.
  s.editor.undo.close_step();
  s.ok("wire type poly 2");
  s.editor.undo.close_step();
  const auto undone = s.editor.undo.undo(s.editor);
  REQUIRE(undone.ok());
  CHECK(s.editor.wire.type == s.type("metal2"));
  CHECK(s.editor.wire.width == 6);
}

TEST_CASE("the first remembered values are Magic's", BUTTONS) {
  const wiring::WireState state;
  CHECK_FALSE(state.type.has_value());
  CHECK(state.width == 0);
  CHECK_FALSE(state.tracking);
  CHECK(state.remembered == wiring::WireParams{std::nullopt, 2, wiring::LegDirection::north});
}
