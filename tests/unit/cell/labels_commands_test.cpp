// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cmd-labels-properties commands through the dispatcher: setlabel on the
// selection, port targets, numbering and attributes, property on the
// window's root, findlabel, goto and element, plus the label and property
// changes the db primitives report to db::ChangeRecorder.

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/labels.hpp"
#include "hier_session.hpp"

namespace cell = hefesto::cell;
namespace db = hefesto::db;
using db::Rect;
using Session = cell::test::HierSession;
using cell::test::last_message;
using cell::test::mentions;

// --- setlabel ------------------------------------------------------------------------

TEST_CASE("setlabel queries and changes the selected edit-cell labels",
          "[cap:cmd-labels-properties][req:Setlabel command][golden:CMDEDIT-LBL-03]") {
  Session s;
  db::Label& a = s.label(*s.top, "A", Rect{0, 0, 0, 0});
  a.size = 80;
  db::Label& b = s.label(*s.top, "B", Rect{10, 0, 10, 0});
  b.size = 160;
  s.label(*s.top, "C", Rect{20, 0, 20, 0});  // not selected
  s.select_label(s.top->labels()[0]);
  s.select_label(s.top->labels()[1]);
  CHECK(s.run("setlabel size").value.to_string() == "10 20");
  s.top->flags().modified = false;
  REQUIRE(s.run("setlabel justify sw").ok());
  CHECK(s.top->labels()[0].justification == db::kSouthWest);
  CHECK(s.top->labels()[1].justification == db::kSouthWest);
  CHECK(s.top->labels()[2].justification == db::kCenter);
  CHECK(s.top->flags().modified);
  // The selection's copies follow the change (SelEnumLabelsMirror).
  CHECK(s.editor.selection.def()->labels()[0].justification == db::kSouthWest);
  REQUIRE(s.run("setlabel text Z").ok());
  CHECK(s.run("setlabel text").value.to_string() == "Z Z");
}

TEST_CASE("setlabel without a selection addresses no label", "[cap:cmd-labels-properties][req:Setlabel command]") {
  Session s;
  s.label(*s.top, "A", Rect{0, 0, 0, 0});
  s.box(-5, -5, 5, 5);
  CHECK(s.run("setlabel text").value.to_string().empty());
  REQUIRE(s.run("setlabel sticky 1").ok());
  CHECK_FALSE(s.top->labels()[0].sticky);
}

TEST_CASE("setlabel box coordinates are relative with magic-compat ON and absolute OFF",
          "[cap:cmd-labels-properties][req:Setlabel command]") {
  for (const bool compat : {true, false}) {
    Session s(compat);
    s.label(*s.top, "A", Rect{0, 0, 0, 0});
    s.select_label(s.top->labels()[0]);
    REQUIRE(s.run("setlabel box 10 10 20 20").ok());
    CHECK(s.top->labels()[0].rect == Rect{10, 10, 20, 20});
    CHECK(last_message(s.run("setlabel", {"box", "1 2 3"})) == "Label box requires four coordinates.");
  }
}

TEST_CASE("a selected label of a child cell is not changed", "[cap:cmd-labels-properties][req:Setlabel command]") {
  Session s;
  db::CellDef* inv = s.cell("inv");
  s.label(*inv, "IN", Rect{1, 1, 1, 1});
  s.place(*s.top, *inv, db::translation(100, 0), "x1");
  s.select_label(inv->labels()[0], db::translation(100, 0));
  REQUIRE(s.run("setlabel justify n").ok());
  CHECK(inv->labels()[0].justification == db::kCenter);
}

// --- port --------------------------------------------------------------------------------

TEST_CASE("port make takes the next index, or the index of a same-text port",
          "[cap:cmd-labels-properties][req:Port make, numbering and directions][golden:CMDEDIT-LBL-04]") {
  Session s;
  db::Label& p1 = s.label(*s.top, "A", Rect{0, 0, 0, 0});
  p1.port_directions = db::kPortNorth;
  p1.port_index = 1;
  db::Label& p4 = s.label(*s.top, "B", Rect{10, 0, 10, 0});
  p4.port_directions = db::kPortNorth;
  p4.port_index = 4;
  s.label(*s.top, "C", Rect{50, 50, 50, 50}, db::kNorthEast);
  s.box(45, 45, 55, 55);
  REQUIRE(s.run("port make").ok());
  CHECK(s.top->labels()[2].port_index == 5);
  CHECK(s.top->labels()[2].port_directions == (db::kPortNorth | db::kPortEast));
  s.label(*s.top, "A", Rect{80, 80, 80, 80});
  s.box(75, 75, 85, 85);
  REQUIRE(s.run("port make").ok());
  CHECK(s.top->labels()[3].port_index == 1);  // the same-text port wins
  CHECK(last_message(s.run("port make")) ==
        "Exactly one label may be present under the cursor box.\nUse \"port <name> ...\" to specify a unique port.");
}

TEST_CASE("the first same-text port in label order gives its index",
          "[cap:cmd-labels-properties][req:Port make, numbering and directions]") {
  Session s;
  for (const auto& [x, idx] : {std::pair{0, 3u}, std::pair{10, 6u}}) {
    db::Label& p = s.label(*s.top, "A", Rect{x, 0, x, 0});
    p.port_directions = db::kPortNorth;
    p.port_index = idx;
  }
  s.label(*s.top, "A", Rect{50, 50, 50, 50});
  s.box(45, 45, 55, 55);
  REQUIRE(s.run("port make").ok());
  CHECK(s.top->labels()[2].port_index == 3);
}

TEST_CASE("port make with an index used by another port is refused, and directions can be given",
          "[cap:cmd-labels-properties][req:Port make, numbering and directions]") {
  Session s;
  db::Label& p = s.label(*s.top, "A", Rect{0, 0, 0, 0});
  p.port_directions = db::kPortNorth;
  p.port_index = 4;
  s.label(*s.top, "D", Rect{50, 50, 50, 50});
  s.box(45, 45, 55, 55);
  const auto r = s.run("port make 4");
  CHECK(mentions(r, "Port index 4 is already used by port A."));
  CHECK_FALSE(s.top->labels()[1].is_port());
  REQUIRE(s.run("port make 7 south west").ok());
  CHECK(s.top->labels()[1].port_index == 7);
  CHECK(s.top->labels()[1].port_directions == (db::kPortSouth | db::kPortWest));
  CHECK(s.run("port D connections").value.to_string() == "sw");
  CHECK(mentions(s.run("port D make"), "already a port"));
}

TEST_CASE("port attributes set every port with the same text and index",
          "[cap:cmd-labels-properties][req:Port attributes][golden:CMDEDIT-LBL-04]") {
  Session s;
  for (const db::Coord x : {0, 10}) {
    db::Label& p = s.label(*s.top, "A", Rect{x, 0, x, 0});
    p.port_directions = db::kPortNorth;
    p.port_index = 3;
  }
  s.box(-100, -100, -90, -90);  // CmdPort needs the box even for a named port
  REQUIRE(s.run("port A class input").ok());
  CHECK(s.top->labels()[0].port_class == db::PortClass::input);
  CHECK(s.top->labels()[1].port_class == db::PortClass::input);
  CHECK(s.run("port A class").value.to_string() == "input");
  CHECK(s.run("port 3 name").value.to_string() == "A");
  CHECK(s.run("port A index").value.to_string() == "3");
  REQUIRE(s.run("port A index 9").ok());
  CHECK(s.top->labels()[1].port_index == 9);
  CHECK(last_message(s.run("port Q class")) == "No port found with name Q.");
  CHECK(last_message(s.run("port 77 class")) == "No label found with index 77.");
}

TEST_CASE("port first, last, next, remove, exists and renumber",
          "[cap:cmd-labels-properties][req:Port make, numbering and directions][golden:CMDEDIT-LBL-04]") {
  Session s;
  const std::vector<std::pair<std::string, unsigned>> ports = {{"b", 2}, {"a", 5}, {"c", 9}};
  db::Coord x = 0;
  for (const auto& [text, idx] : ports) {
    db::Label& p = s.label(*s.top, text, Rect{x, 0, x, 0});
    p.port_directions = db::kPortNorth;
    p.port_index = idx;
    x += 10;
  }
  s.box(-100, -100, -90, -90);
  CHECK(s.run("port first").value.to_string() == "2");
  CHECK(s.run("port last").value.to_string() == "9");
  CHECK(s.run("port b next").value.to_string() == "5");
  CHECK(s.run("port a exists").value.to_string() == "1");
  REQUIRE(s.run("port c remove").ok());
  CHECK_FALSE(s.top->labels()[2].is_port());
  REQUIRE(s.run("port renumber").ok());
  CHECK(s.top->labels()[1].port_index == 0);  // a
  CHECK(s.top->labels()[0].port_index == 1);  // b
  s.box(-1, -1, 1, 1);
  CHECK(s.run("port exists").value.to_string() == "1");
  // Regression (CMDEDIT-LBL-04): the word after `next` is ignored. The
  // target under the box is b (index 1), above which there is no port.
  CHECK(s.run("port next").value.to_string() == "-1");
  CHECK(s.run("port next 0").value.to_string() == "-1");
  CHECK(s.run("port 0 next").value.to_string() == "1");  // the target named by index: a
}

TEST_CASE("port changes are recorded as erase and put", "[cap:cmd-labels-properties][req:Port make, numbering and directions]") {
  Session s;
  cell::test::LabelLog log;
  s.editor.cells.set_recorder(&log);
  s.label(*s.top, "C", Rect{50, 50, 50, 50});
  s.box(45, 45, 55, 55);
  REQUIRE(s.run("port make").ok());
  s.editor.cells.set_recorder(nullptr);
  CHECK(log.events == std::vector<std::string>{"erase top C", "put top C"});
}

// --- property ------------------------------------------------------------------------------

TEST_CASE("property targets the window's root cell, not the edit cell",
          "[cap:cmd-labels-properties][req:Property command][golden:CMDEDIT-LBL-05]") {
  Session s;
  db::CellDef* sub = s.cell("sub");
  s.place(*s.top, *sub, db::translation(0, 0), "x1");
  s.editor.edit_cell = sub;
  REQUIRE(s.run("property foo bar").ok());
  CHECK(s.top->properties().find("foo") != nullptr);
  CHECK(sub->properties().find("foo") == nullptr);
  REQUIRE(s.run("property", {"FIXED_BBOX", "0 0 100 200"}).ok());
  CHECK(s.run("property FIXED_BBOX").value.to_string() == "0 0 100 200");
  REQUIRE(s.run("property", {"foo", ""}).ok());
  CHECK(last_message(s.run("property foo")) == "Property name \"foo\" is not defined");
}

TEST_CASE("property changes are reported to the change recorder", "[cap:cmd-labels-properties][req:Property command]") {
  Session s;
  cell::test::LabelLog log;
  s.editor.cells.set_recorder(&log);
  REQUIRE(s.run("property foo bar").ok());
  REQUIRE(s.run("property foo bar").ok());  // unchanged: nothing reported
  REQUIRE(s.run("property", {"foo", ""}).ok());
  s.editor.cells.set_recorder(nullptr);
  CHECK(log.events == std::vector<std::string>{"property top foo new set", "property top foo had removed"});
  s.top->flags().no_edit = true;
  CHECK_FALSE(db::change_property(*s.top, "x", db::PropertyValue{std::string{"1"}}));
}

// --- findlabel / goto -------------------------------------------------------------------------

TEST_CASE("findlabel moves the box to a label, widening a point label",
          "[cap:cmd-labels-properties][req:Findlabel command][golden:CMDEDIT-LBL-06]") {
  Session s;
  s.label(*s.top, "OUT", Rect{40, 10, 40, 10});
  db::CellDef* inv = s.cell("inv");
  s.label(*inv, "A", Rect{1, 1, 3, 3});
  s.label(*inv, "A", Rect{5, 5, 6, 6});
  s.place(*s.top, *inv, db::translation(100, 0), "x1");
  s.box(0, 0, 1, 1);
  REQUIRE(s.run("findlabel OUT").ok());
  CHECK(s.editor.box == Rect{40, 10, 41, 11});
  REQUIRE(s.run("findlabel x1/A 1").ok());
  CHECK(s.editor.box == Rect{105, 5, 106, 6});
  const auto glob = s.run("findlabel -glob *A");
  CHECK(glob.value.to_string() == "x1/A x1/A");
  const auto missing = s.run("findlabel NOPE");
  REQUIRE_FALSE(missing.ok());
  CHECK(last_message(missing) == "Couldn't find label NOPE");
  CHECK(last_message(s.run("findlabel")) == "Usage: findlabel [-glob] label_name");
}

TEST_CASE("goto finds generated node names, legacy names and labels through instance paths",
          "[cap:cmd-labels-properties][req:Goto command][golden:CMDEDIT-LBL-07]") {
  Session s;
  s.paint(*s.top, "metal1", Rect{-10, 0, 10, 20});
  auto r = s.run("goto m1_n5_7#");
  REQUIRE(r.ok());
  CHECK(s.editor.box == Rect{-5, 7, -4, 8});
  CHECK(r.value.to_string() == "metal1");
  db::CellDef* inv = s.cell("inv");
  s.label(*inv, "VDD", Rect{2, 2, 4, 4});
  s.place(*s.top, *inv, db::translation(100, 0), "x1");
  r = s.run("goto x1/VDD");
  REQUIRE(r.ok());
  CHECK(s.editor.box == Rect{102, 2, 104, 4});
  const auto metal1_plane = *s.editor.technology->home_plane(s.type("metal1"));
  r = s.run("goto " + std::to_string(metal1_plane) + "_11_14");  // x = -5, y = 7
  REQUIRE(r.ok());
  CHECK(s.editor.box == Rect{-5, 7, -4, 8});
  CHECK(s.run("goto nosuch -nocomplain").ok());
  CHECK(last_message(s.run("goto nosuch")) == "Couldn't find label nosuch");
}

// --- element -----------------------------------------------------------------------------------

TEST_CASE("element add, names, inbox, configure and delete",
          "[cap:cmd-labels-properties][req:Element command][golden:CMDEDIT-LBL-08]") {
  Session s;
  REQUIRE(s.run("element add rectangle r1 1 0 0 10 10").ok());
  REQUIRE(s.run("element add text t1 1 5 5 hello").ok());
  REQUIRE(s.run("element add line l1 1 0 0 20 0").ok());
  CHECK(s.top->elements().size() == 3);
  CHECK_FALSE(s.top->elements()[0].persistent);
  const std::string names = s.run("element names").value.to_string();
  CHECK(names.find("r1") != std::string::npos);
  CHECK(names.find("t1") != std::string::npos);
  s.box(-1, -1, 11, 11);
  const std::string inbox = s.run("element inbox").value.to_string();
  CHECK(inbox.find("r1") != std::string::npos);
  CHECK(inbox.find("l1") == std::string::npos);
  s.top->flags().modified = false;
  REQUIRE(s.run("element configure r1 flags persistent").ok());
  CHECK(s.top->elements()[0].persistent);
  CHECK(s.top->flags().modified);
  CHECK(s.run("element configure t1 text").value.to_string() == "hello");
  REQUIRE(s.run("element configure t1 text replace bye").ok());
  CHECK(s.top->elements()[1].text == "bye");
  REQUIRE(s.run("element configure l1 flags arrowleft").ok());
  CHECK(s.top->elements()[2].arrow_left);
  REQUIRE(s.run("element configure r1 position 1 2 3 4").ok());
  CHECK(s.run("element configure r1 position").value.to_string() == "1 2 3 4");
  CHECK(last_message(s.run("element configure nosuch text")) == "No such element nosuch");
  REQUIRE(s.run("element delete l1").ok());
  CHECK(s.top->elements().size() == 2);
  REQUIRE(s.run("element add rectangle r1 1 5 5 6 6").ok());  // replaces r1
  CHECK(s.top->elements().size() == 2);
}

// --- recorder -----------------------------------------------------------------------------------

TEST_CASE("the label primitives report put, erase and attribute changes",
          "[cap:cmd-labels-properties][req:Label command]") {
  Session s;
  cell::test::LabelLog log;
  s.editor.cells.set_recorder(&log);
  s.box(5, 5, 5, 5);
  REQUIRE(s.run("label IN").ok());
  REQUIRE(s.run("label IN").ok());  // the same label replaces the first one
  s.select_label(s.top->labels().back());
  REQUIRE(s.run("setlabel rotate 90").ok());
  db::Label& lab = s.top->labels().back();
  db::Label changed = lab;
  changed.sticky = true;
  db::replace_label(*s.top, lab, changed);
  db::remove_label(*s.top, 0);
  s.editor.cells.set_recorder(nullptr);
  CHECK(log.events == std::vector<std::string>{"put top IN", "erase top IN", "put top IN", "erase top IN",
                                               "put top IN", "erase top IN", "put top IN", "erase top IN"});
}

TEST_CASE("port renumbering and label adjustment report each changed label",
          "[cap:layout-labels-ports][req:Port renumbering]") {
  Session s;
  for (const auto& [text, idx] : {std::pair{"b", 7u}, std::pair{"a", 4u}}) {
    db::Label& p = s.label(*s.top, text, Rect{0, 0, 0, 0});
    p.port_directions = db::kPortNorth;
    p.port_index = idx;
  }
  cell::test::LabelLog log;
  s.editor.cells.set_recorder(&log);
  db::renumber_ports(*s.top);
  CHECK(log.events == std::vector<std::string>{"erase top a", "put top a", "erase top b", "put top b"});
  log.events.clear();
  CHECK(db::erase_labels_with_text(*s.top, Rect{0, 0, 0, 0}, "a") == 1);
  CHECK(log.events == std::vector<std::string>{"erase top a"});
  s.editor.cells.set_recorder(nullptr);
}
