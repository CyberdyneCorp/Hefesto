// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// SREF and AREF elements read through gds::read (tasks.md 9.5): transforms,
// placement scaling, arrays, instance properties, placement rules and
// magnified instances, in both magic-compat settings where a decision
// applies (Q-MASK-124, 125). Owner: GDSR.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/property.hpp"
#include "hefesto/db/transform.hpp"
#include "hefesto/util/magic_hash_table.hpp"
#include "read_fixture.hpp"

using namespace gds_read_test;

namespace {

constexpr std::array<P, 5> kUnit{{{0, 0}, {0, 10}, {10, 10}, {10, 0}, {0, 0}}};

/// Structure `name` with a 10 x 10 metal1 box.
void child(GdsFile& f, std::string_view name) {
  f.begin(name).boundary(68, 20, {kUnit[0], kUnit[1], kUnit[2], kUnit[3], kUnit[4]}).end();
}

const db::CellUse& only_use(const Session& s, std::string_view parent) {
  const auto u = uses(s.cell(parent));
  REQUIRE(u.size() == 1);
  return *u.front();
}

}  // namespace

TEST_CASE("structure reference transforms", "[cap:gds-read][req:Structure reference transforms]") {
  SECTION("rotated and mirrored SREF") {
    Session s;
    GdsFile f;
    f.library();
    child(f, "A");
    f.begin("top").sref("A", {100, 0}, RefSpec{0x8000, {}, 90.0, {}}).end().end_library();
    s.read(f);
    const db::CellUse& u = only_use(s, "top");
    CHECK(db::apply(u.transform, db::Point{1, 2}) == db::Point{102, 1});
  }
  SECTION("non-Manhattan angle rounded") {
    Session s;
    GdsFile f;
    f.library();
    child(f, "A");
    f.begin("top").sref("A", {0, 0}, RefSpec{0, {}, 44.9, {}}).end().end_library();
    s.read(f);
    CHECK(s.said("Non-Manhattan angle (44) in transform"));
    CHECK(s.said("    Rounding to 0 degrees."));
    CHECK(only_use(s, "top").transform == db::kIdentity);
  }
}

TEST_CASE("instance placement scaling", "[cap:gds-read][req:Instance placement scaling]") {
  constexpr std::string_view kS10 = "style test\nscalefactor 10\nlayer m1 M1\ncalma M1 68 20\n";
  SECTION("a half-grid origin subdivides the grid") {
    Session s(kS10);
    GdsFile f;
    f.library();
    f.begin("A").boundary(68, 20, {{0, 0}, {0, 100}, {100, 100}, {100, 0}, {0, 0}}).end();
    f.begin("top").sref("A", {25, 0}).end().end_library();
    s.read(f);
    CHECK(s.said("Input off lambda grid by 1/2; grid redefined."));
    CHECK(only_use(s, "top").transform.c == 5);
  }
  SECTION("rescale disallowed snaps") {
    Session s(kS10);
    s.options.rescale_allowed = false;
    GdsFile f;
    f.library();
    f.begin("A").boundary(68, 20, {{0, 0}, {0, 100}, {100, 100}, {100, 0}, {0, 0}}).end();
    f.begin("top").sref("A", {25, 0}).end().end_library();
    s.read(f);
    CHECK(only_use(s, "top").transform.c == 2);
  }
}

TEST_CASE("array references", "[cap:gds-read][req:Array references]") {
  const auto read_array = [](int cols, int rows, std::initializer_list<P> points, RefSpec spec = {},
                             bool magic_compat = true) {
    auto s = std::make_unique<Session>(kStyle, magic_compat);
    GdsFile f;
    f.library();
    child(f, "A");
    f.begin("top").aref("A", cols, rows, points, spec).end().end_library();
    s->read(f);
    return s;
  };
  SECTION("simple 3 x 2 array") {
    const auto s = read_array(3, 2, {{0, 0}, {300, 0}, {0, 400}});
    const db::ArrayInfo& a = only_use(*s, "top").array;
    CHECK(a == db::ArrayInfo{0, 2, 0, 1, 100, 200});
  }
  SECTION("rotated array") {
    const auto s = read_array(3, 2, {{0, 0}, {0, 300}, {-400, 0}}, RefSpec{0, {}, 90.0, {}});
    const db::ArrayInfo& a = only_use(*s, "top").array;
    CHECK(a.xsep == 100);
    CHECK(a.ysep == 200);
  }
  SECTION("single row ignores bogus y data") {
    const auto s = read_array(4, 1, {{0, 0}, {400, 999}, {0, 12345}});
    const db::ArrayInfo& a = only_use(*s, "top").array;
    CHECK(a.xsep == 100);
    CHECK(a.ysep == 0);
  }
  SECTION("row spacing not divisible (Q-MASK-124)") {
    for (const bool magic_compat : {true, false}) {
      const auto s = read_array(1, 3, {{0, 0}, {0, 0}, {0, 302}}, {}, magic_compat);
      CHECK(s->said("# rows doesn't divide displacement ref pt"));
      CHECK(only_use(*s, "top").array.ysep == (magic_compat ? 100 : 101));
    }
  }
}

TEST_CASE("instance properties", "[cap:gds-read][req:Instance properties]") {
  SECTION("named instance with array limits") {
    Session s;
    GdsFile f;
    f.library();
    child(f, "A");
    f.begin("top").aref("A", 2, 1, {{0, 0}, {200, 0}, {0, 0}}, RefSpec{0, {}, {}, {{61, "x/y"}, {99, "1_2_0_0"}}});
    f.end().end_library();
    s.read(f);
    const db::CellUse& u = only_use(s, "top");
    CHECK(u.id == "x_y");
    CHECK(u.array.xlo == 1);
    CHECK(u.array.xhi == 2);
    CHECK(u.array.ylo == 0);
    CHECK(u.array.yhi == 0);
    CHECK(s.said("\"/\" character cannot be used in instance name; converting to underscore"));
  }
  SECTION("an unknown property (Q-MASK-125)") {
    for (const bool magic_compat : {true, false}) {
      Session s(kStyle, magic_compat);
      GdsFile f;
      f.library();
      child(f, "A");
      f.begin("top").sref("A", {0, 0}, RefSpec{0, {}, {}, {{5, "other"}, {61, "inst"}}}).end().end_library();
      s.read(f);
      CHECK(only_use(s, "top").id == (magic_compat ? "A_0" : "inst"));
    }
  }
  SECTION("a malformed array property is ignored") {
    Session s;
    GdsFile f;
    f.library();
    child(f, "A");
    f.begin("top").aref("A", 2, 1, {{0, 0}, {200, 0}, {0, 0}}, RefSpec{0, {}, {}, {{99, "1_2"}}}).end().end_library();
    s.read(f);
    CHECK(s.said("Malformed \"array\" property ignored: 1_2"));
    CHECK(only_use(s, "top").array.xhi == 1);
  }
}

TEST_CASE("use placement rules", "[cap:gds-read][req:Use placement rules]") {
  SECTION("an exact duplicate SREF is dropped") {
    Session s;
    GdsFile f;
    f.library();
    child(f, "A");
    f.begin("top").sref("A", {0, 0}).sref("A", {0, 0}).sref("A", {50, 0}).end().end_library();
    s.read(f);
    CHECK(uses(s.cell("top")).size() == 2);
    CHECK(s.said("Warning: cell \"A\" placed on top of itself.  Ignoring the extra one."));
  }
  SECTION("duplicate arrays follow DBCellFindDup's spacing rules") {
    Session s;
    GdsFile f;
    f.library();
    child(f, "A");
    f.begin("top")
        .aref("A", 3, 1, {{0, 0}, {60, 0}, {0, 0}})
        .aref("A", 3, 1, {{0, 0}, {60, 0}, {0, 0}})    // same columns and spacing: dropped
        .aref("A", 3, 1, {{0, 0}, {90, 0}, {0, 0}})    // other spacing: kept
        .sref("A", {0, 0})                            // not an array, other bbox: kept
        .end()
        .end_library();
    s.read(f);
    CHECK(uses(s.cell("top")).size() == 3);
    CHECK(s.count("placed on top of itself") == 1);
  }
  SECTION("regression: many uses in one structure are checked without rescanning the index") {
    // The duplicate check searched the use index, which rescanned every use
    // of the cell per placement: O(n^2) in the uses of one structure (the
    // sky130-sram gds-roundtrip shard ran over 3 hours). 30 000 uses stay
    // well inside the test timeout only when the check is incremental.
    Session s;
    GdsFile f;
    f.library();
    child(f, "A");
    f.begin("top");
    constexpr std::int32_t kUses = 30000;
    for (std::int32_t i = 0; i < kUses; ++i) {
      f.sref("A", {100 * (i % 200), 100 * (i / 200)});
    }
    f.sref("A", {0, 0}).end().end_library();  // one duplicate at the end
    s.read(f);
    CHECK(uses(s.cell("top")).size() == static_cast<std::size_t>(kUses));
    CHECK(s.count("placed on top of itself") == 1);
  }
  SECTION("an ancestor is not used") {
    Session s;
    GdsFile f;
    f.library().begin("A").sref("A", {0, 0}).end().end_library();
    s.read(f);
    CHECK(s.said("Cell A is an ancestor of A"));
    CHECK(s.said("(Use skipped)"));
    CHECK(uses(s.cell("A")).empty());
    const auto it = std::find_if(s.last.diagnostics.begin(), s.last.diagnostics.end(),
                                 [](const diag::Diagnostic& d) { return d.text.ends_with("is an ancestor of A"); });
    REQUIRE(it != s.last.diagnostics.end());
    CHECK(it->no_newline);  // Magic's text has no line end
  }
}

TEST_CASE("regression: a read SREF leaves no default-id entry in the parent's id table",
          "[cap:gds-read][req:Instance properties]") {
  // calmaElementSref makes the use with a NULL id and never links it. The
  // reader linked a default id (`A_0`) per SREF and unlinked it, which left
  // a dead `A_0` entry at its old place in cd_idHash, so a later `A_0` took
  // that place instead of the head of its bucket; it also cost an O(n) scan
  // of the parent's uses per SREF (security.md M5-S3).
  Session s;
  GdsFile f;
  f.library();
  child(f, "A");
  f.begin("top");
  constexpr std::int32_t kNamed = 30;  // below the table's growth at 48 entries
  for (std::int32_t i = 0; i < kNamed; ++i) {
    f.sref("A", {100 * i, 0}, RefSpec{0, {}, {}, {{61, "n" + std::to_string(i)}}});
  }
  f.end().end_library();
  s.read(f);
  db::CellDef* top = s.editor.cells.find("top");
  db::CellDef* a = s.editor.cells.find("A");
  REQUIRE(top != nullptr);
  REQUIRE(a != nullptr);
  db::PlaceOptions o;
  o.transform = db::translation(0, 1000);
  const auto placed = db::place(*top, *a, o);
  REQUIRE(placed.ok());
  REQUIRE(placed.value()->id == "A_0");
  // A new key goes to the head of its bucket (16 buckets, DBCellNewDef).
  const hefesto::util::MagicHashTable<bool> probe{16};
  const std::size_t bucket = probe.bucket_of_key("A_0");
  const std::vector<std::string> ids = top->linked_use_ids();
  const auto first = std::find_if(ids.begin(), ids.end(),
                                  [&](const std::string& id) { return probe.bucket_of_key(id) == bucket; });
  REQUIRE(first != ids.end());
  CHECK(*first == "A_0");
  CHECK(std::count_if(ids.begin(), ids.end(), [&](const std::string& id) { return probe.bucket_of_key(id) == bucket; }) >
        1);
}

TEST_CASE("magnified instances", "[cap:gds-read][req:Magnified instances]") {
  SECTION("1.5x instance") {
    Session s;
    GdsFile f;
    f.library();
    child(f, "C");
    f.begin("top").sref("C", {0, 0}, RefSpec{0, 1.5, {}, {}}).end().end_library();
    s.read(f);
    REQUIRE(s.has_cell("C_mag1p5"));
    CHECK(s.painted("C_mag1p5", "m1") == std::vector<db::Rect>{{0, 0, 15, 15}});
    const db::PropertyValue* source = s.cell("C_mag1p5").properties().find("GDS_MAG_SOURCE");
    REQUIRE(source != nullptr);
    CHECK(std::get<std::string>(*source) == "C");
    CHECK(only_use(s, "top").def == &s.cell("C_mag1p5"));
    CHECK(s.painted("C", "m1") == std::vector<db::Rect>{{0, 0, 10, 10}});
  }
  SECTION("nested magnification cancels") {
    Session s;
    GdsFile f;
    f.library();
    child(f, "Q");
    f.begin("P").sref("Q", {0, 0}, RefSpec{0, 0.5, {}, {}}).end();
    f.begin("top").sref("P", {0, 0}, RefSpec{0, 2.0, {}, {}}).end().end_library();
    s.read(f);
    REQUIRE(s.has_cell("P_mag2"));
    CHECK(only_use(s, "P_mag2").def == &s.cell("Q"));
    CHECK(only_use(s, "P").def == &s.cell("Q_mag0p5"));
  }
  SECTION("an undefined structure is placed without magnification") {
    Session s;
    GdsFile f;
    f.library().begin("top").sref("nowhere", {0, 0}, RefSpec{0, 2.0, {}, {}}).end().end_library();
    s.read(f);
    CHECK(s.said("Cannot magnify cell nowhere, which is not defined in this file;  placing it without magnification."));
    CHECK(only_use(s, "top").def == &s.cell("nowhere"));
  }
  SECTION("an invalid magnification is ignored") {
    Session s;
    GdsFile f;
    f.library();
    child(f, "C");
    f.begin("top").sref("C", {0, 0}, RefSpec{0, -1.0, {}, {}}).end().end_library();
    s.read(f);
    CHECK(s.said("Invalid magnification (-1) in instance of C (ignored)"));
    CHECK(only_use(s, "top").def == &s.cell("C"));
  }
}

TEST_CASE("the rescale setting allows or forbids grid subdivision", "[cap:gds-read][req:Grid subdivision option]") {
  // The read side of `gds rescale`: off-grid reference points subdivide the
  // internal grid only when allowed (the option and Q-MASK-130's warning
  // level are the gds command's, GDSW).
  for (const bool allowed : {true, false}) {
    Session s("style test\nscalefactor 10\nlayer m1 M1\ncalma M1 68 20\n");
    s.options.rescale_allowed = allowed;
    GdsFile f;
    f.library();
    f.begin("A").boundary(68, 20, {{0, 0}, {0, 100}, {100, 100}, {100, 0}, {0, 0}}).end();
    f.begin("top").sref("A", {35, 0}).end().end_library();
    s.read(f);
    CHECK(s.said(allowed ? "grid redefined." : "snapped to grid."));
    CHECK(only_use(s, "top").transform.c == (allowed ? 7 : 3));
  }
}

TEST_CASE("a message after the look-ahead of the element end is placed past that header",
          "[cap:gds-read][req:Read diagnostics]") {
  // Magic's FTELL: the property loop reads the next record header and
  // UNREADRH does not seek back, so the duplicate-placement message names
  // the byte after the ENDEL header (MASK-GR-035).
  Session s;
  GdsFile f;
  f.library();
  child(f, "A");
  f.begin("top").sref("A", {0, 0}).sref("A", {0, 0});
  const std::size_t after_second = f.size();  // the second SREF's ENDEL ends here
  f.end().end_library();
  s.read(f);
  CHECK(s.said("Error while reading cell \"top\" (byte position " + std::to_string(after_second) +
               "): Warning: cell \"A\" placed on top of itself."));
}

TEST_CASE("a malformed array property message has no line end, as Magic's TxError",
          "[cap:gds-read][req:Instance properties]") {
  Session s;
  GdsFile f;
  f.library();
  child(f, "A");
  f.begin("top").aref("A", 2, 1, {{0, 0}, {200, 0}, {0, 0}}, RefSpec{0, {}, {}, {{99, "bad"}}}).end().end_library();
  s.read(f);
  const auto it = std::find_if(s.last.diagnostics.begin(), s.last.diagnostics.end(), [](const diag::Diagnostic& d) {
    return d.text == "Malformed \"array\" property ignored: bad";
  });
  REQUIRE(it != s.last.diagnostics.end());
  CHECK(it->no_newline);
}
