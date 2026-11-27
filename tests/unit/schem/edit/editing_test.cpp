// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The editing operations of `schematic` (capability schematic-editing,
// tasks.md 9.1 and 9.2): document model, snap, selection, wires, placing,
// move/copy/stretch, rotate/flip, labels, buses, properties, graphics,
// undo, read-only libraries, hierarchy and highlighting.
//
// Owner: EDIT.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "edit_fixture.hpp"
#include "hefesto/schem/edit/edit_state.hpp"
#include "hefesto/schem/edit/hier_path.hpp"
#include "hefesto/schem/edit/wires.hpp"
#include "hefesto/schem/file/properties.hpp"

namespace schem = hefesto::schem;
using hefesto::test::active;
using hefesto::test::new_schematic;
using hefesto::test::records;
using hefesto::test::run;
using hefesto::test::saved;
using hefesto::test::SchemSession;

namespace {

constexpr const char* kModel = "[cap:schematic-editing][req:Document model]";
constexpr const char* kSnap = "[cap:schematic-editing][req:Snap and grid]";
constexpr const char* kSelection = "[cap:schematic-editing][req:Selection]";
constexpr const char* kWires = "[cap:schematic-editing][req:Wires]";
constexpr const char* kPlacing = "[cap:schematic-editing][req:Placing instances]";
constexpr const char* kMove = "[cap:schematic-editing][req:Move, copy and stretch]";
constexpr const char* kRotate = "[cap:schematic-editing][req:Rotate and flip]";
constexpr const char* kLabels = "[cap:schematic-editing][req:Labels and ports]";
constexpr const char* kBus = "[cap:schematic-editing][req:Bus editing]";
constexpr const char* kProps = "[cap:schematic-editing][req:Property editing]";
constexpr const char* kGraphics = "[cap:schematic-editing][req:Graphics and text]";

std::string prop(const std::string& props, const std::string& key) {
  return schem::get_token(props, key).value_or("<none>");
}

}  // namespace

// ---- Document model ----------------------------------------------------------

TEST_CASE("Created objects appended", kModel) {
  SchemSession s;
  new_schematic(s);
  CHECK(run(s, {"wire", "0", "0", "10", "0"}) == "0");
  CHECK(run(s, {"wire", "0", "10", "10", "10"}) == "1");
  CHECK(run(s, {"wire", "0", "20", "10", "20"}) == "2");
  CHECK(records(saved(s), "N ") ==
        std::vector<std::string>{"N 0 0 10 0 {}", "N 0 10 10 10 {}", "N 0 20 10 20 {}"});
}

TEST_CASE("every editing operation is a schematic option and marks the document modified", kModel) {
  SchemSession s;
  new_schematic(s);
  CHECK_FALSE(active(s).modified);
  run(s, {"wire", "0", "0", "10", "0"});
  CHECK(active(s).modified);
  // The operations are found by CMD's dispatcher (prefix matching included).
  CHECK(run(s, {"selection"}).empty());
  CHECK(run(s, {"SELECT", "wire", "0"}) == "{wire 0}");  // any case
}

TEST_CASE("a schematic and a symbol with the same base name are separate documents", kModel) {
  SchemSession s;
  new_schematic(s, "amp.sch");
  run(s, {"wire", "0", "0", "10", "0"});
  run(s, {"new", "symbol", s.folder().str("amp.sym")});
  CHECK(active(s).wires.empty());
  CHECK(s.schem().documents.size() == 2);
}

// ---- Snap and grid -------------------------------------------------------------

TEST_CASE("Halve snap", kSnap) {
  schem::SnapSettings settings;
  CHECK(settings.snap == 10.0);
  CHECK(settings.grid == 20.0);
  schem::halve_snap(settings);
  CHECK(schem::snap_point({7.4, 0.0}, settings.snap).x == 5.0);
  schem::double_snap(settings);
  schem::double_snap(settings);
  CHECK(settings.snap == 20.0);
  CHECK_FALSE(schem::set_snap(settings, 0.0));
  CHECK_FALSE(schem::set_snap(settings, -1.0));
  CHECK(schem::set_snap(settings, 2.5));
  CHECK(schem::snap_value(6.0, settings.snap) == 5.0);
}

TEST_CASE("objects move by their delta and align snaps every coordinate", kSnap) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "1", "2", "13", "2"});
  run(s, {"select", "wire", "0"});
  run(s, {"move", "10", "0"});
  CHECK(active(s).wires[0].p1 == schem::Point{11, 2});  // off-grid stays off-grid
  run(s, {"align"});
  CHECK(active(s).wires[0].p1 == schem::Point{10, 0});
  CHECK(active(s).wires[0].p2 == schem::Point{20, 0});
}

// ---- Selection -------------------------------------------------------------------

TEST_CASE("Right-to-left touch selection", kSelection) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "0", "0", "100", "0"});
  // Left to right: only objects entirely inside.
  CHECK(run(s, {"select", "area", "40", "-10", "60", "10"}).empty());
  CHECK(run(s, {"select", "area", "60", "-10", "40", "10"}) == "{wire 0}");
  run(s, {"select", "clear"});
  s.schem().edit.select_touched = false;
  CHECK(run(s, {"select", "area", "60", "-10", "40", "10"}).empty());
  CHECK(run(s, {"select", "area", "-10", "-10", "110", "10"}) == "{wire 0}");
}

TEST_CASE("select instance, all and clear; selecting is not an undo step", kSelection) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0"});
  run(s, {"wire", "0", "30", "0", "70"});
  CHECK(run(s, {"select", "instance", "R1"}) == "{instance R1}");
  CHECK(run(s, {"select", "all"}) == "{wire 0} {instance R1}");
  CHECK(run(s, {"select", "clear"}).empty());
  const auto missing = s.schematic({"select", "instance", "R9"});
  CHECK_FALSE(missing.ok());
  CHECK(SchemSession::message(missing) == "schematic: no instance \"R9\"");
  CHECK(s.schem().edit.histories[schem::active_document(s.schem())->id].records.size() == 2);
}

TEST_CASE("select connected takes the wires and labels of the net", kSelection) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "0", "0", "50", "0"});
  run(s, {"wire", "50", "0", "50", "50"});
  run(s, {"wire", "200", "0", "300", "0"});
  run(s, {"label", "0", "0", "A"});
  run(s, {"select", "wire", "1"});
  CHECK(run(s, {"select", "connected"}) == "{wire 0} {wire 1} {instance p1}");
}

// Regression (M8 round 6 review): the subcommand table listed `-remove` and
// `select connected -stopjunction`, but `select` rejected both words.
TEST_CASE("Select connected stops at a junction", kSelection) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    SchemSession s;
    s.project().compat.xschem_compat = on;
    new_schematic(s);
    run(s, {"wire", "0", "0", "50", "0"});
    run(s, {"wire", "50", "0", "100", "0"});
    run(s, {"wire", "50", "0", "50", "50"});
    run(s, {"wire", "-50", "0", "0", "0"});  // W4 continues W1 end to end, no junction
    run(s, {"select", "wire", "0"});
    CHECK(run(s, {"select", "connected", "-stopjunction"}) == "{wire 0} {wire 3}");
    run(s, {"select", "clear"});
    run(s, {"select", "wire", "0"});
    CHECK(run(s, {"select", "connected"}) == "{wire 0} {wire 1} {wire 2} {wire 3}");
    CHECK_FALSE(s.schematic({"select", "all", "-stopjunction"}).ok());
  }
}

TEST_CASE("Remove one object from the selection", kSelection) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    SchemSession s;
    s.project().compat.xschem_compat = on;
    new_schematic(s);
    run(s, {"place", "res.sym", "0", "0"});
    run(s, {"place", "res.sym", "100", "0"});
    run(s, {"wire", "0", "200", "50", "200"});
    run(s, {"select", "all"});
    CHECK(run(s, {"select", "instance", "R1", "-remove"}) == "{wire 0} {instance R2}");
    CHECK(run(s, {"select", "area", "-10", "190", "60", "210", "-remove"}) == "{instance R2}");
    CHECK(run(s, {"select", "instance", "R1", "-remove"}) == "{instance R2}");  // not selected: no change
    // A failing form leaves the selection as it was.
    CHECK_FALSE(s.schematic({"select", "instance", "R9", "-remove"}).ok());
    CHECK(run(s, {"selection"}) == "{instance R2}");
    CHECK_FALSE(s.schematic({"select", "clear", "-remove"}).ok());
  }
}

// ---- Wires -------------------------------------------------------------------------

TEST_CASE("Manhattan wire", kWires) {
  const auto hv = schem::route_wire({0, 0}, {50, 30}, schem::WireRoute::horizontal_first);
  REQUIRE(hv.size() == 2);
  CHECK(hv[0] == schem::WireSegment{{0, 0}, {50, 0}});
  CHECK(hv[1] == schem::WireSegment{{50, 0}, {50, 30}});
  const auto vh = schem::route_wire({0, 0}, {50, 30}, schem::WireRoute::vertical_first);
  CHECK(vh[0] == schem::WireSegment{{0, 0}, {0, 30}});
  CHECK(schem::route_wire({0, 0}, {50, 0}, schem::WireRoute::horizontal_first).size() == 1);
  CHECK(schem::route_wire({0, 0}, {50, 30}, schem::WireRoute::oblique).size() == 1);
  CHECK(schem::next_route(schem::WireRoute::oblique) == schem::WireRoute::horizontal_first);
  CHECK(schem::orthogonal_route({0, 0}, {50, 30}) == schem::WireRoute::horizontal_first);
  CHECK(schem::orthogonal_route({0, 0}, {5, 30}) == schem::WireRoute::vertical_first);

  // The tool issues one `wire` per segment.
  SchemSession s;
  new_schematic(s);
  for (const auto& seg : hv) {
    run(s, {"wire", std::to_string(seg.p1.x), std::to_string(seg.p1.y), std::to_string(seg.p2.x),
            std::to_string(seg.p2.y)});
  }
  CHECK(records(saved(s), "N ") == std::vector<std::string>{"N 0 0 50 0 {}", "N 50 0 50 30 {}"});
}

TEST_CASE("T junction dot", kWires) {
  schem::Document doc;
  doc.wires = {{{0, 0}, {100, 0}, {}}, {{50, 0}, {50, 50}, {}}};
  CHECK(schem::junction_points(doc) == std::vector<schem::Point>{{50, 0}});
  // Two wires meeting end to end make no dot; a third one does.
  schem::Document corner;
  corner.wires = {{{0, 0}, {50, 0}, {}}, {{50, 0}, {50, 50}, {}}};
  CHECK(schem::junction_points(corner).empty());
  corner.wires.push_back({{50, 0}, {100, 0}, {}});
  CHECK(schem::junction_points(corner) == std::vector<schem::Point>{{50, 0}});
}

// ---- Placing instances ------------------------------------------------------------------

TEST_CASE("Place two resistors", kPlacing) {
  SchemSession s;
  new_schematic(s);
  CHECK(run(s, {"place", "res.sym", "0", "0"}) == "R1");
  CHECK(run(s, {"place", "res.sym", "0", "100"}) == "R2");
  const auto& instances = active(s).instances;
  REQUIRE(instances.size() == 2);
  for (const auto& instance : instances) {
    CHECK(instance.symbol == "res.sym");
    CHECK(prop(instance.props, "value") == "1k");
    CHECK(prop(instance.props, "footprint") == "1206");
    CHECK(prop(instance.props, "m") == "1");
  }
  CHECK(instances[1].origin == schem::Point{0, 100});
}

TEST_CASE("place takes the rotation, flip and properties given", kPlacing) {
  SchemSession s;
  new_schematic(s);
  CHECK(run(s, {"place", "res.sym", "10", "20", "-rot", "1", "-flip", "1", "-props", "name=R1 value=2k"}) == "R1");
  CHECK(run(s, {"place", "res.sym", "0", "0", "-props", "name=R1 value=3k"}) == "R2");
  CHECK(records(saved(s), "C ") ==
        std::vector<std::string>{"C {res.sym} 10 20 1 1 {name=R1 value=2k}", "C {res.sym} 0 0 0 0 {name=R2 value=3k}"});
  CHECK_FALSE(s.schematic({"place", "res.sym", "0", "0", "-rot", "4"}).ok());
  CHECK_FALSE(s.schematic({"place", "res.sym", "0"}).ok());
}

// ---- Move, copy and stretch ---------------------------------------------------------

TEST_CASE("Stretch keeps connection", kMove) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0"});
  run(s, {"wire", "0", "-30", "0", "-100"});
  run(s, {"select", "instance", "R1"});
  run(s, {"move", "20", "0", "-stretch"});
  const auto& wire = active(s).wires.at(0);
  CHECK(active(s).instances[0].origin == schem::Point{20, 0});
  CHECK(wire.p1 == schem::Point{20, -30});  // the pin P of R1, moved
  CHECK(wire.p2 == schem::Point{0, -100});
}

TEST_CASE("a plain move leaves unselected wires; -insertwires reconnects", kMove) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0"});
  run(s, {"wire", "0", "-30", "0", "-100"});
  run(s, {"select", "instance", "R1"});
  run(s, {"move", "20", "0"});
  CHECK(active(s).wires.at(0).p1 == schem::Point{0, -30});
  run(s, {"undo"});
  run(s, {"move", "20", "10", "-insertwires"});
  REQUIRE(active(s).wires.size() == 3);
  CHECK(active(s).wires[1].p1 == schem::Point{0, -30});
  CHECK(active(s).wires[1].p2 == schem::Point{20, -30});
  CHECK(active(s).wires[2].p2 == schem::Point{20, -20});
  // The move with its inserted wires is one undo step.
  CHECK(run(s, {"undo"}) == "1");
  CHECK(active(s).wires.size() == 1);
}

TEST_CASE("copy duplicates the selection with unique names and selects the copies", kMove) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0"});
  run(s, {"wire", "0", "30", "0", "70"});
  run(s, {"select", "all"});
  run(s, {"copy", "100", "0"});
  REQUIRE(active(s).instances.size() == 2);
  CHECK(prop(active(s).instances[1].props, "name") == "R2");
  CHECK(active(s).instances[1].origin == schem::Point{100, 0});
  CHECK(active(s).wires[1].p1 == schem::Point{100, 30});
  CHECK(run(s, {"selection"}) == "{wire 1} {instance R2}");
}

// ---- Rotate and flip ---------------------------------------------------------------------

TEST_CASE("Rotate resistor in place", kRotate) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "100", "100"});
  run(s, {"select", "instance", "R1"});
  run(s, {"rotate", "-inplace"});
  const auto lines = records(saved(s), "C ");
  REQUIRE(lines.size() == 1);
  CHECK(lines[0].rfind("C {res.sym} 100 100 1 0 {", 0) == 0);
}

TEST_CASE("Rotate wire around pivot", kRotate) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "0", "0", "10", "0"});
  run(s, {"select", "wire", "0"});
  run(s, {"rotate", "0", "0"});
  CHECK(active(s).wires[0].p1 == schem::Point{0, 0});
  CHECK(active(s).wires[0].p2 == schem::Point{0, 10});
}

// Regression (M8 round 6 review): a `rotate` or `flip` with no pivot always
// turned around the selection's centre; with xschem-compat ON a run without
// the editor uses the origin, as xschem's last snapped pointer position.
TEST_CASE("Script rotation without a pivot", kRotate) {
  for (const bool on : {true, false}) {
    INFO("xschem-compat " << on);
    SchemSession s;
    s.project().compat.xschem_compat = on;
    new_schematic(s);
    run(s, {"wire", "100", "100", "120", "100"});
    run(s, {"select", "wire", "0"});
    run(s, {"rotate"});
    CHECK(active(s).wires[0].p1 == (on ? schem::Point{-100, 100} : schem::Point{110, 90}));
    CHECK(active(s).wires[0].p2 == (on ? schem::Point{-100, 120} : schem::Point{110, 110}));
    run(s, {"undo"});
    run(s, {"flip"});
    CHECK(active(s).wires[0].p1 == (on ? schem::Point{-100, 100} : schem::Point{120, 100}));
    CHECK(active(s).wires[0].p2 == (on ? schem::Point{-120, 100} : schem::Point{100, 100}));
  }
}

TEST_CASE("rotation and flip of instances follow the orientation transform", kRotate) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0"});
  run(s, {"select", "instance", "R1"});
  auto& instance = active(s).instances[0];
  run(s, {"rotate", "-inplace"});
  run(s, {"rotate", "-inplace"});
  run(s, {"rotate", "-inplace"});
  run(s, {"rotate", "-inplace"});
  CHECK(active(s).instances[0].rotation == 0);
  run(s, {"rotate", "10", "0"});  // the origin turns around the pivot
  CHECK(active(s).instances[0].origin == schem::Point{10, -10});
  CHECK(active(s).instances[0].rotation == 1);
  run(s, {"flip", "-inplace"});  // R^1 mirrored is R^3 with the flip
  CHECK(active(s).instances[0].rotation == 3);
  CHECK(active(s).instances[0].flip == 1);
  run(s, {"flip", "0", "0"});
  CHECK(active(s).instances[0].origin == schem::Point{-10, -10});
  CHECK(active(s).instances[0].rotation == 1);
  CHECK(active(s).instances[0].flip == 0);
  (void)instance;
}

TEST_CASE("flipv mirrors about the horizontal line through the pivot", kRotate) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "0", "10", "20", "30"});
  run(s, {"text", "5", "5", "0", "0", "0.4", "0.4", "hello"});
  run(s, {"select", "all"});
  run(s, {"flipv", "0", "0"});
  CHECK(active(s).wires[0].p1 == schem::Point{0, -10});
  CHECK(active(s).wires[0].p2 == schem::Point{20, -30});
  CHECK(active(s).texts[0].origin == schem::Point{5, -5});
  CHECK(active(s).texts[0].rotation == 2);
  CHECK(active(s).texts[0].flip == 1);
}

// ---- Labels and ports ---------------------------------------------------------------------

TEST_CASE("labels and ports are system-library instances with the net as lab", kLabels) {
  SchemSession s;
  new_schematic(s);
  CHECK(run(s, {"label", "0", "-30", "IN"}) == "p1");
  CHECK(run(s, {"label", "0", "30", "OUT", "-kind", "opin", "-rot", "2"}) == "p2");
  const auto lines = records(saved(s), "C ");
  REQUIRE(lines.size() == 2);
  CHECK(lines[0] == "C {lab_pin.sym} 0 -30 0 0 {name=p1 sig_type=std_logic lab=IN}");
  CHECK(lines[1] == "C {opin.sym} 0 30 2 0 {name=p2 lab=OUT}");
  CHECK_FALSE(s.schematic({"label", "0", "0", "X", "-kind", "nope"}).ok());
}

// ---- Bus editing ------------------------------------------------------------------------------

TEST_CASE("Bus wire drawn thick", kBus) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "0", "0", "100", "0"});
  run(s, {"label", "0", "0", "D[3:0]"});
  CHECK(prop(active(s).wires[0].props, "bus") == "1");
  // When the net stops being a bus, `bus` is removed.
  run(s, {"setprop", "instance", "p1", "lab", "D"});
  CHECK(prop(active(s).wires[0].props, "bus") == "<none>");
}

TEST_CASE("an inconsistent bus width on a pin is reported once", kBus) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0"});
  run(s, {"wire", "0", "-30", "0", "-100"});
  const auto labelled = s.schematic({"label", "0", "-100", "D[1:0]"});
  REQUIRE(labelled.ok());
  bool warned = false;
  for (const auto& d : labelled.diagnostics) warned = warned || d.code == "SCHEM-EDIT-005";
  CHECK(warned);
  const auto again = s.schematic({"wire", "500", "0", "600", "0"});
  for (const auto& d : again.diagnostics) CHECK(d.code != "SCHEM-EDIT-005");
}

// ---- Property editing ---------------------------------------------------------------------------

TEST_CASE("Change value of two resistors", kProps) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0"});
  run(s, {"place", "res.sym", "0", "100"});
  run(s, {"setprop", "instance", "R1", "value", "10k"});
  run(s, {"setprop", "instance", "R2", "value", "10k"});
  for (const auto& instance : active(s).instances) {
    CHECK(prop(instance.props, "value") == "10k");
    CHECK(prop(instance.props, "footprint") == "1206");
  }
}

TEST_CASE("an edited instance name stays unique; a key without value is removed", kProps) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0"});
  run(s, {"place", "res.sym", "0", "100"});
  run(s, {"setprop", "instance", "R2", "name", "R1"});
  CHECK(prop(active(s).instances[1].props, "name") != "R1");
  run(s, {"setprop", "instance", "R1", "footprint"});
  CHECK(prop(active(s).instances[0].props, "footprint") == "<none>");
  run(s, {"wire", "0", "0", "1", "0"});
  run(s, {"setprop", "wire", "0", "lab", "X"});
  CHECK(prop(active(s).wires[0].props, "lab") == "X");
  run(s, {"setprop", "schematic", "S", ".param a=1"});
  CHECK(active(s).globals.s == ".param a=1");
  CHECK(run(s, {"getprop", "schematic", "S"}) == ".param a=1");
}

TEST_CASE("replace changes only the symbol reference", kProps) {
  SchemSession s;
  new_schematic(s);
  run(s, {"place", "res.sym", "0", "0", "-props", "name=R1 value=5k"});
  run(s, {"replace", "R1", "capa.sym"});
  CHECK(active(s).instances[0].symbol == "capa.sym");
  CHECK(active(s).instances[0].props == "name=R1 value=5k");
}

// ---- Graphics and text --------------------------------------------------------------------------

TEST_CASE("Closed polygon", kGraphics) {
  SchemSession s;
  new_schematic(s);
  CHECK(run(s, {"polygon", "0", "0", "20", "0", "20", "20", "0", "0"}) == "0");
  CHECK(records(saved(s), "P ") == std::vector<std::string>{"P 4 4 0 0 20 0 20 20 0 0 {}"});
}

TEST_CASE("lines, boxes, arcs and texts on a layer", kGraphics) {
  SchemSession s;
  new_schematic(s, "cell.sym");
  run(s, {"new", "symbol", s.folder().str("cell.sym")});
  run(s, {"line", "0", "0", "10", "0", "-layer", "3"});
  run(s, {"box", "-2.5", "-2.5", "2.5", "2.5", "-layer", "5", "-props", "name=A dir=in"});
  run(s, {"arc", "0", "0", "10", "0", "90"});
  run(s, {"text", "0", "-20", "0", "0", "0.3", "0.3", "@name"});
  const std::string text = saved(s, "cell_out.sym");
  CHECK(records(text, "L ") == std::vector<std::string>{"L 3 0 0 10 0 {}"});
  CHECK(records(text, "B ") == std::vector<std::string>{"B 5 -2.5 -2.5 2.5 2.5 {name=A dir=in}"});
  CHECK(records(text, "A ") == std::vector<std::string>{"A 4 0 0 10 0 90 {}"});
  CHECK(records(text, "T ") == std::vector<std::string>{"T {@name} 0 -20 0 0 0.3 0.3 {}"});
  CHECK_FALSE(s.schematic({"line", "0", "0", "1", "1", "-layer", "22"}).ok());
}

// ---- Labels and ports: symbols from schematics -------------------------------------------------

TEST_CASE("Make symbol", kLabels) {
  SchemSession s;
  new_schematic(s, "amp.sch");
  run(s, {"label", "-100", "0", "INP", "-kind", "ipin"});
  run(s, {"label", "-100", "40", "INM", "-kind", "ipin"});
  run(s, {"label", "100", "0", "OUT", "-kind", "opin"});
  run(s, {"save"});
  CHECK(run(s, {"symbol", "make"}) == s.folder().str("amp.sym"));
  const std::string sym = s.folder().read("amp.sym");
  CHECK(records(sym, "B 5 ") == std::vector<std::string>{"B 5 -82.5 -2.5 -77.5 2.5 {name=INP dir=in}",
                                                         "B 5 -82.5 17.5 -77.5 22.5 {name=INM dir=in}",
                                                         "B 5 77.5 -2.5 82.5 2.5 {name=OUT dir=out}"});
  CHECK(sym.find("type=subcircuit") != std::string::npos);
  CHECK(sym.find("format=\"@name @pinlist @symname\"") != std::string::npos);
  CHECK(sym.find("template=\"name=x1\"") != std::string::npos);
  // No overwrite without confirmation.
  const auto again = s.schematic({"symbol", "make"});
  CHECK_FALSE(again.ok());
  CHECK(again.diagnostics.back().code == "SCHEM-EDIT-007");
  CHECK(s.schematic({"symbol", "make", "-force"}).ok());
  // The symbol netlists as `x1 <nets> amp`.
  run(s, {"new", "schematic", s.folder().str("top.sch")});
  run(s, {"place", "amp.sym", "0", "0"});
  run(s, {"label", "-80", "0", "A"});
  run(s, {"label", "-80", "20", "B"});
  run(s, {"label", "80", "0", "C"});
  const std::string netlist = run(s, {"netlist"});
  CHECK(netlist.find("x1 A B C amp") != std::string::npos);
}

TEST_CASE("make schematic from symbol and symbol pins from schematic ports", kLabels) {
  SchemSession s;
  hefesto::test::write_cell(s, "inv");
  run(s, {"open", s.folder().str("inv.sym")});
  // toschematic does not overwrite.
  CHECK(s.schematic({"symbol", "toschematic"}).diagnostics.back().code == "SCHEM-EDIT-007");
  std::filesystem::remove(s.folder().path() / "inv.sch");
  run(s, {"path", "set", hefesto::test::devices_folder(s)});
  run(s, {"symbol", "toschematic"});
  CHECK(records(s.folder().read("inv.sch"), "C ") ==
        std::vector<std::string>{"C {ipin.sym} -20 0 0 0 {name=p1 lab=A}", "C {opin.sym} 20 0 0 0 {name=p2 lab=Y}"});
  // `symbol pins` adds the ports that are not pins yet.
  s.folder().write("inv.sch",
                   "v {xschem version=3.4.8RC file_version=1.3}\n"
                   "C {ipin.sym} 0 0 0 0 {name=p1 lab=A}\nC {iopin.sym} 0 0 0 0 {name=p3 lab=VDD}\n");
  run(s, {"symbol", "pins"});
  const auto& boxes = active(s).boxes;
  REQUIRE(boxes.size() == 3);
  CHECK(boxes[2].props == "name=VDD dir=inout");
}

// ---- Review of PR #101 -------------------------------------------------------------------

// Regression: coordinates went through strtod, so `1e999` put an infinity
// into the document and its saved file. Every number of an editing option
// must be finite.
TEST_CASE("Non-finite coordinate refused", kModel) {
  SchemSession s;
  new_schematic(s);
  run(s, {"wire", "0", "0", "10", "0"});
  run(s, {"select", "wire", "0"});
  const std::vector<std::vector<std::string>> refused{
      {"wire", "0", "0", "1e999", "0"},      {"wire", "0", "-inf", "1", "0"},
      {"place", "res.sym", "nan", "0"},      {"move", "1e999", "0"},
      {"copy", "0", "-1e999"},               {"select", "area", "0", "0", "1e999", "10"},
      {"line", "0", "0", "inf", "0"},        {"arc", "0", "0", "10", "0", "1e999"},
      {"polygon", "0", "0", "inf", "1", "2", "2"}, {"text", "0", "0", "0", "0", "1e999", "1", "t"},
      {"rotate", "inf", "0"}};
  for (const auto& args : refused) {
    INFO(args.front() << " " << args.back());
    CHECK_FALSE(s.schematic(args).ok());
  }
  CHECK(active(s).wires.size() == 1);
  CHECK(active(s).wires[0].p2 == schem::Point{10, 0});
  CHECK(active(s).instances.empty());
  CHECK(active(s).lines.empty());
  CHECK(active(s).texts.empty());
}

// Regression: `text` wrote any integer as its rotation and flip; they are
// 0..3 and 0 or 1, as for `place` and `label`.
TEST_CASE("text takes a rotation of 0 to 3 and a flip of 0 or 1", kGraphics) {
  SchemSession s;
  new_schematic(s);
  CHECK_FALSE(s.schematic({"text", "0", "0", "7", "0", "0.4", "0.4", "t"}).ok());
  CHECK_FALSE(s.schematic({"text", "0", "0", "0", "-3", "0.4", "0.4", "t"}).ok());
  CHECK_FALSE(s.schematic({"text", "0", "0", "-1", "0", "0.4", "0.4", "t"}).ok());
  CHECK_FALSE(s.schematic({"text", "0", "0", "0", "2", "0.4", "0.4", "t"}).ok());
  CHECK(active(s).texts.empty());
  CHECK(run(s, {"text", "0", "0", "3", "1", "0.4", "0.4", "t"}) == "0");
  CHECK(active(s).texts[0].rotation == 3);
  CHECK(active(s).texts[0].flip == 1);
}

// Regression: `symbol make`/`toschematic` built the pin attributes by
// concatenation, so a port name with a blank or a `;` gave
// malformed properties.
TEST_CASE("made symbols and schematics quote port names", kLabels) {
  SchemSession s;
  new_schematic(s, "odd.sch");
  run(s, {"label", "-100", "0", "my net", "-kind", "ipin"});
  run(s, {"label", "100", "0", "a;b", "-kind", "opin"});
  run(s, {"save"});
  run(s, {"symbol", "make"});
  run(s, {"open", s.folder().str("odd.sym")});
  std::vector<std::string> names;
  for (const auto& box : active(s).boxes) {
    if (box.layer == 5) names.push_back(schem::get_token(box.props, "name").value_or("?"));
  }
  CHECK(names == std::vector<std::string>{"my net", "a;b"});
  std::filesystem::remove(s.folder().path() / "odd.sch");
  run(s, {"path", "set", hefesto::test::devices_folder(s)});
  run(s, {"symbol", "toschematic"});
  run(s, {"open", s.folder().str("odd.sch")});
  std::vector<std::string> labs;
  for (const auto& instance : active(s).instances) labs.push_back(schem::get_token(instance.props, "lab").value_or("?"));
  CHECK(labs == std::vector<std::string>{"my net", "a;b"});
}

// Regression: `symbol pins` read the schematic with default options, so its
// read warnings were dropped (and xschem-compat ignored).
TEST_CASE("symbol pins reports the read warnings of the schematic", kLabels) {
  SchemSession s;
  hefesto::test::write_cell(s, "inv");
  s.folder().write("inv.sch",
                   "v {xschem version=3.4.8RC file_version=1.3}\n"
                   "B 99 0 0 1 1 {}\n"
                   "C {iopin.sym} 0 0 0 0 {name=p3 lab=VDD}\n");
  run(s, {"open", s.folder().str("inv.sym")});
  run(s, {"path", "set", hefesto::test::devices_folder(s)});
  const auto result = s.schematic({"symbol", "pins"});
  REQUIRE(result.ok());
  const bool warned = std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const auto& d) {
    return d.text.find("wrong or missing layer") != std::string::npos;
  });
  CHECK(warned);
}

// Regression: deleting erased one object at a time and a stretch move
// searched the moved points linearly per wire end, both quadratic. 100,000
// wires now take well under the bound even in a debug build.
TEST_CASE("deleting and stretching many objects is not quadratic", kMove) {
  constexpr int kWireCount = 100'000;
  SchemSession s;
  new_schematic(s);
  auto& wires = active(s).wires;
  for (int i = 0; i < kWireCount; ++i) {
    const double y = 10.0 * i;
    wires.push_back({{0, y}, {10, y}, {}});
  }
  auto& selection = s.schem().edit.selections[schem::active_document(s.schem())->id];
  for (int i = 0; i < kWireCount; i += 2) selection.insert({schem::ObjectKind::wire, static_cast<std::size_t>(i)});
  const auto start = std::chrono::steady_clock::now();
  run(s, {"move", "-stretch", "0", "5"});
  run(s, {"delete"});
  CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds{20});
  CHECK(active(s).wires.size() == kWireCount / 2);
}
