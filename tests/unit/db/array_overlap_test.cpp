// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 The Hefesto Authors
//
// db::array_overlap (DBArrayOverlap), the one helper the use index, the DRC
// engine, mask flattening and the writers' subtree loading share.

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

#include "hefesto/db/array_overlap.hpp"

namespace db = hefesto::db;

namespace {

/// A use of a 10x10 child as an array with `array`, at the identity.
struct ArrayUse {
  db::CellDef child{"child", 1};
  db::CellUse use;

  explicit ArrayUse(const db::ArrayInfo& array, const db::Rect& box = db::Rect{0, 0, 10, 10}) {
    child.set_bboxes(box, box);
    use.def = &child;
    use.array = array;
  }
};

}  // namespace

TEST_CASE("array_overlap gives the elements near an area in the use's index direction",
          "[cap:mask-hierarchical-generation][req:Flattening for interaction processing]") {
  SECTION("a single use is its own element") {
    ArrayUse u{db::ArrayInfo{3, 3, 4, 4, 0, 0}};
    CHECK(db::array_overlap(u.use, db::Rect{-100, -100, 100, 100}) == db::ElementRange{3, 3, 4, 4});
  }
  SECTION("positive pitch") {
    ArrayUse u{db::ArrayInfo{0, 9, 0, 0, 20, 0}};
    // Elements at x = 20i .. 20i+10; the area (35,0)-(65,5) meets i = 2 and 3.
    CHECK(db::array_overlap(u.use, db::Rect{35, 0, 65, 5}) == db::ElementRange{2, 3, 0, 0});
    // Past the last element: an empty range.
    const db::ElementRange none = db::array_overlap(u.use, db::Rect{500, 0, 600, 5});
    CHECK(none.xlo > none.xhi);
  }
  SECTION("negative pitch is flipped about the axis") {
    ArrayUse u{db::ArrayInfo{0, 9, 0, 0, -20, 0}};
    // Elements at x = -20i .. -20i+10.
    CHECK(db::array_overlap(u.use, db::Rect{-45, 0, -15, 5}) == db::ElementRange{1, 2, 0, 0});
  }
  SECTION("indices counting down are mirrored back") {
    ArrayUse u{db::ArrayInfo{9, 0, 0, 0, 20, 0}};
    // Element 9 sits at x = 0, element 8 at x = 20, ...
    CHECK(db::array_overlap(u.use, db::Rect{35, 0, 65, 5}) == db::ElementRange{6, 7, 0, 0});
  }
  SECTION("a zero pitch keeps the whole range of that axis") {
    ArrayUse u{db::ArrayInfo{0, 3, 0, 5, 0, 12}};
    CHECK(db::array_overlap(u.use, db::Rect{0, 25, 5, 30}) == db::ElementRange{0, 3, 2, 2});
  }
  SECTION("extreme coordinates and pitches do not overflow") {
    ArrayUse u{db::ArrayInfo{0, 1000, 0, 0, INT32_MAX, 0}, db::Rect{INT32_MIN / 2, 0, INT32_MAX / 2, 10}};
    const db::ElementRange r = db::array_overlap(u.use, db::Rect{INT32_MIN, 0, INT32_MAX, 5});
    CHECK(r.xlo == 0);
    CHECK(r.xhi == 1);
  }
}
