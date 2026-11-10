// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// The db::ChangeRecorder hook of the db primitives EDIT owns (layout-undo
// "Recorded event kinds"; m2-interactive-editing tasks 1.4 and 3.7): paint
// and erase (Manhattan, split tiles, triangles), split-tile fracture and
// join, use place/delete/id/lock and the modified flag.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "db_fixture.hpp"
#include "hefesto/db/cell.hpp"
#include "hefesto/db/change_recorder.hpp"
#include "hefesto/db/hierarchy.hpp"
#include "hefesto/db/paint.hpp"

namespace db = hefesto::db;
namespace tech = hefesto::tech;

namespace {

struct TileLog final : db::NullChangeRecorder {
  std::vector<db::TileChange> tiles;
  void tile_changed(db::CellDef&, const db::TileChange& change) override { tiles.push_back(change); }
};

// Every call, as text, in order.
struct CallLog final : db::NullChangeRecorder {
  std::vector<std::string> calls;
  void use_placed(db::CellDef& p, const db::CellUse& u) override { calls.push_back("placed " + p.name() + " " + u.id); }
  void use_deleted(db::CellDef& p, const db::CellUse& u) override {
    calls.push_back("deleted " + p.name() + " " + u.id);
  }
  void use_id_cleared(db::CellDef&, const db::CellUse&, std::string_view old_id) override {
    calls.push_back("cleared " + std::string{old_id});
  }
  void use_id_set(db::CellDef&, const db::CellUse&, std::string_view new_id) override {
    calls.push_back("set " + std::string{new_id});
  }
  void use_locked(db::CellDef&, const db::CellUse& u, bool locked) override {
    calls.push_back(std::string{locked ? "locked " : "unlocked "} + u.id);
  }
  void modified_changed(db::CellDef& def, bool was) override {
    calls.push_back("modified " + def.name() + (was ? " was" : " was not"));
  }
};

}  // namespace

TEST_CASE("a library without a recorder records nothing", "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  TileLog log;
  db::CellLibrary recorded;
  recorded.set_recorder(&log);
  db::CellLibrary plain;
  CHECK(plain.recorder() == nullptr);
  db::CellDef* def = plain.create("top", mt.tech->plane_count()).value();
  CHECK(def->recorder() == nullptr);
  REQUIRE(db::paint(*def, *mt.tech, db::Rect{0, 0, 10, 10}, mt.m1).ok());
  CHECK(log.tiles.empty());
}

TEST_CASE("a recorder sees one event per changed tile", "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  TileLog log;
  db::CellLibrary cells;
  db::CellDef* def = cells.create("top", mt.tech->plane_count()).value();
  cells.set_recorder(&log);  // definitions created before the call see it too
  REQUIRE(def->recorder() == &log);

  REQUIRE(db::paint(*def, *mt.tech, db::Rect{0, 0, 10, 10}, mt.m1).ok());
  REQUIRE(log.tiles.size() == 1);
  CHECK(log.tiles[0] == db::TileChange{mt.metal1, db::Rect{0, 0, 10, 10}, tech::kSpace, mt.m1, std::nullopt});

  // Over metal1 (unchanged, no event) and space (one event, clipped).
  log.tiles.clear();
  REQUIRE(db::paint(*def, *mt.tech, db::Rect{5, 0, 15, 10}, mt.m1).ok());
  REQUIRE(log.tiles.size() == 1);
  CHECK(log.tiles[0] == db::TileChange{mt.metal1, db::Rect{10, 0, 15, 10}, tech::kSpace, mt.m1, std::nullopt});

  log.tiles.clear();
  REQUIRE(db::erase(*def, *mt.tech, db::Rect{0, 0, 5, 10}, tech::TypeMask::of(mt.m1)).ok());
  REQUIRE(log.tiles.size() == 1);
  CHECK(log.tiles[0] == db::TileChange{mt.metal1, db::Rect{0, 0, 5, 10}, mt.m1, tech::kSpace, std::nullopt});

  cells.set_recorder(nullptr);
  log.tiles.clear();
  REQUIRE(db::paint(*def, *mt.tech, db::Rect{20, 0, 30, 10}, mt.m1).ok());
  CHECK(log.tiles.empty());
}

TEST_CASE("triangle paint is recorded on the painted side", "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  TileLog log;
  db::CellLibrary cells;
  db::CellDef* def = cells.create("top", mt.tech->plane_count()).value();
  cells.set_recorder(&log);
  REQUIRE(db::paint_triangle(*def, *mt.tech, db::Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true, mt.m1).ok());
  REQUIRE(log.tiles.size() == 1);
  CHECK(log.tiles[0] == db::TileChange{mt.metal1, db::Rect{0, 0, 10, 10}, tech::kSpace, mt.m1,
                                       db::TriangleSide{db::Diagonal::ne_sw, true}});
}

TEST_CASE("painting over a split tile records the changed side", "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  TileLog log;
  db::CellLibrary cells;
  db::CellDef* def = cells.create("top", mt.tech->plane_count()).value();
  REQUIRE(db::paint_triangle(*def, *mt.tech, db::Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true, mt.m1).ok());
  cells.set_recorder(&log);
  REQUIRE(db::erase(*def, *mt.tech, db::Rect{0, 0, 10, 10}, tech::TypeMask::of(mt.m1)).ok());
  REQUIRE(log.tiles.size() == 1);
  CHECK(log.tiles[0] == db::TileChange{mt.metal1, db::Rect{0, 0, 10, 10}, mt.m1, tech::kSpace,
                                       db::TriangleSide{db::Diagonal::ne_sw, true}});
}

TEST_CASE("use place, delete, identifier and lock changes are recorded",
          "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  CallLog log;
  db::CellLibrary cells;
  db::CellDef* top = cells.create("top", mt.tech->plane_count()).value();
  db::CellDef* leaf = cells.create("leaf", mt.tech->plane_count()).value();
  cells.set_recorder(&log);
  db::PlaceOptions po;
  po.id = std::string{"u1"};
  db::CellUse* use = db::place(*top, *leaf, po).value();
  REQUIRE(db::rename_use(*use, "u2").ok());
  db::set_use_locked(*use, true);
  db::set_use_locked(*use, true);  // no change, no event
  REQUIRE(db::remove_use(*use).ok());
  CHECK(log.calls == std::vector<std::string>{"placed top u1", "modified top was not", "cleared u1", "set u2",
                                              "locked u2", "deleted top u2"});
}

TEST_CASE("the modified flag is recorded when it changes", "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  CallLog log;
  db::CellLibrary cells;
  db::CellDef* def = cells.create("top", mt.tech->plane_count()).value();
  cells.set_recorder(&log);
  db::mark_modified(*def);
  db::mark_modified(*def);  // already modified: nothing new
  CHECK(log.calls == std::vector<std::string>{"modified top was not"});
}

namespace {

struct SplitLog final : db::NullChangeRecorder {
  std::vector<db::SplitChange> splits;
  void split_changed(db::CellDef&, const db::SplitChange& change) override { splits.push_back(change); }
};

}  // namespace

TEST_CASE("cutting a split tile records its fracture, and the re-merge its join",
          "[cap:layout-undo][req:Recorded event kinds]") {
  HEFESTO_REQUIRE_MT(mt);
  SplitLog log;
  db::CellLibrary cells;
  db::CellDef* def = cells.create("top", mt.tech->plane_count()).value();
  REQUIRE(db::paint_triangle(*def, *mt.tech, db::Rect{0, 0, 10, 10}, db::Diagonal::ne_sw, true, mt.m1).ok());
  cells.set_recorder(&log);
  // Painting metal1 over the left half cuts the split tile at x = 5
  // (TiNMSplitX): one fracture of the original tile, no join.
  REQUIRE(db::paint(*def, *mt.tech, db::Rect{0, 0, 5, 10}, mt.m1).ok());
  REQUIRE(log.splits.size() == 1);
  CHECK(log.splits[0] == db::SplitChange{db::SplitChange::Kind::fracture, mt.metal1, db::Rect{0, 0, 10, 10},
                                         db::Diagonal::ne_sw, tech::kSpace, mt.m1});

  // Two split tiles along one diagonal with matching neighbors merge into
  // one after the second paint (DBMergeNMTiles0: dbJoinUndo).
  db::CellDef* other = cells.create("other", mt.tech->plane_count()).value();
  REQUIRE(db::paint(*other, *mt.tech, db::Rect{5, 0, 10, 5}, mt.m1).ok());
  REQUIRE(db::paint_triangle(*other, *mt.tech, db::Rect{0, 0, 5, 5}, db::Diagonal::ne_sw, true, mt.m1).ok());
  log.splits.clear();
  REQUIRE(db::paint_triangle(*other, *mt.tech, db::Rect{5, 5, 10, 10}, db::Diagonal::ne_sw, true, mt.m1).ok());
  REQUIRE(log.splits.size() == 1);
  CHECK(log.splits[0] == db::SplitChange{db::SplitChange::Kind::join, mt.metal1, db::Rect{0, 0, 10, 10},
                                         db::Diagonal::ne_sw, tech::kSpace, mt.m1});
  // Without a recorder nothing is collected.
  cells.set_recorder(nullptr);
  REQUIRE(db::paint(*other, *mt.tech, db::Rect{0, 0, 5, 10}, mt.m1).ok());
  CHECK(log.splits.size() == 1);
}
