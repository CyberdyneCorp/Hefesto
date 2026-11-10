// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// cmd-misc-edit: xor, random, shell, straighten and the informative
// wizard commands.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "edit_session.hpp"
#include "hefesto/db/diag_codes.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/labels.hpp"
#include "hefesto/db/paint.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;
using db::Point;
using db::Rect;
using db::test::EditSession;
using db::test::rects_of;

#define CAP "[cap:cmd-misc-edit]"

namespace {

struct Misc : EditSession {
  const db::test::MtTech& mt;
  db::CellDef& top;
  db::CellDef& b;

  explicit Misc(const db::test::MtTech& m) : EditSession(m), mt(m), top(*editor.edit_cell), b(*cell("b")) {}

  void put(db::CellDef& def, const Rect& r, tech::TileType t) { REQUIRE(db::paint(def, *mt.tech, r, t).ok()); }
};

}  // namespace

TEST_CASE("xor of identical cells is empty", CAP "[req:Xor command]") {
  HEFESTO_REQUIRE_MT(mt);
  Misc s(mt);
  s.put(s.top, Rect{0, 0, 10, 10}, mt.m1);
  s.put(s.b, Rect{0, 0, 10, 10}, mt.m1);
  s.ok("xor b");
  CHECK(rects_of(s.b, mt.metal1, mt.m1).empty());
}

TEST_CASE("xor leaves the symmetric difference", CAP "[req:Xor command]") {
  HEFESTO_REQUIRE_MT(mt);
  Misc s(mt);
  s.put(s.top, Rect{0, 0, 10, 10}, mt.m1);
  s.put(s.b, Rect{5, 0, 15, 10}, mt.m1);
  s.ok("xor b");
  CHECK(rects_of(s.b, mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 5, 10}, {10, 0, 15, 10}});
}

TEST_CASE("xor of different types keeps the source type", CAP "[req:Xor command]") {
  HEFESTO_REQUIRE_MT(mt);
  Misc s(mt);
  s.put(s.top, Rect{0, 0, 10, 10}, mt.ndiff);
  s.put(s.b, Rect{0, 0, 10, 10}, mt.pdiff);
  s.ok("xor b");
  CHECK(rects_of(s.b, mt.active, mt.ndiff) == std::vector<Rect>{{0, 0, 10, 10}});
  CHECK(rects_of(s.b, mt.active, mt.pdiff).empty());
}

TEST_CASE("xor option selection follows magic-compat", CAP "[req:Xor command]") {
  HEFESTO_REQUIRE_MT(mt);
  for (const bool compat : {true, false}) {
    Misc s(mt);
    s.compat(compat);
    db::Label l;
    l.text = "A";
    l.rect = Rect{1, 1, 1, 1};
    l.type = mt.m1;
    s.put(s.top, Rect{0, 0, 10, 10}, mt.m1);
    REQUIRE(db::put_label(s.top, l).ok());
    db::CellDef* vendor = s.cell("vendor");
    vendor->flags().vendor_gds = true;
    s.put(*vendor, Rect{0, 0, 4, 4}, mt.m2);
    db::recompute_bbox(*vendor);
    db::PlaceOptions po;
    po.transform = db::translation(20, 0);
    REQUIRE(db::place(s.top, *vendor, po).ok());
    s.ok("xor -nolabels -novendor b");
    CHECK(s.b.labels().empty());
    // ON: `-novendor` reads as `-nolabels` again, so the vendor cell is
    // flattened; OFF: it stays an instance.
    CHECK(rects_of(s.b, mt.metal2, mt.m2).empty() != compat);
    CHECK(s.b.uses().empty() == compat);
  }
}

TEST_CASE("xor needs an existing destination and a valid form", CAP "[req:Xor command]") {
  HEFESTO_REQUIRE_MT(mt);
  Misc s(mt);
  CHECK(s.run("xor nope").diagnostics.back().text == "nope does not exist");
  CHECK(s.run("xor -labels b").diagnostics.back().text == "usage: xor [-<option>...] destcell");
  CHECK_FALSE(s.run("xor").ok());
}

TEST_CASE("random repeats after the same seed", CAP "[req:Random command]") {
  HEFESTO_REQUIRE_MT(mt);
  Misc s(mt);
  s.ok("random seed 42");
  const std::string first = s.value("random");
  s.ok("random seed 42");
  CHECK(s.value("random") == first);
  CHECK(std::stoll(first) >= 0);
  const auto usage = s.ok("random what");
  CHECK(EditSession::texts(usage).find("usage: random [seed [<value>]]") != std::string::npos);
}

TEST_CASE("shell reports unavailability without process spawning", CAP "[req:Shell command]") {
  HEFESTO_REQUIRE_MT(mt);
  Misc s(mt);  // the portable services have no process runner (as on iPadOS)
  const auto r = s.run("shell ls");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().code == db::codes::kNotAvailable);
  CHECK(r.diagnostics.back().text.find("unavailable on this platform") != std::string::npos);
}

TEST_CASE("straighten checks its arguments, then needs plowing", CAP "[req:Straighten command surface]") {
  HEFESTO_REQUIRE_MT(mt);
  Misc s(mt);
  CHECK(s.run("straighten").diagnostics.back().text == "Usage: straighten manhattan-direction");
  s.ok("box values 0 0 10 10");
  CHECK(s.run("straighten ne").diagnostics.back().text.starts_with("\"ne\" is not a Manhattan direction"));
  const auto r = s.run("straighten n");
  REQUIRE_FALSE(r.ok());
  CHECK(r.diagnostics.back().code == db::codes::kNotAvailable);
}

TEST_CASE("wizard and build-specific commands are reported unavailable", CAP "[req:Wizard and debug commands]") {
  HEFESTO_REQUIRE_MT(mt);
  Misc s(mt);
  s.put(s.top, Rect{0, 0, 10, 10}, mt.m1);
  for (const char* name : {"*tilestats", "*showtech", "*extract", "makesw", "sgraph"}) {
    INFO(name);
    const auto r = s.run(name);
    CHECK_FALSE(r.ok());
    CHECK(r.diagnostics.back().code == db::codes::kNotAvailable);
  }
  CHECK(rects_of(s.top, mt.metal1, mt.m1) == std::vector<Rect>{{0, 0, 10, 10}});
}
