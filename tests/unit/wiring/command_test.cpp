// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// wiring-tool "wire command dispatch", "Wire type selection", "Wire legs",
// "Contact placement (wire switch, increment, decrement)" and "wire show,
// values and width", through the dispatcher, with exact painted geometry.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "hefesto/wiring/diag_codes.hpp"
#include "wire_fixture.hpp"

namespace wiring = hefesto::wiring;
namespace db = hefesto::db;
namespace cmd = hefesto::cmd;
using db::Rect;
using wiring::test::all_text;
using wiring::test::last_text;
using wiring::test::Session;

namespace {

/// The paint of `layer` in the selection.
std::vector<Rect> selected(const Session& s, std::string_view layer) {
  const db::CellDef* def = s.editor.selection.def();
  if (def == nullptr) {
    return {};
  }
  return wiring::test::rects_of(*def, *s.editor.technology, s.type(layer));
}

/// metal1 of width 4 ending in an east leg whose tip box is (78,0)-(82,4).
void east_leg(Session& s) {
  s.ok("wire type metal1 4");
  s.ok("box 48 0 52 4");
  s.ok("wire horizontal to 80 2");
  REQUIRE(*s.editor.box == Rect{78, 0, 82, 4});
}

}  // namespace

#define DISPATCH "[cap:wiring-tool][req:wire command dispatch]"
#define TYPE "[cap:wiring-tool][req:Wire type selection]"
#define LEGS "[cap:wiring-tool][req:Wire legs]"
#define CONTACT "[cap:wiring-tool][req:Contact placement (wire switch, increment, decrement)]"
#define SHOW "[cap:wiring-tool][req:wire show, values and width]"

TEST_CASE("wire without an option prints the help list", DISPATCH) {
  Session s;
  const auto r = s.run("wire");
  REQUIRE(r.ok());
  REQUIRE(r.diagnostics.size() == 1);
  const std::string& help = r.diagnostics[0].text;
  CHECK(r.diagnostics[0].code == wiring::codes::kReport);
  CHECK(help.starts_with("Wiring commands have the form \":wire option\", where option is one of:\n"
                         "    decrement layer|width \tdecrement the wire layer or width\n"
                         "    help                   print this help information\n"));
  CHECK(help.ends_with("\n    segment layer width x1 y1 x2 y2... [-noendcap]  (or)\n"
                       "segment layer width filename [-noendcap]\n\t\t\tpaint one or more wire segments"));
  CHECK(s.run("wire help").diagnostics.at(0).text == help);
}

TEST_CASE("an invalid wire option is reported followed by the help list", DISPATCH) {
  Session s;
  const auto r = s.run("wire bogus");
  CHECK_FALSE(r.ok());
  REQUIRE(r.diagnostics.size() == 2);
  CHECK(r.diagnostics[0].code == wiring::codes::kUsage);
  CHECK(r.diagnostics[0].text == "\"bogus\" isn't a valid wire option.");
  CHECK(r.diagnostics[1].text.starts_with("Wiring commands have the form"));
  // "s" is a prefix of show, switch and segment: ambiguous, so not valid.
  CHECK(s.run("wire s").diagnostics.at(0).text == "\"s\" isn't a valid wire option.");
}

TEST_CASE("wire options match case-insensitively by unique prefix", DISPATCH) {
  Session s;
  s.ok("wire TYP metal1 5");
  CHECK(s.run("wire Wid").value.to_string() == "5");
  s.ok("wire incr width");
  CHECK(s.run("wire val").value.to_string() == "metal1 6");
}

TEST_CASE("wrong argument forms print the option's syntax", DISPATCH) {
  Session s;
  const auto r = s.run("wire type metal1 4 5");
  CHECK_FALSE(r.ok());
  CHECK(last_text(r) == "Wrong arguments.  The correct syntax is\n    \"wire type [layer [width]]   select the type and "
                        "size of wires\"");
  CHECK(last_text(s.run("wire increment")) ==
        "Wrong arguments.  The correct syntax is\n    \"wire increment layer|width \tincrement the wire layer or width\"");
  CHECK(last_text(s.run("wire decrement size")).starts_with("Wrong arguments."));
  CHECK(last_text(s.run("wire leg to 1")).starts_with("Wrong arguments."));
  CHECK(last_text(s.run("wire switch metal1")).starts_with("Wrong arguments."));
  CHECK(last_text(s.run("wire width 1 2")).starts_with("Wrong arguments."));
}

TEST_CASE("cursor-reading wire commands are logged with explicit coordinates", DISPATCH) {
  Session s;
  s.point_at(80, 2);
  const auto logged = [&](std::vector<std::string> args) {
    const auto inv = wiring::logged_form(cmd::Invocation{"wire", std::move(args), cmd::Origin::key, {}}, s.editor);
    return inv ? cmd::format_command_line(*inv) : std::string{};
  };
  CHECK(logged({"leg"}) == "wire leg to 80i 2i");
  CHECK(logged({"hor"}) == "wire hor to 80i 2i");
  CHECK(logged({"vertical"}) == "wire vertical to 80i 2i");
  CHECK(logged({"type"}) == "wire type at 80i 2i");
  CHECK(logged({"leg", "to", "1", "2"}).empty());
  CHECK(logged({"type", "metal1"}).empty());
  CHECK(logged({"show"}).empty());
  s.editor.edit_transform = db::Transform{1, 0, 10, 0, 1, -5};  // edit cell placed at (10,-5)
  CHECK(logged({"leg"}) == "wire leg to 70i 7i");
  s.editor.command_point.reset();
  CHECK(logged({"leg"}).empty());
}

TEST_CASE("wire type picks the material under the cursor", TYPE) {
  Session s;
  s.paint("metal1", 0, 0, 100, 4);
  s.point_at(50, 2);
  const auto r = s.run("wire type");
  REQUIRE(r.ok());
  CHECK(last_text(r) == "Using metal1 wires 4 units wide.");
  CHECK(s.editor.wire.type == s.type("metal1"));
  CHECK(s.editor.wire.width == 4);
  CHECK(*s.editor.box == Rect{48, 0, 52, 4});
  CHECK(selected(s, "metal1") == std::vector<Rect>{{48, 0, 52, 4}});
  CHECK(s.editor.wire.last_direction == wiring::LegDirection::none);
  // `wire type at x y` reads the edit-cell point instead of the cursor.
  s.editor.command_point.reset();
  s.ok("wire type metal2 2");
  CHECK(last_text(s.run("wire type at 10 3")) == "Using metal1 wires 4 units wide.");
  CHECK(*s.editor.box == Rect{8, 0, 12, 4});
}

TEST_CASE("repeated wire type cycles through overlapping layers", TYPE) {
  Session s;
  s.paint("metal1", 0, 0, 100, 4);
  s.paint("metal2", 40, -10, 60, 20);
  s.point_at(50, 2);
  CHECK(last_text(s.run("wire type")) == "Using metal1 wires 4 units wide.");
  CHECK(last_text(s.run("wire type")) == "Using metal2 wires 20 units wide.");
  CHECK(*s.editor.box == Rect{40, -8, 60, 12});  // shifted inside nothing: fits
  CHECK(last_text(s.run("wire type")) == "Using metal1 wires 4 units wide.");
}

TEST_CASE("the type box is shifted inside the chunk; odd widths grow the point", TYPE) {
  Session s;
  s.paint("metal1", 0, 0, 3, 50);  // 3 wide
  s.point_at(0, 49);
  CHECK(last_text(s.run("wire type")) == "Using metal1 wires 3 units wide.");
  // odd: the 1x1 point area (0,49)-(1,50) grown by 1, then moved inside.
  CHECK(*s.editor.box == Rect{0, 47, 3, 50});
}

TEST_CASE("wire type without material or cursor", TYPE) {
  Session s;
  s.point_at(500, 500);
  const auto r = s.run("wire type");
  CHECK_FALSE(r.ok());
  CHECK(last_text(r) == "There's no material visible underneath the cursor.");
  CHECK(r.diagnostics.back().code == wiring::codes::kNoMaterial);
  s.editor.command_point.reset();
  CHECK(last_text(s.run("wire type")) ==
        "Can't use cursor to select wiring material unless\n    cursor is in a layout window.");
  CHECK_FALSE(s.editor.wire.type.has_value());
}

TEST_CASE("invisible layers are not picked", TYPE) {
  Session s;
  s.paint("metal1", 0, 0, 100, 4);
  auto* w = s.editor.windows.find(s.window);
  hefesto::tech::TypeMask visible = hefesto::tech::TypeMask::all();
  visible.clear_type(s.type("metal1"));
  w->set_visible_types(visible);
  s.point_at(50, 2);
  CHECK(last_text(s.run("wire type")) == "There's no material visible underneath the cursor.");
}

TEST_CASE("wire type with a layer name and width", TYPE) {
  Session s;
  s.ok("wire type metal1 7");
  CHECK(s.editor.wire.width == 7);
  s.ok("wire type metal2");  // the larger of the current width and the DRC minimum
  CHECK(s.editor.wire.width == 7);
  CHECK(s.editor.wire.type == s.type("metal2"));
  s.ok("wire width 1");
  s.ok("wire type metal3");
  CHECK(s.editor.wire.width == 6);
  auto r = s.run("wire type nosuch");
  CHECK(last_text(r) == "Layer name \"nosuch\" doesn't exist.");
  CHECK(r.diagnostics.back().code == wiring::codes::kBadLayer);
  CHECK(last_text(s.run("wire type met")) == "Layer name \"met\" is ambiguous.");
  CHECK(s.editor.wire.type == s.type("metal3"));
}

TEST_CASE("a type change resets the last leg direction", TYPE) {
  Session s;
  east_leg(s);
  CHECK(s.editor.wire.last_direction == wiring::LegDirection::east);
  s.ok("wire width 4");
  CHECK(s.editor.wire.last_direction == wiring::LegDirection::none);
}

TEST_CASE("horizontal leg to the right", LEGS) {
  Session s;
  s.ok("wire type metal1 4");
  s.ok("box 48 0 52 4");
  s.ok("wire horizontal to 80 2");
  CHECK(s.rects("metal1") == std::vector<Rect>{{48, 0, 82, 4}});
  CHECK(*s.editor.box == Rect{78, 0, 82, 4});
  CHECK(s.editor.box_root == s.top);
  CHECK(s.editor.wire.last_direction == wiring::LegDirection::east);
  CHECK(selected(s, "metal1") == std::vector<Rect>{{48, 0, 82, 4}});
  CHECK(s.top->flags().modified);
  CHECK(s.top->bbox() == Rect{48, 0, 82, 4});
}

TEST_CASE("legs west, north and south; leg direction choice", LEGS) {
  Session s;
  s.ok("wire type metal1 4");
  s.ok("box 48 0 52 4");
  s.ok("wire horizontal to 20 2");
  CHECK(s.rects("metal1") == std::vector<Rect>{{18, 0, 52, 4}});
  CHECK(*s.editor.box == Rect{18, 0, 22, 4});
  CHECK(s.editor.wire.last_direction == wiring::LegDirection::west);
  s.ok("wire vertical to 20 30");
  CHECK(*s.editor.box == Rect{18, 28, 22, 32});
  CHECK(s.editor.wire.last_direction == wiring::LegDirection::north);
  s.ok("wire vertical to 20 -30");
  CHECK(*s.editor.box == Rect{18, -32, 22, -28});
  CHECK(s.editor.wire.last_direction == wiring::LegDirection::south);
  // `leg` runs along the larger distance from the box: here vertically.
  s.ok("box 48 0 52 4");
  s.ok("wire leg to 60 30");
  CHECK(*s.editor.box == Rect{48, 28, 52, 32});
  CHECK(s.editor.wire.last_direction == wiring::LegDirection::north);
}

TEST_CASE("a box of another height is recentered; points inside the span paint nothing", LEGS) {
  Session s;
  s.ok("wire type metal1 4");
  s.ok("box 40 0 60 10");
  s.ok("wire horizontal to 80 0");  // square (48,3)-(52,7); y clamped to it
  CHECK(s.rects("metal1") == std::vector<Rect>{{48, 3, 82, 7}});
  s.ok("box 48 0 52 4");
  const auto before = s.rects("metal1");
  s.ok("wire horizontal to 50 2");
  CHECK(s.rects("metal1") == before);
  CHECK(*s.editor.box == Rect{48, 0, 52, 4});
}

TEST_CASE("odd wire widths use half the width rounded down", LEGS) {
  Session s;
  s.ok("wire type metal1 3");
  s.ok("box 0 0 3 3");
  s.ok("wire horizontal to 10 1");
  CHECK(s.rects("metal1") == std::vector<Rect>{{0, 0, 11, 3}});
  CHECK(*s.editor.box == Rect{8, 0, 11, 3});
}

TEST_CASE("a leg before any wire type reports no material and paints nothing", LEGS) {
  for (const bool compat : {true, false}) {
    Session s;
    s.project.compat.magic_compat = compat;
    s.ok("box 48 0 52 4");
    const auto r = s.run("wire horizontal to 80 2");
    CHECK_FALSE(r.ok());
    CHECK(last_text(r) == "Can't add a new wire leg:  no wiring material selected.");
    CHECK(s.top->plane(hefesto::tech::kFirstUserPlane + 2).empty());
    CHECK_FALSE(s.top->flags().modified);
  }
}

TEST_CASE("legs need the box on the edit cell and the cursor over it", LEGS) {
  Session s;
  s.ok("wire type metal1 4");
  CHECK(last_text(s.run("wire leg to 10 10")) ==
        "No box!  Before wiring a leg, you must set the box\n    to indicate where the leg starts.");
  s.ok("box 0 0 4 4");
  db::CellDef* other = s.editor.cells.create("other", s.editor.technology->plane_count()).value();
  s.editor.box_root = other;
  CHECK(last_text(s.run("wire leg to 10 10")) ==
        "The box must be on the edit cell so it can be used\n    as the starting point for a wire leg.");
  s.editor.box_root = s.top;
  CHECK(last_text(s.run("wire leg")) ==
        "Before wiring, must place cursor over edit cell to\n    indicate endpoint of new wire leg.");
  s.point_at(20, 2);
  s.ok("wire leg");
  CHECK(s.rects("metal1") == std::vector<Rect>{{0, 0, 22, 4}});
}

TEST_CASE("leg coordinates are edit-cell coordinates", LEGS) {
  Session s;
  s.editor.edit_transform = db::Transform{1, 0, 100, 0, 1, 0};  // edit cell at x+100 in its root
  s.ok("wire type metal1 4");
  s.editor.box = Rect{148, 0, 152, 4};  // root coordinates
  s.ok("wire horizontal to 80 2");      // edit (80,2) = root (180,2)
  CHECK(s.rects("metal1") == std::vector<Rect>{{48, 0, 82, 4}});  // painted in edit coordinates
  CHECK(*s.editor.box == Rect{178, 0, 182, 4});
}

TEST_CASE("contact at the end of an east leg", CONTACT) {
  Session s;
  east_leg(s);
  s.ok("wire switch metal2 4");
  CHECK(s.rects("m2contact") == std::vector<Rect>{{78, 0, 82, 4}});
  CHECK(s.editor.wire.type == s.type("metal2"));
  CHECK(s.editor.wire.width == 4);
  CHECK(*s.editor.box == Rect{78, 0, 82, 4});
  CHECK(selected(s, "m2contact") == std::vector<Rect>{{78, 0, 82, 4}});
  // Back down: the same contact, used the other way round.
  s.ok("wire horizontal to 120 2");
  s.ok("wire switch metal1 4");
  CHECK(wiring::test::sorted(s.rects("m2contact")) == std::vector<Rect>{{78, 0, 82, 4}, {118, 0, 122, 4}});
}

TEST_CASE("contacts with surrounds and extensions", CONTACT) {
  Session s;
  // pc: size 2, poly surround 1, metal1 surround 1, at a north poly leg.
  s.ok("wire type poly 2");
  s.ok("box 10 0 12 2");
  s.ok("wire vertical to 11 19");
  REQUIRE(*s.editor.box == Rect{10, 18, 12, 20});
  s.ok("wire switch metal1 3");
  CHECK(s.rects("pc") == std::vector<Rect>{{10, 18, 12, 20}});
  CHECK(s.top->plane(*s.editor.technology->home_plane(s.type("metal1"))).type_at({9, 17}) == s.type("metal1"));
  CHECK(s.top->plane(*s.editor.technology->home_plane(s.type("poly"))).type_at({12, 20}) == s.type("poly"));
  CHECK(*s.editor.box == Rect{9, 17, 13, 21});

  // m3c: size 2, metal2 surround 1 extend 1, metal3 surround 1 extend 2, east leg.
  Session t;
  t.ok("wire type metal2 4");
  t.ok("box 48 0 52 4");
  t.ok("wire horizontal to 80 2");
  t.ok("wire switch metal3 6");
  const auto& m2 = t.top->plane(*t.editor.technology->home_plane(t.type("metal2")));
  const auto& m3 = t.top->plane(*t.editor.technology->home_plane(t.type("metal3")));
  CHECK(m3.type_at({79, 1}) == t.type("m3c"));
  CHECK(m3.type_at({80, 2}) == t.type("m3c"));
  CHECK(m3.type_at({81, 3}) == t.type("metal3"));
  CHECK(m3.type_at({78, -1}) == t.type("metal3"));  // extension 2 across the east leg
  CHECK(m3.type_at({81, 4}) == t.type("metal3"));
  CHECK(m3.type_at({82, 2}) == hefesto::tech::kSpace);
  CHECK(m3.type_at({78, -2}) == hefesto::tech::kSpace);
  CHECK(m2.type_at({78, -1}) == hefesto::tech::kSpace);
  CHECK(*t.editor.box == Rect{78, 0, 82, 4});
}

TEST_CASE("the contact keeps only its length at the leg's end", CONTACT) {
  Session s;
  east_leg(s);
  s.ok("wire width 6");  // resets the direction ...
  s.editor.wire.last_direction = wiring::LegDirection::east;  // ... restored for the test
  s.ok("box 70 0 82 6");
  s.ok("wire switch metal2 6");
  CHECK(s.rects("m2contact") == std::vector<Rect>{{76, 0, 82, 6}});
  Session t;
  t.ok("wire type metal1 6");
  t.ok("box 70 0 82 6");  // no leg yet: no trimming
  t.ok("wire switch metal2 6");
  CHECK(t.rects("m2contact") == std::vector<Rect>{{70, 0, 82, 6}});
}

TEST_CASE("contact placement errors", CONTACT) {
  Session s;
  s.ok("wire type metal1 4");
  CHECK(last_text(s.run("wire switch metal2 4")) ==
        "No box!  To place a contact, you must first use\n    the box to mark the previous leg of the wire,\n    at "
        "the end of which the contact will be placed.");
  s.ok("box 0 0 4 4");
  auto r = s.run("wire switch metal1 4");
  CHECK(last_text(r) == "The new wiring layer is the same as the old one, so\n    there's no need for a contact.");
  CHECK(r.diagnostics.back().code == wiring::codes::kNoContact);
  CHECK(last_text(s.run("wire switch metal3 4")) ==
        "The technology file doesn't define a contact\n    between \"metal1\" and \"metal3\".");
  CHECK(s.editor.wire.type == s.type("metal3"));  // the type was switched anyway, as in Magic
  CHECK(last_text(s.run("wire switch nosuch 4")) == "Layer name \"nosuch\" doesn't exist.");
}

TEST_CASE("wire increment and decrement of the type", CONTACT) {
  Session s;
  east_leg(s);
  s.ok("wire increment type");
  CHECK(s.editor.wire.type == s.type("metal2"));
  CHECK(s.editor.wire.width == 4);  // max(4, DRC width of metal2 = 4)
  CHECK(s.rects("m2contact") == std::vector<Rect>{{78, 0, 82, 4}});
  s.ok("wire decrement layer");
  CHECK(s.editor.wire.type == s.type("metal1"));
  s.ok("wire decrement type");
  CHECK(s.editor.wire.type == s.type("poly"));
  const auto r = s.run("wire decrement type");
  CHECK(last_text(r) == "No routing layer defined");
  CHECK(s.editor.wire.type == s.type("poly"));
  Session fresh;
  CHECK(last_text(fresh.run("wire increment type")) == "No routing layer defined");
}

TEST_CASE("several routing layers: the first in search order is used", CONTACT) {
  std::string text = wiring::test::read_text(std::filesystem::path{HEFESTO_TEST_DATA_DIR} / "wire.tech");
  const std::string old_line = "contact pc 2 poly 1 metal1 1";
  text.replace(text.find(old_line), old_line.size(), "contact pc 2 metal1 1 poly 1");
  Session s(wiring::test::load_tech(text));
  east_leg(s);
  const auto r = s.run("wire incr type");
  REQUIRE(r.diagnostics.size() >= 1);
  CHECK(r.diagnostics.front().text == "Ambiguous directive---multiple routing types");
  CHECK(r.diagnostics.front().code == wiring::codes::kAmbiguousRouting);
  // Search order is reverse file order: m2contact (metal1 -> metal2) comes
  // before the first line's pc (metal1 -> poly).
  CHECK(s.editor.wire.type == s.type("metal2"));
  CHECK(s.rects("m2contact") == std::vector<Rect>{{78, 0, 82, 4}});
}

TEST_CASE("wire increment and decrement of the width", CONTACT) {
  Session s;
  s.ok("wire type metal1 4");
  s.ok("wire increment width");
  CHECK(s.editor.wire.width == 5);
  s.ok("wire decrement width 3");
  CHECK(s.editor.wire.width == 2);
  CHECK(s.editor.wire.type == s.type("metal1"));
}

TEST_CASE("wire show previews the next leg as the selection only", SHOW) {
  Session s;
  s.ok("wire type metal1 4");
  s.ok("box 48 0 52 4");
  s.point_at(80, 2);
  s.ok("wire show");
  CHECK(selected(s, "metal1") == std::vector<Rect>{{48, 0, 82, 4}});
  CHECK(s.rects("metal1").empty());
  CHECK_FALSE(s.top->flags().modified);
  CHECK(*s.editor.box == Rect{48, 0, 52, 4});
  // Silent without a cursor, box or type.
  s.editor.command_point.reset();
  CHECK(s.run("wire show").diagnostics.empty());
  Session t;
  t.point_at(80, 2);
  t.ok("box 48 0 52 4");
  CHECK(t.run("wire show").diagnostics.empty());
  CHECK(t.editor.selection.empty());
}

TEST_CASE("wire values and width", SHOW) {
  Session s;
  CHECK(s.run("wire values").value.to_string() == "space 0");
  s.ok("wire type metal1 4");
  CHECK(s.run("wire width").value.to_string() == "4");
  CHECK(s.run("wire values").value.to_string() == "metal1 4");
  s.ok("wire width 6");
  CHECK(s.editor.wire.type == s.type("metal1"));
  CHECK(s.run("wire width").value.to_string() == "6");
  CHECK(s.run("wire values extra").value.is_none());
  CHECK(all_text(s.run("wire width")).empty());
}
