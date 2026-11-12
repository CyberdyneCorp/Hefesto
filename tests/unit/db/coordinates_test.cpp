// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cmd-coordinates-and-units: keyword matching, directions, the coordinate
// expression parser, units, snap, pointer snapping and reported values.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <string_view>

#include "edit_session.hpp"
#include "hefesto/cmd/coordinates.hpp"
#include "hefesto/cmd/keywords.hpp"
#include "hefesto/cmd/units.hpp"
#include "hefesto/diag/sink.hpp"

namespace cmd = hefesto::cmd;
namespace db = hefesto::db;
using db::test::EditSession;

#define CAP "[cap:cmd-coordinates-and-units]"

namespace {

constexpr std::array<std::string_view, 9> kUnits = {"default", "internal", "lambda", "grid",   "user",
                                                    "microns", "list",     "print",  "noprint"};

std::int32_t dist(std::string_view text, const cmd::CoordinateUnits& u, cmd::Axis axis = cmd::Axis::x,
                  bool relative = false, hefesto::diag::DiagnosticSink* sink = nullptr) {
  const auto v = cmd::parse_distance(text, u, cmd::DistanceRequest{axis, relative, 1}, sink);
  REQUIRE(v.ok());
  return v.value();
}

cmd::CoordinateUnits lambda2() {
  cmd::CoordinateUnits u;
  u.internal_per = 2;  // L1 = 2, L0 = 1
  return u;
}

cmd::CoordinateUnits microns10() {
  cmd::CoordinateUnits u;
  u.cif_scale = std::array<std::int64_t, 2>{1, 1};  // U = 10 nm
  u.default_unit = cmd::DefaultUnit::internal;
  return u;
}

}  // namespace

TEST_CASE("keyword matching: unique prefix, ambiguity and exact match", CAP "[req:Keyword matching]") {
  CHECK(cmd::lookup("mi", kUnits).match == cmd::Match::found);
  CHECK(cmd::lookup("mi", kUnits).index == 5);
  CHECK(cmd::lookup("l", kUnits).match == cmd::Match::ambiguous);
  CHECK(cmd::lookup("xyz", kUnits).match == cmd::Match::unrecognized);
  CHECK(cmd::lookup("", kUnits).match == cmd::Match::ambiguous);
  CHECK(cmd::lookup("MICRONS", kUnits).index == 5);           // case-insensitive
  CHECK(cmd::lookup("::magic::lambda", kUnits).index == 2);  // qualifier ignored
  CHECK(cmd::lookup("magic::print", kUnits).index == 7);
  constexpr std::array<std::string_view, 5> dirs = {"n", "ne", "north", "northeast", "northwest"};
  CHECK(cmd::lookup("n", dirs).index == 0);
  CHECK(cmd::lookup("no", dirs).match == cmd::Match::ambiguous);
  constexpr std::array<std::string_view, 2> worded = {"width [value] set width", "height"};
  CHECK(cmd::lookup("width", worded).index == 0);  // only the leading word matches
  CHECK(cmd::lookup("width [", worded).match == cmd::Match::unrecognized);
}

TEST_CASE("direction and position names", CAP "[req:Direction and position names]") {
  CHECK(cmd::parse_direction("up").value() == cmd::Direction::north);
  CHECK(cmd::parse_direction("n").value() == cmd::Direction::north);
  CHECK(cmd::parse_direction("dl").value() == cmd::Direction::southwest);
  CHECK(cmd::parse_direction("right").value() == cmd::Direction::east);
  CHECK(cmd::parse_position("center").value() == cmd::Direction::center);
  const auto t = cmd::parse_direction("t");
  REQUIRE_FALSE(t.ok());
  CHECK(t.error().text.starts_with("\"t\" is ambiguous."));
  const auto bad = cmd::parse_direction("zz");
  REQUIRE_FALSE(bad.ok());
  CHECK(bad.error().text.starts_with("\"zz\" is not a valid direction or position."));
  CHECK(bad.error().text.find("Legal directions/positions are:") != std::string::npos);
  const auto ne = cmd::parse_direction("ne", true);
  REQUIRE_FALSE(ne.ok());
  CHECK(ne.error().text.starts_with("\"ne\" is not a Manhattan direction or position."));
  CHECK(ne.error().text.find("tr") == std::string::npos);  // only Manhattan names listed
}

TEST_CASE("numeric literals round half away from zero per operand", CAP "[req:Numeric literal parsing]") {
  cmd::CoordinateUnits u;
  u.default_unit = cmd::DefaultUnit::internal;
  CHECK(dist("2.5", u) == 3);
  CHECK(dist("-2.5", u) == -3);
  CHECK(dist("1e2", u) == 100);
  CHECK(dist("{ 7", u) == 7);  // a leading brace and blanks are skipped
  hefesto::diag::CollectingSink sink;
  CHECK(dist("abc", u, cmd::Axis::x, false, &sink) == 0);
  CHECK(dist("5+abc", u, cmd::Axis::x, false, &sink) == 5);  // the value so far
  const auto notes = sink.take();
  REQUIRE(notes.size() == 2);
  CHECK(notes[0].text == "Coordinate value cannot be parsed:  assuming 0");
}

TEST_CASE("unit suffixes and their errors", CAP "[req:Unit suffixes]") {
  const cmd::CoordinateUnits m = microns10();
  CHECK(dist("1.5um", m) == 150);
  CHECK(dist("0.005um", m) == 1);
  CHECK(dist("25nm", m) == 3);
  CHECK(dist("-0.005um", m) == -1);
  CHECK(dist("1microns", m) == 100);
  CHECK(dist("10centimicrons", m) == 10);
  CHECK(dist("1mm", m) == 100000);
  CHECK(dist("3l", lambda2()) == 6);
  cmd::CoordinateUnits g;
  g.grid = std::array<std::int64_t, 4>{5, 5, 15, 25};
  CHECK(dist("2g", g, cmd::Axis::x, false) == 25);
  CHECK(dist("2g", g, cmd::Axis::y, true) == 40);
  hefesto::diag::CollectingSink sink;
  CHECK(dist("5km", cmd::CoordinateUnits{}, cmd::Axis::x, false, &sink) == 5);
  CHECK(sink.take().front().text == "Unknown metric prefix \"km\"; assuming internal units");
  const auto no_style = cmd::parse_distance("1um", cmd::CoordinateUnits{}, {});
  REQUIRE_FALSE(no_style.ok());
  CHECK(no_style.error().text == "Metric units need a CIF/GDS output style");
  const auto no_window = cmd::parse_distance("5+2g", cmd::CoordinateUnits{}, {});
  REQUIRE_FALSE(no_window.ok());
  CHECK(no_window.error().text == "Grid units need a layout window");
}

TEST_CASE("un-suffixed operands use the effective input unit", CAP "[req:Default unit of un-suffixed values]") {
  cmd::CoordinateUnits u = lambda2();
  CHECK(dist("5", u) == 10);
  CHECK(dist("3+1", u) == 5);  // magic-compat ON: 3 is internal before the operator
  u.magic_compat = false;
  CHECK(dist("3+1", u) == 8);
  u.default_unit = cmd::DefaultUnit::microns;
  u.cif_scale = std::array<std::int64_t, 2>{1, 1};
  CHECK(dist("2", u) == 200);
}

TEST_CASE("coordinate expressions evaluate left to right", CAP "[req:Coordinate expressions]") {
  cmd::CoordinateUnits i;
  i.default_unit = cmd::DefaultUnit::internal;
  CHECK(dist("2 + 3 * 4", i) == 20);
  CHECK(dist("{2 + 3 * 4}", i) == 20);
  CHECK(dist("7 / 2", i) == 3);
  CHECK(dist("-7 / 2", i) == -3);
  CHECK(dist("100+5", lambda2()) == 110);
  cmd::CoordinateUnits off = lambda2();
  off.magic_compat = false;
  CHECK(dist("100+5", off) == 210);
  hefesto::diag::CollectingSink sink;
  CHECK(dist("1um+2um", microns10(), cmd::Axis::x, false, &sink) == 100);
  REQUIRE(sink.snapshot().size() == 1);
  CHECK(sink.take().front().text == "Ignored text after unit suffix: \"+2um\"");
  cmd::CoordinateUnits m_off = microns10();
  m_off.magic_compat = false;
  CHECK(dist("1um+2um", m_off, cmd::Axis::x, false, &sink) == 300);
  CHECK(sink.snapshot().empty());
  const auto zero = cmd::parse_distance("4/0", i, {});
  REQUIRE_FALSE(zero.ok());
}

TEST_CASE("units command queries and sets the units", CAP "[req:Units command]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession s(mt);
  CHECK(s.value("units") == "default");
  s.ok("units microns print");
  CHECK(s.value("units") == "microns print");
  s.ok("units internal");
  CHECK(s.value("units") == "internal print");  // the print flag is kept
  s.ok("units default");
  CHECK(s.value("units list") == "default");
  s.ok("units lambda noprint");
  CHECK(s.value("units") == "lambda noprint");
  const auto bad = s.run("units lambda what");
  CHECK_FALSE(bad.ok());
  CHECK(s.value("units") == "lambda noprint");
  const auto usage = s.ok("units l");  // ambiguous: usage, nothing changes
  CHECK(EditSession::texts(usage).find("Usage: units") != std::string::npos);
  CHECK(s.value("units") == "lambda noprint");
}

TEST_CASE("bare units print from default follows magic-compat", CAP "[req:Units command]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession on(mt);
  on.ok("units print");
  CHECK(on.value("units") == "microns print");
  EditSession off(mt);
  off.compat(false);
  const auto r = off.run("units print");
  CHECK_FALSE(r.ok());
  CHECK(r.diagnostics.front().text == "set a unit type first");
  CHECK(off.value("units") == "default");
}

TEST_CASE("snap command and its deprecated units side effect", CAP "[req:Snap command]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession s(mt);
  CHECK(s.value("snap") == "lambda");
  const auto r = s.ok("snap internal");
  CHECK(EditSession::texts(r).find("deprecated") != std::string::npos);
  CHECK(s.value("units") == "internal noprint");
  CHECK(s.value("snap") == "internal");
  EditSession t(mt);
  t.ok("units lambda");
  const auto quiet = t.ok("snap internal");
  CHECK(EditSession::texts(quiet).find("deprecated") == std::string::npos);
  CHECK(t.value("units") == "lambda noprint");
  t.ok("snap on");
  CHECK(t.value("snap") == "user");
}

TEST_CASE("pointer snapping picks the nearer line, ties upward", CAP "[req:Pointer snapping]") {
  const cmd::SnapGrid user{db::Point{0, 0}, 4, 4};
  CHECK(cmd::snap_point(db::Point{5, 6}, user) == db::Point{4, 8});
  const cmd::SnapGrid lambda{db::Point{0, 0}, 2, 2};
  CHECK(cmd::snap_point(db::Point{-3, 0}, lambda).x == -2);
  CHECK(cmd::snap_point(db::Point{-5, 0}, lambda).x == -4);
  const cmd::SnapGrid offset{db::Point{5, 5}, 10, 20};
  CHECK(cmd::snap_point(db::Point{9, 16}, offset) == db::Point{5, 25});
}

TEST_CASE("typed values are not snapped", CAP "[req:Pointer snapping]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession s(mt);
  s.editor.grid.internal = 2;
  s.ok("box values 1i 1i 3i 3i");
  CHECK(s.box() == db::Rect{1, 1, 3, 3});
}

TEST_CASE("reported values follow units", CAP "[req:Reported value formatting]") {
  cmd::CoordinateUnits scale = lambda2();
  cmd::UnitsState u{cmd::InputUnit::lambda, true};
  CHECK(cmd::format_value(15, u, scale) == "7.5l");
  CHECK(cmd::format_value(15, u, scale, cmd::ValueKind::area) == "3.75l^2");
  u.print = false;
  CHECK(cmd::format_value(20, u, scale) == "10");
  const cmd::CoordinateUnits m = microns10();
  const cmd::UnitsState mic{cmd::InputUnit::microns, false};
  CHECK(cmd::format_value(150, mic, m) == "1.5");
  CHECK(cmd::format_value(20000, mic, m, cmd::ValueKind::area) == "2");
  CHECK(cmd::format_value(150, cmd::UnitsState{cmd::InputUnit::microns, true}, m) == "1.5um");
  CHECK(cmd::format_value(10, cmd::UnitsState{}, m) == "10");
  CHECK(cmd::format_value(10, cmd::UnitsState{cmd::InputUnit::internal, true}, m) == "10i");
  cmd::CoordinateUnits g;
  const cmd::UnitsState user{cmd::InputUnit::user, true};
  CHECK(cmd::format_value(10, user, g) == "10");  // no window: as default
  g.grid = std::array<std::int64_t, 4>{0, 0, 4, 8};
  CHECK(cmd::format_value(10, user, g, cmd::ValueKind::length, cmd::Axis::x) == "2.5gx");
  CHECK(cmd::format_value(10, user, g, cmd::ValueKind::length, cmd::Axis::y) == "1.25gy");
  CHECK(cmd::format_value(64, user, g, cmd::ValueKind::area) == "2gx*gy");
}

TEST_CASE("startup units are lambda and box reports follow units", CAP "[req:Default unit of un-suffixed values]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession s(mt);
  s.editor.grid.internal = 2;
  s.ok("box values 0 0 1 1");
  s.ok("box width 5");
  CHECK(s.box().width() == 10);
  CHECK(s.value("box width") == "10");  // default units report internal integers
  s.ok("box width 3+1");
  CHECK(s.box().width() == 5);
  s.ok("units lambda print");
  s.ok("box values 0 0 15i 1");
  CHECK(s.value("box width") == "7.5l");
}

TEST_CASE("editing needs the box in a window on the edit cell", CAP "[req:Root and edit coordinate frames]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession s(mt);
  db::CellDef* other = s.cell("other");
  s.editor.box = db::Rect{0, 0, 10, 10};
  s.editor.box_root = other;
  const auto r = s.run("paint metal1");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().text == "The box isn't in a window on the edit cell.");
  CHECK(db::test::rects_of(*s.editor.edit_cell, mt.metal1, mt.m1).empty());
  s.editor.box.reset();
  // Regression (golden SCRIPT-API-21, SCRIPT-RSP-04): ToolGetEditBox's text, not the box
  // command's "Box tool must be present".
  CHECK(s.run("paint metal1").diagnostics.back().text == "Box must be present");
  CHECK(s.run("label A c metal1").diagnostics.back().text == "Box must be present");
  CHECK(s.run("port first").diagnostics.back().text == "Box must be present");
  CHECK(s.run("box values").diagnostics.back().text == "Box tool must be present");
}

TEST_CASE("message texts name the bad argument and substitute 0", CAP "[req:Message texts]") {
  HEFESTO_REQUIRE_MT(mt);
  EditSession s(mt);
  s.ok("box values 0 0 10 10");
  const auto r = s.ok("box width abc");
  CHECK(EditSession::texts(r).find("Coordinate value cannot be parsed:  assuming 0") != std::string::npos);
  CHECK(s.box().width() == 0);
}
